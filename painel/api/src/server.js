import { criarApp } from './app.js';
import { carregarConfig } from './config.js';
import { criarPool } from './db.js';
import { expirarCiclosParados } from './ciclo.js';
import { limparSessoesVencidas } from './auth.js';
import { migrar } from './migrate.js';
import { expirarPedidos } from './pedidos.js';

const config = carregarConfig();
const pool = criarPool(config.databaseUrl);

// o banco pode demorar a ficar pronto quando tudo sobe junto (docker compose): tenta por um tempo
async function migrarComTentativas(tentativas = 30) {
  for (let i = 1; i <= tentativas; i++) {
    try {
      await migrar(pool);
      return;
    } catch (erro) {
      if (i === tentativas) throw erro;
      console.log(`Banco ainda não está pronto (${erro.message}). Tentativa ${i}/${tentativas}...`);
      await new Promise((r) => setTimeout(r, 2000));
    }
  }
}

await migrarComTentativas();
const app = criarApp({ pool, config });

// tarefas de manutenção a cada minuto: pedidos vencidos, ciclos parados e sessões antigas
const manutencao = setInterval(async () => {
  try {
    await expirarPedidos(pool);
    await expirarCiclosParados(pool, config.cicloValidadeMinutos);
    await limparSessoesVencidas(pool);
  } catch (erro) {
    app.log.error(erro, 'manutenção falhou');
  }
}, 60 * 1000);
manutencao.unref();

const encerrar = async () => {
  clearInterval(manutencao);
  await app.close();
  await pool.end();
  process.exit(0);
};
process.on('SIGTERM', encerrar);
process.on('SIGINT', encerrar);

await app.listen({ port: config.porta, host: '0.0.0.0' });
