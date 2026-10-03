import { carregarConfig } from '../src/config.js';
import { criarApp } from '../src/app.js';
import { contrato } from '../src/contrato.js';
import { checksum } from '../src/ciclo.js';
import { criarPool } from '../src/db.js';
import { migrar } from '../src/migrate.js';
import { gerarToken, hashSenha, hashToken } from '../src/seguranca.js';

export const configTeste = (extra = {}) => ({
  ...carregarConfig({ ...process.env, NODE_ENV: 'test', COOKIE_SEGURO: 'false', EXIGIR_2FA: 'false' }),
  ...extra,
});

export async function prepararBanco() {
  const config = configTeste();
  const pool = criarPool(config.databaseUrl);
  await migrar(pool);
  return { pool, config };
}

// zera tudo entre os testes (a estrutura fica)
export async function limpar(pool) {
  await pool.query('TRUNCATE painel.pedidos, painel.ciclo_lotes, painel.ciclos, painel.sessoes, painel.auditoria, painel.usuarios, painel.lojas RESTART IDENTITY CASCADE');
  for (const t of contrato.tabelas) {
    await pool.query(`TRUNCATE espelho."${t.nome}"`);
    await pool.query(`TRUNCATE stg."${t.nome}" RESTART IDENTITY`);
  }
}

export async function criarLoja(pool, nome = 'Loja de Teste') {
  const token = gerarToken(32);
  const r = await pool.query('INSERT INTO painel.lojas (nome, token_hash) VALUES ($1, $2) RETURNING id, nome, fuso', [nome, hashToken(token)]);
  return { ...r.rows[0], token };
}

export async function criarUsuario(pool, { nome = 'Maria', email = 'maria@exemplo.com', perfil = 'gerente', senha = 'Senha-Forte-123' } = {}) {
  const r = await pool.query('INSERT INTO painel.usuarios (nome, email, senha_hash, perfil) VALUES ($1, $2, $3, $4) RETURNING id, nome, email, perfil',
    [nome, email, await hashSenha(senha), perfil]);
  return { ...r.rows[0], senha };
}

export function criarAppTeste(pool, config = configTeste()) {
  return criarApp({ pool, config });
}

// ---- monta o corpo de um ciclo completo a partir de { tabela: [linhas como arrays de texto/null] } ------------
export function montarCiclo(dados = {}, extra = {}) {
  const tabelas = contrato.tabelas.map((t) => {
    const linhas = dados[t.nome] ?? [];
    return { nome: t.nome, colunas: t.colunas.map((c) => c.nome), linhas, checksum: checksum(linhas) };
  });
  return {
    abrir: {
      contratoVersao: contrato.versao,
      contratoHash: contrato.hash,
      versaoApp: 'teste',
      terminal: 'CAIXA-TESTE',
      fuso: 'America/Sao_Paulo',
      geradoEm: new Date().toISOString(),
      tabelas: tabelas.map((t) => ({ nome: t.nome, linhas: t.linhas.length, checksum: t.checksum, colunas: t.colunas })),
      ...extra,
    },
    tabelas,
  };
}

const autorizacao = (token) => ({ authorization: `Bearer ${token}` });

// envia um ciclo inteiro pela API (rotas HTTP reais, via inject)
export async function enviarCiclo(app, token, dados, { lotesDe = 500 } = {}) {
  const c = montarCiclo(dados);
  const ab = await app.inject({ method: 'POST', url: '/api/loja/ciclo/iniciar', headers: autorizacao(token), payload: c.abrir });
  if (ab.statusCode !== 200) return { etapa: 'iniciar', resp: ab };
  const cicloId = ab.json().cicloId;
  for (const t of c.tabelas) {
    for (let i = 0, lote = 0; i < t.linhas.length; i += lotesDe, lote++) {
      const r = await app.inject({
        method: 'PUT', url: `/api/loja/ciclo/${cicloId}/tabela/${t.nome}`, headers: autorizacao(token),
        payload: { lote, colunas: t.colunas, linhas: t.linhas.slice(i, i + lotesDe) },
      });
      if (r.statusCode !== 200) return { etapa: 'lote', resp: r, cicloId };
    }
  }
  const fim = await app.inject({ method: 'POST', url: `/api/loja/ciclo/${cicloId}/concluir`, headers: autorizacao(token), payload: { tabelas: c.abrir.tabelas } });
  return { etapa: 'concluir', resp: fim, cicloId };
}

export { autorizacao };

// linha(tabela, { coluna: valor }) -> array na ordem do contrato (texto ou null)
export function linha(nomeTabela, obj) {
  const t = contrato.tabela(nomeTabela);
  return t.colunas.map((c) => (obj[c.nome] === undefined || obj[c.nome] === null ? null : String(obj[c.nome])));
}
