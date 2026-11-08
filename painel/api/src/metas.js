import { z } from 'zod';
import { ErroHttp } from './ciclo.js';

// Ponto de equilíbrio: quanto a loja precisa vender no mês para pagar as despesas fixas, dada a margem de lucro.
//   faturamento necessário = despesas fixas ÷ margem        (ex.: 8.000 ÷ 30% = 26.666,67)
export function pontoDeEquilibrio(totalFixo, margemPercentual, diasFuncionamento) {
  const margem = Number(margemPercentual) / 100;
  if (!(margem > 0)) return { faturamentoNecessario: 0, porDia: 0 };
  const necessario = Math.round((Number(totalFixo) / margem) * 100) / 100;
  return { faturamentoNecessario: necessario, porDia: Math.round((necessario / Math.max(1, diasFuncionamento)) * 100) / 100 };
}

const dinheiro = (v) => Math.round(Number(v ?? 0) * 100) / 100;
const despesa = z.object({
  id: z.number().int().positive().optional(),
  nome: z.string().trim().min(2, 'Informe o nome da despesa').max(100),
  valor: z.number().min(0).max(99999999),
});
const config = z.object({
  margemPercentual: z.number().gt(0, 'A margem precisa ser maior que zero').max(100),
  diasFuncionamento: z.number().int().min(1).max(31),
});

async function dia(pool, fuso) {
  return (await pool.query("SELECT (now() AT TIME ZONE $1)::date::text AS d", [fuso || 'America/Sao_Paulo'])).rows[0].d;
}

export async function lerMetas(pool, loja) {
  const L = loja.id;
  const d = await pool.query('SELECT id, nome, valor FROM painel.despesas_fixas WHERE loja_id = $1 ORDER BY valor DESC, id', [L]);
  const m = (await pool.query('SELECT margem_percentual, dias_funcionamento FROM painel.metas WHERE loja_id = $1', [L])).rows[0];
  const margem = m ? Number(m.margem_percentual) : 30;
  const dias = m ? m.dias_funcionamento : 30;
  const totalFixo = dinheiro(d.rows.reduce((s, x) => s + Number(x.valor), 0));
  const alvo = pontoDeEquilibrio(totalFixo, margem, dias);

  const hoje = await dia(pool, loja.fuso);
  const inicio = `${hoje.slice(0, 8)}01`;
  const diaDoMes = Number(hoje.slice(8, 10));
  const diasNoMes = new Date(Number(hoje.slice(0, 4)), Number(hoje.slice(5, 7)), 0).getDate();
  const v = await pool.query(
    `SELECT coalesce(sum(valor_final), 0) AS total FROM espelho.vendas2
     WHERE loja_id = $1 AND painel.ts_seguro(data_hora)::date >= $2::date AND painel.ts_seguro(data_hora)::date <= $3::date`,
    [L, inicio, hoje],
  );
  const vendido = dinheiro(v.rows[0].total);
  const projecao = dinheiro((vendido / Math.max(1, diaDoMes)) * diasNoMes);
  return {
    despesas: d.rows.map((x) => ({ id: x.id, nome: x.nome, valor: dinheiro(x.valor) })),
    totalFixo,
    margemPercentual: margem,
    diasFuncionamento: dias,
    ...alvo,
    mes: {
      vendido,
      percentualDaMeta: alvo.faturamentoNecessario > 0 ? Math.round((vendido / alvo.faturamentoNecessario) * 1000) / 10 : 0,
      faltaVender: Math.max(0, dinheiro(alvo.faturamentoNecessario - vendido)),
      projecaoDoMes: projecao,
      diaDoMes, diasNoMes,
      lucroEstimado: dinheiro(vendido * (margem / 100) - totalFixo),   // lucro do mês até agora, depois das despesas fixas
    },
  };
}

export async function salvarDespesa(pool, loja, corpo) {
  const d = despesa.parse(corpo);
  if (d.id) {
    const r = await pool.query('UPDATE painel.despesas_fixas SET nome = $3, valor = $4 WHERE loja_id = $1 AND id = $2', [loja.id, d.id, d.nome, d.valor]);
    if (!r.rowCount) throw new ErroHttp(404, 'Despesa não encontrada.');
  } else {
    await pool.query('INSERT INTO painel.despesas_fixas (loja_id, nome, valor) VALUES ($1, $2, $3)', [loja.id, d.nome, d.valor]);
  }
  return lerMetas(pool, loja);
}

export async function removerDespesa(pool, loja, id) {
  await pool.query('DELETE FROM painel.despesas_fixas WHERE loja_id = $1 AND id = $2', [loja.id, Number(id)]);
  return lerMetas(pool, loja);
}

export async function salvarConfig(pool, loja, corpo) {
  const c = config.parse(corpo);
  await pool.query(
    `INSERT INTO painel.metas (loja_id, margem_percentual, dias_funcionamento) VALUES ($1, $2, $3)
     ON CONFLICT (loja_id) DO UPDATE SET margem_percentual = EXCLUDED.margem_percentual, dias_funcionamento = EXCLUDED.dias_funcionamento`,
    [loja.id, c.margemPercentual, c.diasFuncionamento],
  );
  return lerMetas(pool, loja);
}
