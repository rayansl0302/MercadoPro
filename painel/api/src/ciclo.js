import { createHash } from 'node:crypto';
import { contrato, expressaoConversao } from './contrato.js';
import { transacao } from './db.js';
import { ident } from './espelho.js';

// Erro com código HTTP: as rotas respondem com a mensagem (em português) para o programa da loja mostrar.
export class ErroHttp extends Error {
  constructor(status, mensagem, detalhe) {
    super(mensagem);
    this.status = status;
    this.detalhe = detalhe;
  }
}

// ---- conferência de integridade ---------------------------------------------------------------------------

const SEP_VALOR = String.fromCharCode(0x1f);
const FIM_LINHA = String.fromCharCode(0x1e);

// Mesmo cálculo da loja (NuvemExportador::checksum): sha256 de valores separados por 0x1F, linhas terminadas em
// 0x1E, nulo = "\N". Vetores compartilhados: test/checksum.test.js.
export function checksum(linhas) {
  const hash = createHash('sha256');
  for (const linha of linhas) {
    hash.update(linha.map((v) => (v === null || v === undefined ? '\\N' : String(v))).join(SEP_VALOR), 'utf8');
    hash.update(FIM_LINHA, 'utf8');
  }
  return hash.digest('hex');
}

// ---- ciclo de envio ---------------------------------------------------------------------------------------

function conferirDeclaracao(corpo) {
  if (corpo.contratoVersao !== contrato.versao || corpo.contratoHash !== contrato.hash) {
    throw new ErroHttp(
      409,
      `Contrato de dados incompatível: a loja usa a versão ${corpo.contratoVersao} e a API a ${contrato.versao}. ` +
        'Atualize o QEstoqueLoja ou a API.',
    );
  }
  const declaradas = Array.isArray(corpo.tabelas) ? corpo.tabelas : [];
  const nomes = new Set();
  for (const d of declaradas) {
    const t = contrato.tabela(d?.nome);
    if (!t) throw new ErroHttp(422, `Tabela fora do contrato: ${d?.nome}`);
    if (nomes.has(t.nome)) throw new ErroHttp(422, `Tabela repetida: ${t.nome}`);
    nomes.add(t.nome);
    const esperadas = t.colunas.map((c) => c.nome);
    if (JSON.stringify(d.colunas) !== JSON.stringify(esperadas))
      throw new ErroHttp(422, `Colunas de ${t.nome} diferem do contrato.`);
    if (!Number.isInteger(d.linhas) || d.linhas < 0) throw new ErroHttp(422, `Contagem inválida em ${t.nome}.`);
    if (!/^[0-9a-f]{64}$/.test(String(d.checksum))) throw new ErroHttp(422, `Checksum inválido em ${t.nome}.`);
  }
  // o espelho é uma foto completa: faltar tabela deixaria dados antigos misturados com novos
  for (const t of contrato.tabelas)
    if (!nomes.has(t.nome)) throw new ErroHttp(422, `A loja não avisou a tabela ${t.nome}.`);
  return declaradas;
}

export async function descartarStaging(cliente, cicloId) {
  for (const t of contrato.tabelas) await cliente.query(`DELETE FROM stg.${ident(t.nome)} WHERE ciclo_id = $1`, [cicloId]);
}

export async function iniciarCiclo(pool, loja, corpo) {
  const declaradas = conferirDeclaracao(corpo);
  return transacao(pool, async (c) => {
    // um ciclo novo invalida os que ficaram abertos (a loja caiu no meio): o último vale
    const abertos = await c.query("SELECT id FROM painel.ciclos WHERE loja_id = $1 AND status = 'ABERTO'", [loja.id]);
    for (const { id } of abertos.rows) {
      await descartarStaging(c, id);
      await c.query("UPDATE painel.ciclos SET status = 'EXPIRADO', erro = 'Substituído por um ciclo novo' WHERE id = $1", [id]);
    }
    const r = await c.query(
      `INSERT INTO painel.ciclos (loja_id, terminal, contrato_versao, gerado_em, fuso, declarado)
       VALUES ($1, $2, $3, $4, $5, $6) RETURNING id`,
      [loja.id, corpo.terminal ?? null, corpo.contratoVersao, corpo.geradoEm ?? null, corpo.fuso ?? null, JSON.stringify(declaradas)],
    );
    return r.rows[0].id;
  });
}

