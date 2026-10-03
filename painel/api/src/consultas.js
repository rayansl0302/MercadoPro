import { z } from 'zod';
import { ErroHttp } from './ciclo.js';

// Consultas do painel (somente leitura, sobre o espelho). A API é o contrato com a tela: se a loja mudar de
// estrutura, só o espelho e estas consultas se ajustam, a tela não percebe.

const dataIso = z.string().regex(/^\d{4}-\d{2}-\d{2}$/);
const filtro = z.object({
  de: dataIso.optional(),
  ate: dataIso.optional(),
  empresa: z.coerce.number().int().positive().optional(),
});

const dinheiro = (v) => (v === null || v === undefined ? 0 : Math.round(Number(v) * 100) / 100);
const inteiro = (v) => (v === null || v === undefined ? 0 : Number(v));

async function hoje(pool, fuso) {
  const r = await pool.query("SELECT (now() AT TIME ZONE $1)::date::text AS d", [fuso || 'America/Sao_Paulo']);
  return r.rows[0].d;
}

async function periodo(pool, req, padraoDias = 30) {
  const f = filtro.parse(req.query ?? {});
  const fim = f.ate ?? (await hoje(pool, req.loja.fuso));
  const r = await pool.query("SELECT ($1::date - make_interval(days => $2))::date::text AS d", [fim, padraoDias]);
  return { de: f.de ?? r.rows[0].d, ate: fim, empresa: f.empresa ?? null };
}

// saldo em aberto de dívidas manuais e de vendas a prazo do PDV (mesma conta do programa da loja)
const SQL_ITENS_A_RECEBER = `
  SELECT d.id_cliente, painel.data_segura(d.data_compra) AS data, 'CADERNETA' AS origem, d.id_empresa,
         d.valor_total - COALESCE((SELECT sum(p.valor) FROM espelho.pagamentos_divida p
                                   WHERE p.loja_id = d.loja_id AND p.id_divida = d.id AND NOT COALESCE(p.cancelado, false)), 0) AS saldo
  FROM espelho.dividas d WHERE d.loja_id = $1 AND d.status = 'ABERTA' AND ($2::int IS NULL OR d.id_empresa = $2)
  UNION ALL
  SELECT v.id_cliente, painel.ts_seguro(v.data_hora)::date, 'PDV', v.id_empresa,
         v.valor_final - COALESCE((SELECT sum(e.total) FROM espelho.entradas_vendas e WHERE e.loja_id = v.loja_id AND e.id_venda = v.id), 0)
  FROM espelho.vendas2 v WHERE v.loja_id = $1 AND v.forma_pagamento = 'Prazo' AND ($2::int IS NULL OR v.id_empresa = $2)`;

