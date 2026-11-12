import { beforeAll, afterAll, beforeEach, describe, expect, it } from 'vitest';
import { criarApp } from '../src/app.js';
import { configTeste, criarLoja, limpar, prepararBanco } from './ajuda.js';

const CHAVE = 'chave-de-teste-com-mais-de-24-caracteres';
let app, pool, loja;
const rpc = (chave, corpo) => app.inject({ method: 'POST', url: `/mcp/${chave}`, payload: corpo });

beforeAll(async () => {
  const b = await prepararBanco();
  pool = b.pool;
  app = criarApp({ pool, config: configTeste({ mcpDesignChave: CHAVE }) });
  await app.ready();
});
afterAll(async () => { await app.close(); await pool.end(); });
beforeEach(async () => {
  await limpar(pool);
  loja = await criarLoja(pool);
  await pool.query(
    `INSERT INTO espelho.produtos (loja_id, id, descricao, preco, preco_fornecedor, codigo_barras, un_comercial, quantidade, uso_loja)
     VALUES ($1, 1, 'PÃO FRANCÊS', 12.9, 5, '1', 'KG', 10, false), ($1, 2, 'SACOLA', 0.1, 0.05, '2', 'UN', 10, true)`, [loja.id]);
});

describe('MCP de produtos', () => {
  it('chave errada não acha a rota', async () => {
    expect((await rpc('errada', { jsonrpc: '2.0', id: 1, method: 'ping' })).statusCode).toBe(404);
  });
  it('lista ferramentas e busca sem acento, sem vazar custo e sem uso da loja', async () => {
    const l = (await rpc(CHAVE, { jsonrpc: '2.0', id: 1, method: 'tools/list' })).json();
    expect(l.result.tools.map((t) => t.name)).toContain('buscar_produtos');
    const r = (await rpc(CHAVE, { jsonrpc: '2.0', id: 2, method: 'tools/call', params: { name: 'buscar_produtos', arguments: { termo: 'pao' } } })).json();
    const t = r.result.content[0].text;
    expect(t).toContain('PÃO FRANCÊS');
    expect(t).toContain('R$ 12,90 /kg');
    expect(t).not.toMatch(/custo|fornecedor|quantidade/i);
    const s = (await rpc(CHAVE, { jsonrpc: '2.0', id: 3, method: 'tools/call', params: { name: 'buscar_produtos', arguments: { termo: 'sacola' } } })).json();
    expect(s.result.content[0].text).toContain('Nenhum produto');
  });
  it('notificação não tem resposta', async () => {
    expect((await rpc(CHAVE, { jsonrpc: '2.0', method: 'notifications/initialized' })).statusCode).toBe(202);
  });
});