async function cicloAberto(c, loja, cicloId) {
  if (!/^[0-9a-f-]{36}$/.test(cicloId)) throw new ErroHttp(404, 'Ciclo não encontrado.');
  const r = await c.query('SELECT id, status, declarado FROM painel.ciclos WHERE id = $1 AND loja_id = $2 FOR UPDATE', [cicloId, loja.id]);
  if (r.rowCount === 0) throw new ErroHttp(404, 'Ciclo não encontrado.');
  if (r.rows[0].status !== 'ABERTO') throw new ErroHttp(409, `O ciclo já está ${r.rows[0].status.toLowerCase()}.`);
  return r.rows[0];
}

export async function receberLote(pool, loja, cicloId, nomeTabela, corpo) {
  const t = contrato.tabela(nomeTabela);
  if (!t) throw new ErroHttp(422, `Tabela fora do contrato: ${nomeTabela}`);
  const esperadas = t.colunas.map((c) => c.nome);
  if (JSON.stringify(corpo.colunas) !== JSON.stringify(esperadas)) throw new ErroHttp(422, `Colunas de ${t.nome} diferem do contrato.`);
  if (!Number.isInteger(corpo.lote) || corpo.lote < 0) throw new ErroHttp(422, 'Número de lote inválido.');
  const linhas = corpo.linhas;
  if (!Array.isArray(linhas)) throw new ErroHttp(422, 'Lote sem linhas.');
  for (const linha of linhas) {
    if (!Array.isArray(linha) || linha.length !== esperadas.length) throw new ErroHttp(422, `Linha com número de colunas errado em ${t.nome}.`);
    for (const v of linha) if (v !== null && typeof v !== 'string') throw new ErroHttp(422, `Valor que não é texto em ${t.nome}.`);
  }

  return transacao(pool, async (c) => {
    await cicloAberto(c, loja, cicloId);
    const ult = await c.query('SELECT ultimo_lote FROM painel.ciclo_lotes WHERE ciclo_id = $1 AND tabela = $2 FOR UPDATE', [cicloId, t.nome]);
    const ultimo = ult.rowCount ? ult.rows[0].ultimo_lote : -1;
    if (corpo.lote <= ultimo) return { repetido: true };           // o mesmo lote de novo: já está guardado
    if (corpo.lote !== ultimo + 1) throw new ErroHttp(409, `Lote fora de ordem em ${t.nome}: esperado ${ultimo + 1}, recebido ${corpo.lote}.`);
    if (linhas.length > 0) {
      // uma coluna por parâmetro (arrays) mantém a ordem de chegada e evita montar SQL com os valores
      const arrays = esperadas.map((_, i) => linhas.map((l) => l[i]));
      const colunasSql = esperadas.map(ident).join(', ');
      const params = esperadas.map((_, i) => `$${i + 2}::text[]`).join(', ');
      await c.query(
        `INSERT INTO stg.${ident(t.nome)} (ciclo_id, ${colunasSql}) SELECT $1::uuid, ${esperadas.map((_, i) => `c${i}`).join(', ')} FROM unnest(${params}) WITH ORDINALITY AS u(${esperadas.map((_, i) => `c${i}`).join(', ')}, ord) ORDER BY ord`,
        [cicloId, ...arrays],
      );
    }
    await c.query(
      `INSERT INTO painel.ciclo_lotes (ciclo_id, tabela, ultimo_lote) VALUES ($1, $2, $3)
       ON CONFLICT (ciclo_id, tabela) DO UPDATE SET ultimo_lote = EXCLUDED.ultimo_lote`,
      [cicloId, t.nome, corpo.lote],
    );
    return { repetido: false, linhas: linhas.length };
  });
}

