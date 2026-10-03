import pg from 'pg';

// Recria o banco de teste do zero antes de rodar (nunca toca no banco "painel" de verdade).
export default async function setup() {
  const admin = process.env.DATABASE_ADMIN_URL;
  if (!admin) throw new Error('DATABASE_ADMIN_URL não definida: os testes precisam de um PostgreSQL (docker compose --profile test run --rm testes).');
  const cliente = new pg.Client({ connectionString: admin });
  await cliente.connect();
  try {
    await cliente.query('DROP DATABASE IF EXISTS painel_test WITH (FORCE)');
    await cliente.query('CREATE DATABASE painel_test');
  } finally {
    await cliente.end();
  }
}
