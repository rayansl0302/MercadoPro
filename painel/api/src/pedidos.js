import { z } from 'zod';
import { transacao } from './db.js';
import { ErroHttp } from './ciclo.js';

// Pedidos do painel para a loja (fila). O painel nunca grava no banco da loja: cria um pedido; a loja o busca,
// aplica pelas regras do sistema e devolve o resultado.

export const FORMAS_PAGAMENTO = ['Dinheiro', 'Pix', 'Transferência', 'Boleto', 'Cartão de débito', 'Cartão de crédito', 'Cheque', 'Outro'];

const data = z.string().regex(/^\d{4}-\d{2}-\d{2}$/, 'Data inválida (use AAAA-MM-DD)').refine((d) => !Number.isNaN(Date.parse(d)), 'Data inválida');
const dinheiro = z.union([z.number(), z.string()]).transform((v) => Number(String(v).replace(',', '.'))).pipe(z.number().positive().max(99999999.99));

const esquemas = {
  CONTA_PAGAR_BAIXAR: z.object({
    contaId: z.number().int().positive(),
    valorPago: dinheiro,
    forma: z.enum(FORMAS_PAGAMENTO),
    data,
  }),
  CONTA_PAGAR_LANCAR: z.object({
    descricao: z.string().trim().min(2).max(200),
    fornecedor: z.string().trim().max(200).optional().default(''),
    categoria: z.string().trim().max(100).optional().default(''),
    documento: z.string().trim().max(100).optional().default(''),
    valor: dinheiro,
    vencimento: data,
    parcelas: z.number().int().min(1).max(120).default(1),
    periodicidade: z.enum(['MENSAL', 'QUINZENAL', 'SEMANAL']).default('MENSAL'),
    observacao: z.string().trim().max(500).optional().default(''),
    idEmpresa: z.number().int().positive(),
  }),
};

export const tiposDePedido = Object.keys(esquemas);

export function validarPayload(tipo, payload) {
  const esquema = esquemas[tipo];
  if (!esquema) throw new ErroHttp(422, `Tipo de pedido desconhecido: ${tipo}`);
  const r = esquema.safeParse(payload);
  if (!r.success) {
    const msg = r.error.issues.map((i) => `${i.path.join('.') || 'pedido'}: ${i.message}`).join('; ');
    throw new ErroHttp(422, `Pedido inválido — ${msg}`);
  }
  return r.data;
}

// Cria o pedido. A mesma "chave" (gerada pela tela ao abrir o formulário) vale uma vez só: clicar duas vezes
// ou repetir a chamada devolve o mesmo pedido, sem criar outro.
export async function criarPedido(pool, config, { loja, usuario, tipo, payload, chave, estadoVisto }) {
  const dados = validarPayload(tipo, payload);

  // baixar conta: o painel só deixa pedir se a conta existe no espelho e está em aberto (o que o usuário viu)
  let visto = estadoVisto ?? null;
  if (tipo === 'CONTA_PAGAR_BAIXAR') {
    const r = await pool.query('SELECT id, status, descricao, valor FROM espelho.contas_pagar WHERE loja_id = $1 AND id = $2', [loja.id, dados.contaId]);
    if (r.rowCount === 0) throw new ErroHttp(404, 'Conta não encontrada no painel. Aguarde a próxima sincronização.');
    if (r.rows[0].status !== 'ABERTA') throw new ErroHttp(409, `Esta conta já está ${r.rows[0].status === 'PAGA' ? 'paga' : 'cancelada'}.`);
    visto = { statusVisto: r.rows[0].status, descricao: r.rows[0].descricao, valor: r.rows[0].valor };
  }
  if (tipo === 'CONTA_PAGAR_LANCAR') {
    const e = await pool.query('SELECT id FROM espelho.empresas WHERE loja_id = $1 AND id = $2 AND ativa', [loja.id, dados.idEmpresa]);
    if (e.rowCount === 0) throw new ErroHttp(422, 'Empresa não encontrada ou desativada.');
  }

  const chaveFinal = chave ? String(chave).slice(0, 100) : null;
  return transacao(pool, async (c) => {
    if (chaveFinal) {
      const existente = await c.query('SELECT * FROM painel.pedidos WHERE loja_id = $1 AND chave = $2', [loja.id, chaveFinal]);
      if (existente.rowCount) return { pedido: existente.rows[0], repetido: true };
    }
    const r = await c.query(
      `INSERT INTO painel.pedidos (loja_id, tipo, payload, chave, solicitante_id, solicitante_nome, estado_visto, expira_em)
       VALUES ($1, $2, $3, $4, $5, $6, $7, now() + make_interval(hours => $8)) RETURNING *`,
      [loja.id, tipo, JSON.stringify(dados), chaveFinal, usuario.id, usuario.nome, visto ? JSON.stringify(visto) : null, config.pedidoValidadeHoras],
    );
    await c.query('INSERT INTO painel.auditoria (usuario_id, usuario, acao, detalhe) VALUES ($1, $2, $3, $4)', [
      usuario.id, usuario.nome, `PEDIDO_${tipo}`, `Pedido ${r.rows[0].id} criado`,
    ]);
    return { pedido: r.rows[0], repetido: false };
  });
}