async function checksumDoStaging(c, t, cicloId) {
  const colunasSql = t.colunas.map((col) => ident(col.nome)).join(', ');
  const r = await c.query(`SELECT ${colunasSql} FROM stg.${ident(t.nome)} WHERE ciclo_id = $1 ORDER BY seq`, [cicloId]);
  const linhas = r.rows.map((row) => t.colunas.map((col) => row[col.nome]));
  return { linhas: linhas.length, checksum: checksum(linhas) };
}

export async function concluirCiclo(pool, loja, cicloId) {
  try {
    return await transacao(pool, async (c) => {
      const ciclo = await cicloAberto(c, loja, cicloId);
      const declaradas = new Map(ciclo.declarado.map((d) => [d.nome, d]));

      // 1) o que chegou é exatamente o que a loja disse que mandou (contagem e conteúdo)
      const divergencias = [];
      let total = 0;
      for (const t of contrato.tabelas) {
        const d = declaradas.get(t.nome);
        const recebido = await checksumDoStaging(c, t, cicloId);
        total += recebido.linhas;
        if (recebido.linhas !== d.linhas) divergencias.push(`${t.nome}: chegaram ${recebido.linhas} linhas, a loja enviou ${d.linhas}`);
        else if (recebido.checksum !== d.checksum) divergencias.push(`${t.nome}: o conteúdo recebido difere do enviado`);
      }
      if (divergencias.length) throw new ErroHttp(422, `Conferência falhou: ${divergencias.join('; ')}`);

      // 2) troca o espelho desta loja de uma vez (qualquer erro de conversão desfaz tudo e o espelho antigo fica)
      for (const t of contrato.tabelas) {
        const cols = t.colunas.map((col) => ident(col.nome)).join(', ');
        const conversoes = t.colunas.map((col) => expressaoConversao(col.nome, col.tipo)).join(', ');
        await c.query(`DELETE FROM espelho.${ident(t.nome)} WHERE loja_id = $1`, [loja.id]);
        await c.query(
          `INSERT INTO espelho.${ident(t.nome)} (loja_id, ${cols}) SELECT $1, ${conversoes} FROM stg.${ident(t.nome)} WHERE ciclo_id = $2 ORDER BY seq`,
          [loja.id, cicloId],
        );
      }
      await descartarStaging(c, cicloId);
      await c.query("UPDATE painel.ciclos SET status = 'CONCLUIDO', concluido_em = now(), linhas = $2 WHERE id = $1", [cicloId, total]);
      return { linhas: total };
    });
  } catch (erro) {
    // a transação desfez o espelho; registra a falha e limpa a preparação fora dela
    await transacao(pool, async (c) => {
      await descartarStaging(c, cicloId);
      await c.query("UPDATE painel.ciclos SET status = 'FALHOU', erro = $2 WHERE id = $1 AND status = 'ABERTO'", [cicloId, String(erro.message).slice(0, 1000)]);
    }).catch(() => {});
    if (erro instanceof ErroHttp) throw erro;
    throw new ErroHttp(422, `Não foi possível aplicar o envio: ${erro.message}`);
  }
}

export async function cancelarCiclo(pool, loja, cicloId) {
  return transacao(pool, async (c) => {
    const r = await c.query('SELECT status FROM painel.ciclos WHERE id = $1 AND loja_id = $2', [cicloId, loja.id]);
    if (r.rowCount === 0) throw new ErroHttp(404, 'Ciclo não encontrado.');
    if (r.rows[0].status === 'ABERTO') {
      await descartarStaging(c, cicloId);
      await c.query("UPDATE painel.ciclos SET status = 'CANCELADO' WHERE id = $1", [cicloId]);
    }
    return { ok: true };
  });
}

// ciclos abertos há tempo demais (a loja caiu e nunca mais voltou) liberam a preparação
export async function expirarCiclosParados(pool, minutos) {
  return transacao(pool, async (c) => {
    const r = await c.query(
      "SELECT id FROM painel.ciclos WHERE status = 'ABERTO' AND iniciado_em < now() - make_interval(mins => $1)",
      [minutos],
    );
    for (const { id } of r.rows) {
      await descartarStaging(c, id);
      await c.query("UPDATE painel.ciclos SET status = 'EXPIRADO', erro = 'Ciclo parado' WHERE id = $1", [id]);
    }
    return r.rowCount;
  });
}
