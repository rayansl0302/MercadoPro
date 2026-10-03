import { contrato, tiposPg } from './contrato.js';

const ident = (nome) => `"${nome.replace(/"/g, '""')}"`;

// Cria (ou completa) as tabelas do espelho e de preparação a partir do contrato:
//  - espelho.<tabela>: dados da loja com tipos finais, uma loja por vez (loja_id);
//  - stg.<tabela>: preparação do ciclo em andamento, tudo texto, na ordem de chegada (seq).
// Coluna nova no contrato é adicionada sozinha; coluna removida do contrato não é apagada (nada se perde).
export async function garantirEspelho(cliente) {
  await cliente.query('CREATE SCHEMA IF NOT EXISTS espelho');
  await cliente.query('CREATE SCHEMA IF NOT EXISTS stg');
  for (const t of contrato.tabelas) {
    const colunasEspelho = t.colunas.map((c) => `${ident(c.nome)} ${tiposPg[c.tipo]}`).join(', ');
    const pk = ['loja_id', ...t.pk].map(ident).join(', ');
    await cliente.query(
      `CREATE TABLE IF NOT EXISTS espelho.${ident(t.nome)} (loja_id integer NOT NULL, ${colunasEspelho}, PRIMARY KEY (${pk}))`,
    );
    for (const c of t.colunas)
      await cliente.query(`ALTER TABLE espelho.${ident(t.nome)} ADD COLUMN IF NOT EXISTS ${ident(c.nome)} ${tiposPg[c.tipo]}`);

    const colunasStg = t.colunas.map((c) => `${ident(c.nome)} text`).join(', ');
    await cliente.query(
      `CREATE UNLOGGED TABLE IF NOT EXISTS stg.${ident(t.nome)} (seq bigserial PRIMARY KEY, ciclo_id uuid NOT NULL, ${colunasStg})`,
    );
    for (const c of t.colunas)
      await cliente.query(`ALTER TABLE stg.${ident(t.nome)} ADD COLUMN IF NOT EXISTS ${ident(c.nome)} text`);
    await cliente.query(`CREATE INDEX IF NOT EXISTS ${ident('idx_stg_' + t.nome)} ON stg.${ident(t.nome)} (ciclo_id, seq)`);
  }
}

export { ident };
