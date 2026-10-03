import { readdir, readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { transacao } from './db.js';
import { garantirEspelho } from './espelho.js';

const pastaMigracoes = path.join(path.dirname(fileURLToPath(import.meta.url)), 'migrations');

// Aplica as migrações SQL em ordem (cada uma uma única vez) e sincroniza as tabelas do espelho com o contrato.
// Uma trava consultiva impede duas instâncias de migrar ao mesmo tempo.
export async function migrar(pool) {
  const cliente = await pool.connect();
  try {
    await cliente.query('SELECT pg_advisory_lock(727001)');
    await cliente.query('CREATE SCHEMA IF NOT EXISTS painel');
    await cliente.query(
      'CREATE TABLE IF NOT EXISTS painel.migracoes (nome text PRIMARY KEY, aplicada_em timestamptz NOT NULL DEFAULT now())',
    );
    const jaFeitas = new Set((await cliente.query('SELECT nome FROM painel.migracoes')).rows.map((r) => r.nome));
    const arquivos = (await readdir(pastaMigracoes)).filter((n) => n.endsWith('.sql')).sort();
    for (const nome of arquivos) {
      if (jaFeitas.has(nome)) continue;
      const sql = await readFile(path.join(pastaMigracoes, nome), 'utf8');
      await cliente.query('BEGIN');
      try {
        await cliente.query(sql);
        await cliente.query('INSERT INTO painel.migracoes (nome) VALUES ($1)', [nome]);
        await cliente.query('COMMIT');
      } catch (erro) {
        await cliente.query('ROLLBACK');
        throw new Error(`Migração ${nome} falhou: ${erro.message}`);
      }
    }
  } finally {
    await cliente.query('SELECT pg_advisory_unlock(727001)').catch(() => {});
    cliente.release();
  }
  await transacao(pool, (c) => garantirEspelho(c));
}
