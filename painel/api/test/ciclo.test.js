import { afterAll, beforeAll, beforeEach, describe, expect, it } from 'vitest';
import { autorizacao, criarAppTeste, criarLoja, enviarCiclo, limpar, linha, montarCiclo, prepararBanco } from './ajuda.js';
import { contrato } from '../src/contrato.js';

let pool;
let app;
let loja;

beforeAll(async () => {
  ({ pool } = await prepararBanco());
  app = criarAppTeste(pool);
});
afterAll(async () => {
  await app.close();
  await pool.end();
});
beforeEach(async () => {
  await limpar(pool);
  loja = await criarLoja(pool);
});

const dadosBasicos = () => ({
  empresas: [linha('empresas', { id: 1, apelido: 'Empresa A', cnpj: '11222333000181', ativa: 1 })],
  clientes: [linha('clientes', { id: 2, nome: 'Maria da Silva', limite_credito: '500.00', ativo: 1 })],
  vendas2: [
    linha('vendas2', { id: 1, cliente: 'Maria', data_hora: '2026-10-01 10:00:00', total: '100.00', valor_final: '100.00', forma_pagamento: 'Pix', esta_pago: 1, id_empresa: 1 }),
    linha('vendas2', { id: 2, data_hora: '2026-10-01 11:00:00', total: '50.50', valor_final: '50.50', forma_pagamento: 'Dinheiro', esta_pago: 0, id_empresa: 1 }),
  ],
  produtos: [linha('produtos', { id: 1, descricao: 'CAFÉ 500G ç', quantidade: '2.5', preco: '18.90', nf: 0 })],
});