export async function expirarPedidos(pool) {
  const r = await pool.query(
    `UPDATE painel.pedidos SET status = 'EXPIRADO', concluido_em = now(),
            resultado_mensagem = 'Expirou antes de a loja processar (a loja ficou fora do ar).'
     WHERE status IN ('PENDENTE', 'ENTREGUE') AND expira_em < now()`,
  );
  return r.rowCount;
}

// A loja busca o que está pendente. Pedido já entregue e ainda sem resultado volta de novo (a loja pode ter
// fechado no meio); a loja ignora o que já aplicou.
export async function entregarPedidos(pool, loja) {
  await expirarPedidos(pool);
  const r = await pool.query(
    `UPDATE painel.pedidos SET status = 'ENTREGUE', entregue_em = COALESCE(entregue_em, now())
     WHERE loja_id = $1 AND status IN ('PENDENTE', 'ENTREGUE') AND expira_em > now()
     RETURNING id, tipo, payload, solicitante_nome, criado_em, expira_em`,
    [loja.id],
  );
  return r.rows
    .sort((a, b) => a.criado_em - b.criado_em)
    .map((p) => ({
      id: p.id,
      tipo: p.tipo,
      solicitante: p.solicitante_nome,
      criadoEm: p.criado_em.toISOString(),
      expiraEm: p.expira_em.toISOString(),
      payload: p.payload,
    }));
}

const statusDeResultado = ['APLICADO', 'RECUSADO', 'CONFLITO', 'EXPIRADO'];

export async function registrarResultado(pool, loja, id, corpo) {
  if (!/^[0-9a-f-]{36}$/.test(id)) throw new ErroHttp(404, 'Pedido não encontrado.');
  if (!statusDeResultado.includes(corpo?.status)) throw new ErroHttp(422, 'Status de resultado inválido.');
  return transacao(pool, async (c) => {
    const r = await c.query('SELECT status FROM painel.pedidos WHERE id = $1 AND loja_id = $2 FOR UPDATE', [id, loja.id]);
    if (r.rowCount === 0) throw new ErroHttp(404, 'Pedido não encontrado.');
    if (!['PENDENTE', 'ENTREGUE'].includes(r.rows[0].status)) throw new ErroHttp(409, `O pedido já está ${r.rows[0].status.toLowerCase()}.`);
    await c.query(
      `UPDATE painel.pedidos SET status = $2, concluido_em = now(), resultado_mensagem = $3, resultado_detalhe = $4 WHERE id = $1`,
      [id, corpo.status, String(corpo.mensagem ?? '').slice(0, 1000), corpo.detalhe ? JSON.stringify(corpo.detalhe) : null],
    );
    return { ok: true };
  });
}

export async function cancelarPedido(pool, usuario, id) {
  if (!/^[0-9a-f-]{36}$/.test(id)) throw new ErroHttp(404, 'Pedido não encontrado.');
  return transacao(pool, async (c) => {
    const r = await c.query('SELECT status FROM painel.pedidos WHERE id = $1 FOR UPDATE', [id]);
    if (r.rowCount === 0) throw new ErroHttp(404, 'Pedido não encontrado.');
    // pedido já entregue pode estar sendo aplicado agora: só dá para cancelar o que a loja ainda não buscou
    if (r.rows[0].status !== 'PENDENTE') throw new ErroHttp(409, 'Só dá para cancelar um pedido que a loja ainda não buscou.');
    await c.query("UPDATE painel.pedidos SET status = 'CANCELADO', concluido_em = now(), resultado_mensagem = $2 WHERE id = $1", [id, `Cancelado por ${usuario.nome}`]);
    await c.query('INSERT INTO painel.auditoria (usuario_id, usuario, acao, detalhe) VALUES ($1, $2, $3, $4)', [usuario.id, usuario.nome, 'PEDIDO_CANCELADO', `Pedido ${id}`]);
    return { ok: true };
  });
}

export async function listarPedidos(pool, loja, limite = 100) {
  const r = await pool.query(
    `SELECT id, tipo, status, payload, solicitante_nome, criado_em, expira_em, entregue_em, concluido_em,
            resultado_mensagem, resultado_detalhe, estado_visto
     FROM painel.pedidos WHERE loja_id = $1 ORDER BY criado_em DESC LIMIT $2`,
    [loja.id, limite],
  );
  return r.rows;
}
