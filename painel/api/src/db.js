import pg from 'pg';

// Valores numeric/bigint chegam como texto por padrão no driver: o painel trata os valores como texto
// ou converte de forma explícita nas consultas, então não há perda de precisão silenciosa.
export function criarPool(databaseUrl) {
  return new pg.Pool({ connectionString: databaseUrl, max: 10, idleTimeoutMillis: 30000 });
}

// Executa fn dentro de uma transação (rollback automático em qualquer erro).
export async function transacao(pool, fn) {
  const cliente = await pool.connect();
  try {
    await cliente.query('BEGIN');
    const resultado = await fn(cliente);
    await cliente.query('COMMIT');
    return resultado;
  } catch (erro) {
    try {
      await cliente.query('ROLLBACK');
    } catch {
      /* a conexão já caiu: nada a desfazer */
    }
    throw erro;
  } finally {
    cliente.release();
  }
}