describe('ciclo de envio (rotas reais)', () => {
  it('recebe, confere e troca o espelho, com os tipos certos', async () => {
    const r = await enviarCiclo(app, loja.token, dadosBasicos());
    expect(r.resp.statusCode, JSON.stringify(r.resp.json())).toBe(200);
    expect(r.resp.json().linhas).toBe(5);

    const v = await pool.query('SELECT id, valor_final, esta_pago, data_hora, id_empresa FROM espelho.vendas2 WHERE loja_id = $1 ORDER BY id', [loja.id]);
    expect(v.rows).toHaveLength(2);
    expect(v.rows[0].valor_final).toBe('100.00');
    expect(v.rows[0].esta_pago).toBe(true);
    expect(v.rows[1].esta_pago).toBe(false);
    expect(v.rows[1].data_hora).toBe('2026-10-01 11:00:00');
    const p = await pool.query('SELECT descricao, quantidade FROM espelho.produtos WHERE loja_id = $1', [loja.id]);
    expect(p.rows[0].descricao).toBe('CAFÉ 500G ç');
    expect(p.rows[0].quantidade).toBe(2.5);
    const c = await pool.query('SELECT status, linhas FROM painel.ciclos WHERE id = $1', [r.cicloId]);
    expect(c.rows[0].status).toBe('CONCLUIDO');
    // a preparação é limpa depois de trocar
    const stg = await pool.query('SELECT count(*)::int AS n FROM stg.vendas2');
    expect(stg.rows[0].n).toBe(0);
  });

  it('o ciclo seguinte substitui o anterior (linha que sumiu na loja some do espelho)', async () => {
    await enviarCiclo(app, loja.token, dadosBasicos());
    const menos = dadosBasicos();
    menos.vendas2 = [menos.vendas2[0]];
    const r = await enviarCiclo(app, loja.token, menos);
    expect(r.resp.statusCode).toBe(200);
    const v = await pool.query('SELECT id FROM espelho.vendas2 WHERE loja_id = $1', [loja.id]);
    expect(v.rows.map((x) => x.id)).toEqual(['1']);
  });

  it('envia em vários lotes mantendo a ordem', async () => {
    const muitas = Array.from({ length: 1234 }, (_, i) => linha('vendas2', { id: i + 1, data_hora: '2026-10-01 10:00:00', total: '1.00', valor_final: '1.00', id_empresa: 1 }));
    const r = await enviarCiclo(app, loja.token, { vendas2: muitas }, { lotesDe: 100 });
    expect(r.resp.statusCode, JSON.stringify(r.resp.json())).toBe(200);
    const n = await pool.query('SELECT count(*)::int AS n FROM espelho.vendas2 WHERE loja_id = $1', [loja.id]);
    expect(n.rows[0].n).toBe(1234);
  });

  it('o mesmo lote reenviado (resposta perdida) não duplica linhas', async () => {
    const c = montarCiclo(dadosBasicos());
    const ab = await app.inject({ method: 'POST', url: '/api/loja/ciclo/iniciar', headers: autorizacao(loja.token), payload: c.abrir });
    const id = ab.json().cicloId;
    const vendas = c.tabelas.find((t) => t.nome === 'vendas2');
    const enviar = () => app.inject({ method: 'PUT', url: `/api/loja/ciclo/${id}/tabela/vendas2`, headers: autorizacao(loja.token), payload: { lote: 0, colunas: vendas.colunas, linhas: vendas.linhas } });
    expect((await enviar()).statusCode).toBe(200);
    const repetido = await enviar();
    expect(repetido.statusCode).toBe(200);
    expect(repetido.json().repetido).toBe(true);
    const n = await pool.query('SELECT count(*)::int AS n FROM stg.vendas2 WHERE ciclo_id = $1', [id]);
    expect(n.rows[0].n).toBe(2);
  });

  it('recusa lote fora de ordem', async () => {
    const c = montarCiclo(dadosBasicos());
    const ab = await app.inject({ method: 'POST', url: '/api/loja/ciclo/iniciar', headers: autorizacao(loja.token), payload: c.abrir });
    const t = c.tabelas.find((x) => x.nome === 'vendas2');
    const r = await app.inject({ method: 'PUT', url: `/api/loja/ciclo/${ab.json().cicloId}/tabela/vendas2`, headers: autorizacao(loja.token), payload: { lote: 3, colunas: t.colunas, linhas: t.linhas } });
    expect(r.statusCode).toBe(409);
    expect(r.json().mensagem).toContain('fora de ordem');
  });

  it('conferência falha se faltar linha: nada é trocado e o espelho antigo continua', async () => {
    await enviarCiclo(app, loja.token, dadosBasicos());
    const c = montarCiclo(dadosBasicos());
    const ab = await app.inject({ method: 'POST', url: '/api/loja/ciclo/iniciar', headers: autorizacao(loja.token), payload: c.abrir });
    const id = ab.json().cicloId;
    const t = c.tabelas.find((x) => x.nome === 'vendas2');
    // manda só uma das duas linhas declaradas
    await app.inject({ method: 'PUT', url: `/api/loja/ciclo/${id}/tabela/vendas2`, headers: autorizacao(loja.token), payload: { lote: 0, colunas: t.colunas, linhas: [t.linhas[0]] } });
    const fim = await app.inject({ method: 'POST', url: `/api/loja/ciclo/${id}/concluir`, headers: autorizacao(loja.token), payload: {} });
    expect(fim.statusCode).toBe(422);
    expect(fim.json().mensagem).toContain('vendas2');
    const n = await pool.query('SELECT count(*)::int AS n FROM espelho.vendas2 WHERE loja_id = $1', [loja.id]);
    expect(n.rows[0].n).toBe(2);                                      // o espelho antigo não foi mexido
    const ciclo = await pool.query('SELECT status FROM painel.ciclos WHERE id = $1', [id]);
    expect(ciclo.rows[0].status).toBe('FALHOU');
  });

  it('conferência falha se o conteúdo mudou no caminho (mesma contagem, checksum diferente)', async () => {
    const c = montarCiclo(dadosBasicos());
    const ab = await app.inject({ method: 'POST', url: '/api/loja/ciclo/iniciar', headers: autorizacao(loja.token), payload: c.abrir });
    const id = ab.json().cicloId;
    for (const t of c.tabelas) {
      const linhas = t.nome === 'vendas2' ? t.linhas.map((l) => l.map((v, i) => (i === 3 && v ? '999.99' : v))) : t.linhas;
      if (linhas.length) await app.inject({ method: 'PUT', url: `/api/loja/ciclo/${id}/tabela/${t.nome}`, headers: autorizacao(loja.token), payload: { lote: 0, colunas: t.colunas, linhas } });
    }
    const fim = await app.inject({ method: 'POST', url: `/api/loja/ciclo/${id}/concluir`, headers: autorizacao(loja.token), payload: {} });
    expect(fim.statusCode).toBe(422);
    expect(fim.json().mensagem).toContain('difere');
  });

  it('valor impossível de converter desfaz o envio inteiro (espelho antigo preservado)', async () => {
    await enviarCiclo(app, loja.token, dadosBasicos());
    const ruim = dadosBasicos();
    ruim.vendas2 = [linha('vendas2', { id: 'abc', total: '1.00' })];     // id que não é número
    const r = await enviarCiclo(app, loja.token, ruim);
    expect(r.resp.statusCode).toBe(422);
    const n = await pool.query('SELECT count(*)::int AS n FROM espelho.vendas2 WHERE loja_id = $1', [loja.id]);
    expect(n.rows[0].n).toBe(2);
    const clientes = await pool.query('SELECT count(*)::int AS n FROM espelho.clientes WHERE loja_id = $1', [loja.id]);
    expect(clientes.rows[0].n).toBe(1);                                // nenhuma tabela foi trocada pela metade
  });

  it('recusa contrato incompatível, tabela fora do contrato e coluna trocada', async () => {
    const c = montarCiclo(dadosBasicos());
    const post = (payload) => app.inject({ method: 'POST', url: '/api/loja/ciclo/iniciar', headers: autorizacao(loja.token), payload });

    const versao = await post({ ...c.abrir, contratoVersao: 999 });
    expect(versao.statusCode).toBe(409);
    expect(versao.json().mensagem).toContain('incompatível');
    const hash = await post({ ...c.abrir, contratoHash: 'a'.repeat(64) });
    expect(hash.statusCode).toBe(409);

    const fora = await post({ ...c.abrir, tabelas: [...c.abrir.tabelas, { nome: 'config_caixa', linhas: 0, checksum: 'a'.repeat(64), colunas: ['id'] }] });
    expect(fora.statusCode).toBe(422);
    expect(fora.json().mensagem).toContain('fora do contrato');

    const cols = structuredClone(c.abrir);
    cols.tabelas[0].colunas = [...cols.tabelas[0].colunas, 'pin_hash'];
    const extra = await post(cols);
    expect(extra.statusCode).toBe(422);

    const faltando = await post({ ...c.abrir, tabelas: c.abrir.tabelas.slice(1) });
    expect(faltando.statusCode).toBe(422);
    expect(faltando.json().mensagem).toContain('não avisou');
  });

  it('um ciclo novo invalida o que ficou aberto, e não dá para concluir duas vezes', async () => {
    const c = montarCiclo(dadosBasicos());
    const a = await app.inject({ method: 'POST', url: '/api/loja/ciclo/iniciar', headers: autorizacao(loja.token), payload: c.abrir });
    const b = await app.inject({ method: 'POST', url: '/api/loja/ciclo/iniciar', headers: autorizacao(loja.token), payload: c.abrir });
    const antigo = await pool.query('SELECT status FROM painel.ciclos WHERE id = $1', [a.json().cicloId]);
    expect(antigo.rows[0].status).toBe('EXPIRADO');

    const r = await enviarCiclo(app, loja.token, dadosBasicos());
    expect(r.resp.statusCode).toBe(200);
    const denovo = await app.inject({ method: 'POST', url: `/api/loja/ciclo/${r.cicloId}/concluir`, headers: autorizacao(loja.token), payload: {} });
    expect(denovo.statusCode).toBe(409);
    expect(b.statusCode).toBe(200);
  });

  it('cancelar descarta a preparação', async () => {
    const c = montarCiclo(dadosBasicos());
    const ab = await app.inject({ method: 'POST', url: '/api/loja/ciclo/iniciar', headers: autorizacao(loja.token), payload: c.abrir });
    const id = ab.json().cicloId;
    const t = c.tabelas.find((x) => x.nome === 'vendas2');
    await app.inject({ method: 'PUT', url: `/api/loja/ciclo/${id}/tabela/vendas2`, headers: autorizacao(loja.token), payload: { lote: 0, colunas: t.colunas, linhas: t.linhas } });
    const r = await app.inject({ method: 'POST', url: `/api/loja/ciclo/${id}/cancelar`, headers: autorizacao(loja.token), payload: {} });
    expect(r.statusCode).toBe(200);
    expect((await pool.query('SELECT count(*)::int AS n FROM stg.vendas2')).rows[0].n).toBe(0);
    expect((await pool.query('SELECT status FROM painel.ciclos WHERE id = $1', [id])).rows[0].status).toBe('CANCELADO');
  });

  it('cada loja só mexe no espelho dela', async () => {
    const outra = await criarLoja(pool, 'Outra loja');
    await enviarCiclo(app, loja.token, dadosBasicos());
    const dela = dadosBasicos();
    dela.vendas2 = [linha('vendas2', { id: 1, total: '7.00', valor_final: '7.00', id_empresa: 1 })];
    await enviarCiclo(app, outra.token, dela);
    const a = await pool.query('SELECT count(*)::int AS n FROM espelho.vendas2 WHERE loja_id = $1', [loja.id]);
    const b = await pool.query('SELECT count(*)::int AS n FROM espelho.vendas2 WHERE loja_id = $1', [outra.id]);
    expect(a.rows[0].n).toBe(2);
    expect(b.rows[0].n).toBe(1);
    // e uma loja não enxerga o ciclo da outra
    const c = montarCiclo({});
    const ab = await app.inject({ method: 'POST', url: '/api/loja/ciclo/iniciar', headers: autorizacao(loja.token), payload: c.abrir });
    const intruso = await app.inject({ method: 'POST', url: `/api/loja/ciclo/${ab.json().cicloId}/cancelar`, headers: autorizacao(outra.token), payload: {} });
    expect(intruso.statusCode).toBe(404);
  });

  it('o espelho tem uma tabela para cada tabela do contrato', async () => {
    const r = await pool.query("SELECT table_name FROM information_schema.tables WHERE table_schema = 'espelho'");
    const nomes = new Set(r.rows.map((x) => x.table_name));
    for (const t of contrato.tabelas) expect(nomes.has(t.nome)).toBe(true);
  });
});
