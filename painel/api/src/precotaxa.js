// Preço de gôndola: o cadastro guarda o preço BASE; o PDV cobra base ÷ (1 − taxa), com a maior taxa
// (débito ou crédito) das maquininhas ativas. Mesma conta do desktop (PrecoTaxa::comTaxa).
export async function taxaDaLoja(pool, lojaId) {
  const r = await pool.query(
    `SELECT coalesce(max(greatest(coalesce(taxa_debito, 0), coalesce(taxa_credito, 0))), 0) AS taxa
     FROM espelho.maquinas_cartao WHERE loja_id = $1 AND ativa`,
    [lojaId],
  );
  return Number(r.rows[0]?.taxa ?? 0);
}

export function comTaxa(base, taxa) {
  const b = Number(base ?? 0);
  if (!(b > 0) || !(taxa > 0) || !(taxa < 100)) return Math.round(b * 100) / 100;
  return Math.round((b / (1 - taxa / 100)) * 100) / 100;
}