export function registrarRotasConsulta(app, { pool, exigirSessao, lojaUnica }) {
  const comLoja = async (req) => {
    await exigirSessao(req);
    await lojaUnica(req);
  };

  app.get('/api/painel/empresas', async (req) => {
    await comLoja(req);
    const r = await pool.query('SELECT id, apelido, cnpj, razao_social, ativa FROM espelho.empresas WHERE loja_id = $1 ORDER BY id', [req.loja.id]);
    return { empresas: r.rows };
  });

  // ------------------------------------------------------------------ visão geral
  app.get('/api/painel/resumo', async (req) => {
    await comLoja(req);
    const L = req.loja.id;
    const { empresa } = filtro.parse(req.query ?? {});
    const dia = await hoje(pool, req.loja.fuso);
    const emp = 'AND ($2::int IS NULL OR id_empresa = $2)';

    const vendas = async (desde) => {
      const r = await pool.query(
        `SELECT count(*)::int AS qtd, coalesce(sum(valor_final), 0) AS total,
                coalesce(sum(valor_final) FILTER (WHERE forma_pagamento = 'Prazo'), 0) AS a_prazo
         FROM espelho.vendas2 WHERE loja_id = $1 ${emp} AND painel.ts_seguro(data_hora)::date >= $3::date AND painel.ts_seguro(data_hora)::date <= $4::date`,
        [L, empresa ?? null, desde, dia],
      );
      const x = r.rows[0];
      return { qtd: x.qtd, total: dinheiro(x.total), aPrazo: dinheiro(x.a_prazo), ticketMedio: x.qtd ? dinheiro(x.total / x.qtd) : 0 };
    };
    const inicioMes = `${dia.slice(0, 8)}01`;

    const recebidoHoje = await pool.query(
      `SELECT
         (SELECT coalesce(sum(valor_final), 0) FROM espelho.vendas2 WHERE loja_id = $1 ${emp} AND forma_pagamento <> 'Prazo' AND painel.ts_seguro(data_hora)::date = $3::date)
       + (SELECT coalesce(sum(e.total), 0) FROM espelho.entradas_vendas e JOIN espelho.vendas2 v ON v.loja_id = e.loja_id AND v.id = e.id_venda
          WHERE e.loja_id = $1 AND ($2::int IS NULL OR v.id_empresa = $2) AND painel.ts_seguro(e.data_hora)::date = $3::date)
       + (SELECT coalesce(sum(p.valor), 0) FROM espelho.pagamentos_divida p JOIN espelho.dividas d ON d.loja_id = p.loja_id AND d.id = p.id_divida
          WHERE p.loja_id = $1 ${'AND ($2::int IS NULL OR d.id_empresa = $2)'} AND NOT COALESCE(p.cancelado, false) AND painel.ts_seguro(p.data_pagamento)::date = $3::date) AS total`,
      [L, empresa ?? null, dia],
    );

    const receber = await pool.query(
      `SELECT coalesce(sum(saldo) FILTER (WHERE saldo > 0.004), 0) AS total, count(DISTINCT id_cliente) FILTER (WHERE saldo > 0.004)::int AS clientes
       FROM (${SQL_ITENS_A_RECEBER}) t`,
      [L, empresa ?? null],
    );

    const pagar = await pool.query(
      `SELECT
         count(*) FILTER (WHERE painel.data_segura(vencimento) < $3::date)::int AS venc_qtd,
         coalesce(sum(valor) FILTER (WHERE painel.data_segura(vencimento) < $3::date), 0) AS venc_valor,
         count(*) FILTER (WHERE painel.data_segura(vencimento) = $3::date)::int AS hoje_qtd,
         coalesce(sum(valor) FILTER (WHERE painel.data_segura(vencimento) = $3::date), 0) AS hoje_valor,
         count(*) FILTER (WHERE painel.data_segura(vencimento) > $3::date AND painel.data_segura(vencimento) <= ($3::date + 7))::int AS sete_qtd,
         coalesce(sum(valor) FILTER (WHERE painel.data_segura(vencimento) > $3::date AND painel.data_segura(vencimento) <= ($3::date + 7)), 0) AS sete_valor,
         count(*)::int AS abertas_qtd, coalesce(sum(valor), 0) AS abertas_valor
       FROM espelho.contas_pagar WHERE loja_id = $1 AND status = 'ABERTA' ${emp}`,
      [L, empresa ?? null, dia],
    );

    const caixas = await pool.query(
      `SELECT c.id, c.terminal, c.aberto_em, o.nome AS operador FROM espelho.caixas c
       LEFT JOIN espelho.operadores o ON o.loja_id = c.loja_id AND o.id = c.id_operador
       WHERE c.loja_id = $1 AND c.status = 'ABERTO' ORDER BY c.id`,
      [L],
    );
    const dif = await pool.query(
      `SELECT coalesce(sum(f.diferenca), 0) AS total FROM espelho.fechamentos_caixa f
       JOIN espelho.caixas c ON c.loja_id = f.loja_id AND c.id = f.id_caixa
       WHERE f.loja_id = $1 AND painel.ts_seguro(c.fechado_em)::date >= $2::date`,
      [L, inicioMes],
    );
    const semEstoque = await pool.query('SELECT count(*)::int AS n FROM espelho.produtos WHERE loja_id = $1 AND coalesce(quantidade, 0) <= 0', [L]);

    const p = pagar.rows[0];
    return {
      hoje: dia,
      vendasHoje: await vendas(dia),
      vendasMes: await vendas(inicioMes),
      recebidoHoje: dinheiro(recebidoHoje.rows[0].total),
      aReceber: { total: dinheiro(receber.rows[0].total), clientes: receber.rows[0].clientes },
      aPagar: {
        vencidas: { qtd: p.venc_qtd, valor: dinheiro(p.venc_valor) },
        hoje: { qtd: p.hoje_qtd, valor: dinheiro(p.hoje_valor) },
        proximos7Dias: { qtd: p.sete_qtd, valor: dinheiro(p.sete_valor) },
        abertas: { qtd: p.abertas_qtd, valor: dinheiro(p.abertas_valor) },
      },
      caixasAbertos: caixas.rows,
      diferencaCaixaMes: dinheiro(dif.rows[0].total),
      produtosSemEstoque: semEstoque.rows[0].n,
    };
  });

  // ------------------------------------------------------------------ vendas
  app.get('/api/painel/vendas', async (req) => {
    await comLoja(req);
    const per = await periodo(pool, req);
    const agrupar = z.enum(['dia', 'mes', 'hora', 'forma', 'operador']).default('dia').parse(req.query?.agrupar);
    const chave = {
      dia: "to_char(painel.ts_seguro(v.data_hora), 'YYYY-MM-DD')",
      mes: "to_char(painel.ts_seguro(v.data_hora), 'YYYY-MM')",
      hora: "lpad(extract(hour FROM painel.ts_seguro(v.data_hora))::int::text, 2, '0')",
      forma: "coalesce(v.forma_pagamento, 'Sem forma')",
      operador: "coalesce(o.nome, CASE WHEN v.id_operador_sessao = 0 THEN 'Gerente (PIN geral)' ELSE 'Sem operador' END)",
    }[agrupar];
    const r = await pool.query(
      `SELECT ${chave} AS chave, count(*)::int AS qtd, coalesce(sum(v.valor_final), 0) AS total,
              coalesce(sum(v.valor_final) FILTER (WHERE v.forma_pagamento = 'Prazo'), 0) AS a_prazo
       FROM espelho.vendas2 v LEFT JOIN espelho.operadores o ON o.loja_id = v.loja_id AND o.id = v.id_operador_sessao
       WHERE v.loja_id = $1 AND ($4::int IS NULL OR v.id_empresa = $4)
         AND painel.ts_seguro(v.data_hora)::date BETWEEN $2::date AND $3::date
       GROUP BY 1 ORDER BY 1`,
      [req.loja.id, per.de, per.ate, per.empresa],
    );
    return { periodo: per, agrupar, serie: r.rows.map((x) => ({ chave: x.chave, qtd: x.qtd, total: dinheiro(x.total), aPrazo: dinheiro(x.a_prazo) })) };
  });

  app.get('/api/painel/vendas/lista', async (req) => {
    await comLoja(req);
    const per = await periodo(pool, req, 7);
    const r = await pool.query(
      `SELECT v.id, v.cliente, v.data_hora, v.forma_pagamento, v.valor_final, v.esta_pago, v.id_empresa,
              coalesce(o.nome, CASE WHEN v.id_operador_sessao = 0 THEN 'Gerente (PIN geral)' ELSE NULL END) AS operador, v.id_caixa
       FROM espelho.vendas2 v LEFT JOIN espelho.operadores o ON o.loja_id = v.loja_id AND o.id = v.id_operador_sessao
       WHERE v.loja_id = $1 AND ($4::int IS NULL OR v.id_empresa = $4) AND painel.ts_seguro(v.data_hora)::date BETWEEN $2::date AND $3::date
       ORDER BY painel.ts_seguro(v.data_hora) DESC, v.id DESC LIMIT 300`,
      [req.loja.id, per.de, per.ate, per.empresa],
    );
    return { periodo: per, vendas: r.rows.map((x) => ({ ...x, valor_final: dinheiro(x.valor_final) })) };
  });

  // ------------------------------------------------------------------ operadores
  app.get('/api/painel/operadores', async (req) => {
    await comLoja(req);
    const per = await periodo(pool, req);
    const L = req.loja.id;
    const vendas = await pool.query(
      `SELECT v.id_operador_sessao AS id, count(*)::int AS qtd, coalesce(sum(v.valor_final), 0) AS total
       FROM espelho.vendas2 v WHERE v.loja_id = $1 AND ($4::int IS NULL OR v.id_empresa = $4)
         AND painel.ts_seguro(v.data_hora)::date BETWEEN $2::date AND $3::date GROUP BY 1`,
      [L, per.de, per.ate, per.empresa],
    );
    const cancel = await pool.query(
      `SELECT m.id_operador_sessao AS id, count(*)::int AS qtd, coalesce(sum(m.valor), 0) AS total
       FROM espelho.movimentacoes_caixa m WHERE m.loja_id = $1 AND m.tipo = 'CANCELAMENTO'
         AND painel.ts_seguro(m.data_hora)::date BETWEEN $2::date AND $3::date GROUP BY 1`,
      [L, per.de, per.ate],
    );
    const sessoes = await pool.query(
      `SELECT s.id_operador AS id, count(*)::int AS entradas,
              coalesce(sum(extract(epoch FROM (coalesce(painel.ts_seguro(s.saida_em), painel.ts_seguro(s.entrada_em)) - painel.ts_seguro(s.entrada_em)))), 0) AS segundos
       FROM espelho.sessoes_operador s WHERE s.loja_id = $1 AND painel.ts_seguro(s.entrada_em)::date BETWEEN $2::date AND $3::date GROUP BY 1`,
      [L, per.de, per.ate],
    );
    const difs = await pool.query(
      `SELECT c.id_operador AS id, coalesce(sum(f.diferenca), 0) AS total, count(DISTINCT c.id)::int AS caixas
       FROM espelho.caixas c JOIN espelho.fechamentos_caixa f ON f.loja_id = c.loja_id AND f.id_caixa = c.id
       WHERE c.loja_id = $1 AND painel.ts_seguro(c.fechado_em)::date BETWEEN $2::date AND $3::date GROUP BY 1`,
      [L, per.de, per.ate],
    );
    const ops = await pool.query('SELECT id, nome, ativo, gerente FROM espelho.operadores WHERE loja_id = $1', [L]);

    const mapa = new Map();
    const pega = (id) => {
      const k = id === null ? -1 : Number(id);
      if (!mapa.has(k)) mapa.set(k, { id: k, nome: k === 0 ? 'Gerente (PIN geral)' : k === -1 ? 'Sem operador' : `Operador ${k}`, gerente: false, ativo: true, vendas: 0, total: 0, ticketMedio: 0, cancelamentos: 0, valorCancelado: 0, entradas: 0, horasLogado: 0, diferencaCaixas: 0, caixasFechados: 0 });
      return mapa.get(k);
    };
    for (const o of ops.rows) Object.assign(pega(o.id), { nome: o.nome, ativo: o.ativo, gerente: o.gerente });
    for (const x of vendas.rows) Object.assign(pega(x.id), { vendas: x.qtd, total: dinheiro(x.total), ticketMedio: x.qtd ? dinheiro(x.total / x.qtd) : 0 });
    for (const x of cancel.rows) Object.assign(pega(x.id), { cancelamentos: x.qtd, valorCancelado: dinheiro(x.total) });
    for (const x of sessoes.rows) Object.assign(pega(x.id), { entradas: x.entradas, horasLogado: Math.round((Number(x.segundos) / 3600) * 10) / 10 });
    for (const x of difs.rows) Object.assign(pega(x.id), { diferencaCaixas: dinheiro(x.total), caixasFechados: x.caixas });
    const lista = [...mapa.values()].filter((o) => o.vendas || o.cancelamentos || o.entradas || o.caixasFechados || o.ativo).sort((a, b) => b.total - a.total);
    return { periodo: per, operadores: lista };
  });

  // ------------------------------------------------------------------ caixas
  app.get('/api/painel/caixas', async (req) => {
    await comLoja(req);
    const per = await periodo(pool, req, 14);
    const r = await pool.query(
      `SELECT c.id, c.terminal, c.status, c.aberto_em, c.fechado_em, c.troco_inicial, o.nome AS operador,
              (SELECT coalesce(sum(v.valor_final), 0) FROM espelho.vendas2 v WHERE v.loja_id = c.loja_id AND v.id_caixa = c.id AND v.forma_pagamento <> 'Prazo') AS vendas,
              (SELECT count(*)::int FROM espelho.vendas2 v WHERE v.loja_id = c.loja_id AND v.id_caixa = c.id) AS qtd_vendas,
              (SELECT coalesce(sum(m.valor), 0) FROM espelho.movimentacoes_caixa m WHERE m.loja_id = c.loja_id AND m.id_caixa = c.id AND m.tipo = 'SANGRIA') AS sangrias,
              (SELECT coalesce(sum(m.valor), 0) FROM espelho.movimentacoes_caixa m WHERE m.loja_id = c.loja_id AND m.id_caixa = c.id AND m.tipo = 'SUPRIMENTO') AS suprimentos,
              (SELECT coalesce(sum(f.valor_esperado), 0) FROM espelho.fechamentos_caixa f WHERE f.loja_id = c.loja_id AND f.id_caixa = c.id) AS esperado,
              (SELECT coalesce(sum(f.valor_informado), 0) FROM espelho.fechamentos_caixa f WHERE f.loja_id = c.loja_id AND f.id_caixa = c.id) AS contado,
              (SELECT coalesce(sum(f.diferenca), 0) FROM espelho.fechamentos_caixa f WHERE f.loja_id = c.loja_id AND f.id_caixa = c.id) AS diferenca,
              round(extract(epoch FROM (coalesce(painel.ts_seguro(c.fechado_em), now() AT TIME ZONE $4) - painel.ts_seguro(c.aberto_em))) / 60)::int AS minutos
       FROM espelho.caixas c LEFT JOIN espelho.operadores o ON o.loja_id = c.loja_id AND o.id = c.id_operador
       WHERE c.loja_id = $1 AND painel.ts_seguro(c.aberto_em)::date BETWEEN $2::date AND $3::date
       ORDER BY painel.ts_seguro(c.aberto_em) DESC, c.id DESC LIMIT 300`,
      [req.loja.id, per.de, per.ate, req.loja.fuso || 'America/Sao_Paulo'],
    );
    const num = (x) => dinheiro(x);
    return {
      periodo: per,
      caixas: r.rows.map((x) => ({ ...x, troco_inicial: num(x.troco_inicial), vendas: num(x.vendas), sangrias: num(x.sangrias), suprimentos: num(x.suprimentos), esperado: num(x.esperado), contado: num(x.contado), diferenca: num(x.diferenca) })),
    };
  });

  // ------------------------------------------------------------------ devedores (fiado)
  app.get('/api/painel/devedores', async (req) => {
    await comLoja(req);
    const { empresa } = filtro.parse(req.query ?? {});
    const dia = await hoje(pool, req.loja.fuso);
    const r = await pool.query(
      `SELECT t.id_cliente, c.nome, c.telefone, c.whatsapp, c.ativo, c.limite_credito,
              sum(t.saldo) AS total, min(t.data) AS mais_antiga, max(t.data) AS ultima_compra,
              sum(t.saldo) FILTER (WHERE $3::date - t.data <= 30) AS ate30,
              sum(t.saldo) FILTER (WHERE $3::date - t.data BETWEEN 31 AND 60) AS de31a60,
              sum(t.saldo) FILTER (WHERE $3::date - t.data BETWEEN 61 AND 90) AS de61a90,
              sum(t.saldo) FILTER (WHERE $3::date - t.data > 90) AS mais90
       FROM (SELECT * FROM (${SQL_ITENS_A_RECEBER}) i WHERE i.saldo > 0.004) t
       LEFT JOIN espelho.clientes c ON c.loja_id = $1 AND c.id = t.id_cliente
       GROUP BY t.id_cliente, c.nome, c.telefone, c.whatsapp, c.ativo, c.limite_credito
       ORDER BY total DESC`,
      [req.loja.id, empresa ?? null, dia],
    );
    const devedores = r.rows.map((x) => ({
      idCliente: x.id_cliente, nome: x.nome ?? 'Cliente removido', telefone: x.whatsapp || x.telefone || null, ativo: x.ativo ?? true,
      total: dinheiro(x.total), maisAntiga: x.mais_antiga, ultimaCompra: x.ultima_compra,
      limite: x.limite_credito === null ? null : dinheiro(x.limite_credito),
      acimaDoLimite: x.limite_credito !== null && Number(x.total) > Number(x.limite_credito) + 0.004,
      idadeDias: x.mais_antiga ? Math.round((new Date(dia) - new Date(x.mais_antiga)) / 86400000) : 0,
      faixas: { ate30: dinheiro(x.ate30), de31a60: dinheiro(x.de31a60), de61a90: dinheiro(x.de61a90), mais90: dinheiro(x.mais90) },
    }));
    const soma = (f) => dinheiro(devedores.reduce((s, d) => s + d.faixas[f], 0));
    return {
      total: dinheiro(devedores.reduce((s, d) => s + d.total, 0)),
      faixas: { ate30: soma('ate30'), de31a60: soma('de31a60'), de61a90: soma('de61a90'), mais90: soma('mais90') },
      devedores,
    };
  });

  // ------------------------------------------------------------------ contas a pagar
  app.get('/api/painel/contas-pagar', async (req) => {
    await comLoja(req);
    const q = z.object({
      status: z.enum(['ABERTA', 'PAGA', 'CANCELADA', 'VENCIDA', 'TODAS']).default('ABERTA'),
      empresa: z.coerce.number().int().positive().optional(),
      de: dataIso.optional(),
      ate: dataIso.optional(),
    }).parse(req.query ?? {});
    const dia = await hoje(pool, req.loja.fuso);
    const r = await pool.query(
      `SELECT id, id_empresa, descricao, fornecedor, categoria, documento, valor, vencimento, status, valor_pago,
              forma_pagamento, pago_em, parcela, total_parcelas, observacao, motivo_cancelamento
       FROM espelho.contas_pagar
       WHERE loja_id = $1 AND ($2::int IS NULL OR id_empresa = $2)
         AND ($3 = 'TODAS' OR ($3 = 'VENCIDA' AND status = 'ABERTA' AND painel.data_segura(vencimento) < $6::date) OR ($3 <> 'VENCIDA' AND status = $3))
         AND ($4::date IS NULL OR painel.data_segura(vencimento) >= $4::date) AND ($5::date IS NULL OR painel.data_segura(vencimento) <= $5::date)
       ORDER BY painel.data_segura(vencimento), id LIMIT 500`,
      [req.loja.id, q.empresa ?? null, q.status, q.de ?? null, q.ate ?? null, dia],
    );
    const porCategoria = await pool.query(
      `SELECT coalesce(nullif(categoria, ''), 'Sem categoria') AS categoria, count(*)::int AS qtd, coalesce(sum(valor), 0) AS total
       FROM espelho.contas_pagar WHERE loja_id = $1 AND status = 'ABERTA' AND ($2::int IS NULL OR id_empresa = $2) GROUP BY 1 ORDER BY 3 DESC`,
      [req.loja.id, q.empresa ?? null],
    );
    return {
      hoje: dia,
      contas: r.rows.map((x) => ({ ...x, valor: dinheiro(x.valor), valor_pago: x.valor_pago === null ? null : dinheiro(x.valor_pago) })),
      porCategoria: porCategoria.rows.map((x) => ({ categoria: x.categoria, qtd: x.qtd, total: dinheiro(x.total) })),
    };
  });

  // ------------------------------------------------------------------ fluxo (a pagar por dia x a receber)
  app.get('/api/painel/fluxo', async (req) => {
    await comLoja(req);
    const { empresa } = filtro.parse(req.query ?? {});
    const dias = z.coerce.number().int().min(7).max(120).default(30).parse(req.query?.dias);
    const dia = await hoje(pool, req.loja.fuso);
    const pagar = await pool.query(
      `SELECT painel.data_segura(vencimento)::text AS dia, coalesce(sum(valor), 0) AS total, count(*)::int AS qtd
       FROM espelho.contas_pagar WHERE loja_id = $1 AND status = 'ABERTA' AND ($2::int IS NULL OR id_empresa = $2)
         AND painel.data_segura(vencimento) <= ($3::date + $4::int) GROUP BY 1 ORDER BY 1`,
      [req.loja.id, empresa ?? null, dia, dias],
    );
    const receber = await pool.query(`SELECT coalesce(sum(saldo) FILTER (WHERE saldo > 0.004), 0) AS total FROM (${SQL_ITENS_A_RECEBER}) t`, [req.loja.id, empresa ?? null]);
    const recebidoMedio = await pool.query(
      `SELECT coalesce(sum(valor_final), 0) / 30.0 AS media FROM espelho.vendas2
       WHERE loja_id = $1 AND ($2::int IS NULL OR id_empresa = $2) AND forma_pagamento <> 'Prazo'
         AND painel.ts_seguro(data_hora)::date > ($3::date - 30) AND painel.ts_seguro(data_hora)::date <= $3::date`,
      [req.loja.id, empresa ?? null, dia],
    );
    const porDia = pagar.rows.map((x) => ({ dia: x.dia, total: dinheiro(x.total), qtd: x.qtd }));
    return {
      hoje: dia,
      dias,
      aPagarPorDia: porDia,
      aPagarNoPeriodo: dinheiro(porDia.reduce((s, x) => s + x.total, 0)),
      aReceberTotal: dinheiro(receber.rows[0].total),
      vendaMediaDiaria: dinheiro(recebidoMedio.rows[0].media),
    };
  });

  // ------------------------------------------------------------------ estoque
  app.get('/api/painel/estoque', async (req) => {
    await comLoja(req);
    const limite = z.coerce.number().min(0).max(100000).default(5).parse(req.query?.limite);
    const dia = await hoje(pool, req.loja.fuso);
    const L = req.loja.id;
    const baixo = await pool.query(
      `SELECT id, descricao, quantidade, un_comercial, preco, preco_fornecedor FROM espelho.produtos
       WHERE loja_id = $1 AND coalesce(quantidade, 0) <= $2 ORDER BY quantidade NULLS FIRST, descricao LIMIT 100`,
      [L, limite],
    );
    const parados = await pool.query(
      `SELECT p.id, p.descricao, p.quantidade, p.preco FROM espelho.produtos p
       WHERE p.loja_id = $1 AND p.quantidade > 0 AND NOT EXISTS (
         SELECT 1 FROM espelho.produtos_vendidos pv JOIN espelho.vendas2 v ON v.loja_id = pv.loja_id AND v.id = pv.id_venda
         WHERE pv.loja_id = p.loja_id AND pv.id_produto = p.id AND painel.ts_seguro(v.data_hora)::date > ($2::date - 60))
       ORDER BY p.quantidade * p.preco DESC NULLS LAST LIMIT 50`,
      [L, dia],
    );
    const maisVendidos = await pool.query(
      `SELECT p.id, p.descricao, sum(pv.quantidade) AS qtd, sum(pv.quantidade * pv.preco_vendido) AS total
       FROM espelho.produtos_vendidos pv JOIN espelho.vendas2 v ON v.loja_id = pv.loja_id AND v.id = pv.id_venda
       JOIN espelho.produtos p ON p.loja_id = pv.loja_id AND p.id = pv.id_produto
       WHERE pv.loja_id = $1 AND painel.ts_seguro(v.data_hora)::date > ($2::date - 30)
       GROUP BY p.id, p.descricao ORDER BY total DESC LIMIT 20`,
      [L, dia],
    );
    const valores = await pool.query(
      `SELECT coalesce(sum(quantidade * preco) FILTER (WHERE quantidade > 0), 0) AS a_preco_de_venda,
              coalesce(sum(quantidade * preco_fornecedor) FILTER (WHERE quantidade > 0 AND preco_fornecedor > 0), 0) AS a_custo,
              count(*)::int AS produtos FROM espelho.produtos WHERE loja_id = $1`,
      [L],
    );
    const v = valores.rows[0];
    return {
      limite,
      resumo: { produtos: v.produtos, valorAPrecoDeVenda: dinheiro(v.a_preco_de_venda), valorACusto: dinheiro(v.a_custo) },
      baixoEstoque: baixo.rows.map((x) => ({ ...x, quantidade: inteiro(x.quantidade), preco: dinheiro(x.preco), preco_fornecedor: x.preco_fornecedor === null ? null : dinheiro(x.preco_fornecedor) })),
      parados60Dias: parados.rows.map((x) => ({ ...x, quantidade: inteiro(x.quantidade), preco: dinheiro(x.preco) })),
      maisVendidos30Dias: maisVendidos.rows.map((x) => ({ id: x.id, descricao: x.descricao, quantidade: inteiro(x.qtd), total: dinheiro(x.total) })),
    };
  });

  // busca por nome (qualquer parte, sem acento/caixa) ou código de barras; com ?id= devolve os detalhes do produto
  app.get('/api/painel/produtos', async (req) => {
    await comLoja(req);
    const q = z.object({ q: z.string().trim().max(100).optional(), id: z.coerce.number().int().positive().optional() }).parse(req.query ?? {});
    const L = req.loja.id;
    const dia = await hoje(pool, req.loja.fuso);
    if (q.id) {
      const p = await pool.query('SELECT * FROM espelho.produtos WHERE loja_id = $1 AND id = $2', [L, q.id]);
      if (!p.rowCount) throw new ErroHttp(404, 'Produto não encontrado.');
      const x = p.rows[0];
      const v = await pool.query(
        `SELECT coalesce(sum(pv.quantidade) FILTER (WHERE painel.ts_seguro(v.data_hora)::date > ($3::date - 30)), 0) AS qtd30,
                coalesce(sum(pv.quantidade) FILTER (WHERE painel.ts_seguro(v.data_hora)::date > ($3::date - 90)), 0) AS qtd90,
                coalesce(sum(pv.quantidade * pv.preco_vendido) FILTER (WHERE painel.ts_seguro(v.data_hora)::date > ($3::date - 30)), 0) AS total30,
                max(painel.ts_seguro(v.data_hora))::text AS ultima_venda
         FROM espelho.produtos_vendidos pv JOIN espelho.vendas2 v ON v.loja_id = pv.loja_id AND v.id = pv.id_venda
         WHERE pv.loja_id = $1 AND pv.id_produto = $2`,
        [L, q.id, dia],
      );
      const preco = dinheiro(x.preco);
      const custo = x.preco_fornecedor === null ? null : dinheiro(x.preco_fornecedor);
      return {
        produto: {
          id: x.id, descricao: x.descricao, codigoBarras: x.codigo_barras, unidade: x.un_comercial, local: x.local, fiscal: x.nf,
          quantidade: inteiro(x.quantidade), preco, custo, margemPercentual: custo && custo > 0 ? Math.round(((preco - custo) / custo) * 1000) / 10 : null,
          valorEmEstoque: dinheiro(inteiro(x.quantidade) * preco), adicionadoEm: x.adicionado_em, atualizadoEm: x.atualizado_em,
        },
        vendas: { qtd30: inteiro(v.rows[0].qtd30), qtd90: inteiro(v.rows[0].qtd90), total30: dinheiro(v.rows[0].total30), ultimaVenda: v.rows[0].ultima_venda },
      };
    }
    const termo = q.q ?? '';
    if (termo.length < 2) return { produtos: [] };
    const r = await pool.query(
      `SELECT id, descricao, codigo_barras, quantidade, preco, un_comercial FROM espelho.produtos
       WHERE loja_id = $1 AND (codigo_barras = $2 OR painel.sem_acento(descricao) ILIKE '%' || painel.sem_acento($2) || '%' OR codigo_barras ILIKE $2 || '%')
       ORDER BY (codigo_barras = $2) DESC, descricao LIMIT 30`,
      [L, termo],
    );
    return { produtos: r.rows.map((x) => ({ ...x, quantidade: inteiro(x.quantidade), preco: dinheiro(x.preco) })) };
  });

  // ------------------------------------------------------------------ fiscal
  app.get('/api/painel/fiscal', async (req) => {
    await comLoja(req);
    const per = await periodo(pool, req);
    const r = await pool.query(
      `SELECT modelo, coalesce(finalidade, '—') AS finalidade, coalesce(cstat, '—') AS cstat, tp_amb, count(*)::int AS qtd, coalesce(sum(valor_total), 0) AS total
       FROM espelho.notas_fiscais WHERE loja_id = $1 AND painel.ts_seguro(dhemi)::date BETWEEN $2::date AND $3::date
       GROUP BY 1, 2, 3, 4 ORDER BY 1, 2, 3`,
      [req.loja.id, per.de, per.ate],
    );
    const cont = await pool.query("SELECT count(*)::int AS n FROM espelho.notas_fiscais WHERE loja_id = $1 AND cstat = 'CONTINGENCIA'", [req.loja.id]);
    return { periodo: per, emContingencia: cont.rows[0].n, grupos: r.rows.map((x) => ({ ...x, total: dinheiro(x.total) })) };
  });

  // ------------------------------------------------------------------ segurança (o que merece olhar)
  app.get('/api/painel/seguranca', async (req) => {
    await comLoja(req);
    const per = await periodo(pool, req);
    const eventos = await pool.query(
      `SELECT id, data_hora, nome_operador, terminal, acao, detalhe FROM espelho.auditoria_acesso
       WHERE loja_id = $1 AND painel.ts_seguro(data_hora)::date BETWEEN $2::date AND $3::date
       ORDER BY painel.ts_seguro(data_hora) DESC, id DESC LIMIT 200`,
      [req.loja.id, per.de, per.ate],
    );
    const resumo = await pool.query(
      `SELECT acao, count(*)::int AS qtd FROM espelho.auditoria_acesso
       WHERE loja_id = $1 AND painel.ts_seguro(data_hora)::date BETWEEN $2::date AND $3::date GROUP BY 1 ORDER BY 2 DESC`,
      [req.loja.id, per.de, per.ate],
    );
    const cancel = await pool.query(
      `SELECT count(*)::int AS qtd, coalesce(sum(valor), 0) AS total FROM espelho.movimentacoes_caixa
       WHERE loja_id = $1 AND tipo = 'CANCELAMENTO' AND painel.ts_seguro(data_hora)::date BETWEEN $2::date AND $3::date`,
      [req.loja.id, per.de, per.ate],
    );
    return { periodo: per, resumo: resumo.rows, cancelamentos: { qtd: cancel.rows[0].qtd, total: dinheiro(cancel.rows[0].total) }, eventos: eventos.rows };
  });

  // ------------------------------------------------------------------ histórico de sincronização
  app.get('/api/painel/sincronizacao', async (req) => {
    await comLoja(req);
    const r = await pool.query(
      `SELECT id, status, iniciado_em, concluido_em, terminal, linhas, erro FROM painel.ciclos WHERE loja_id = $1 ORDER BY iniciado_em DESC LIMIT 30`,
      [req.loja.id],
    );
    return { ciclos: r.rows };
  });
}
