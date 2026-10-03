import { afterAll, beforeAll, beforeEach, describe, expect, it } from 'vitest';
import { autorizacao, configTeste, criarAppTeste, criarLoja, criarUsuario, enviarCiclo, limpar, linha, prepararBanco } from './ajuda.js';
import { cancelarPedido, criarPedido, entregarPedidos, expirarPedidos, registrarResultado, validarPayload } from '../src/pedidos.js';

let pool;
let app;
let loja;
let usuario;
const config = configTeste();

const hojeIso = () => new Date().toISOString().slice(0, 10);

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
  usuario = await criarUsuario(pool);
  // espelho com uma conta em aberto, uma paga e uma empresa
  await enviarCiclo(app, loja.token, {
    empresas: [linha('empresas', { id: 1, apelido: 'Empresa A', ativa: 1 }), linha('empresas', { id: 2, apelido: 'Desativada', ativa: 0 })],
    contas_pagar: [
      linha('contas_pagar', { id: 10, id_empresa: 1, descricao: 'Aluguel', valor: '1200.00', vencimento: '2026-10-10', status: 'ABERTA' }),
      linha('contas_pagar', { id: 11, id_empresa: 1, descricao: 'Luz', valor: '300.00', vencimento: '2026-10-05', status: 'PAGA' }),
    ],
  });
});

const baixa = (extra = {}) => ({ contaId: 10, valorPago: 1210.5, forma: 'Pix', data: hojeIso(), ...extra });

describe('validação dos pedidos', () => {
  it('aceita pedidos válidos e converte valores', () => {
    const v = validarPayload('CONTA_PAGAR_BAIXAR', baixa({ valorPago: '1.210,50'.replace('.', '') }));
    expect(v.valorPago).toBe(1210.5);
  });
  it('recusa dados ruins com mensagem em português', () => {
    expect(() => validarPayload('CONTA_PAGAR_BAIXAR', baixa({ valorPago: 0 }))).toThrow(/Pedido inválido/);
    expect(() => validarPayload('CONTA_PAGAR_BAIXAR', baixa({ forma: 'Bitcoin' }))).toThrow(/forma/);
    expect(() => validarPayload('CONTA_PAGAR_BAIXAR', baixa({ data: '10/10/2026' }))).toThrow(/Data/);
    expect(() => validarPayload('CONTA_PAGAR_BAIXAR', { contaId: 'x' })).toThrow();
    expect(() => validarPayload('APAGAR_TUDO', {})).toThrow(/desconhecido/);
    expect(() => validarPayload('CONTA_PAGAR_LANCAR', { descricao: 'x', valor: 10, vencimento: hojeIso(), idEmpresa: 1 })).toThrow();   // descrição curta
    expect(() => validarPayload('CONTA_PAGAR_LANCAR', { descricao: 'Aluguel', valor: 10, vencimento: hojeIso(), idEmpresa: 1, parcelas: 500 })).toThrow();
  });
});

describe('fila de pedidos', () => {
  it('cria, entrega, recebe o resultado e não deixa responder duas vezes', async () => {
    const { pedido, repetido } = await criarPedido(pool, config, { loja, usuario, tipo: 'CONTA_PAGAR_BAIXAR', payload: baixa(), chave: 'abc' });
    expect(repetido).toBe(false);
    expect(pedido.status).toBe('PENDENTE');
    expect(pedido.estado_visto.statusVisto).toBe('ABERTA');

    const lista = await entregarPedidos(pool, loja);
    expect(lista).toHaveLength(1);
    expect(lista[0]).toMatchObject({ id: pedido.id, tipo: 'CONTA_PAGAR_BAIXAR', solicitante: 'Maria' });
    expect(new Date(lista[0].expiraEm) > new Date()).toBe(true);
    expect((await entregarPedidos(pool, loja))).toHaveLength(1);         // sem resultado, volta a ser entregue

    await registrarResultado(pool, loja, pedido.id, { status: 'APLICADO', mensagem: 'Conta baixada.', detalhe: { contaId: 10 } });
    expect(await entregarPedidos(pool, loja)).toHaveLength(0);
    await expect(registrarResultado(pool, loja, pedido.id, { status: 'APLICADO' })).rejects.toMatchObject({ status: 409 });
    const salvo = await pool.query('SELECT status, resultado_mensagem, resultado_detalhe FROM painel.pedidos WHERE id = $1', [pedido.id]);
    expect(salvo.rows[0]).toMatchObject({ status: 'APLICADO', resultado_mensagem: 'Conta baixada.' });
    expect(salvo.rows[0].resultado_detalhe).toEqual({ contaId: 10 });
  });

  it('a mesma chave devolve o mesmo pedido (duplo clique não duplica)', async () => {
    const a = await criarPedido(pool, config, { loja, usuario, tipo: 'CONTA_PAGAR_BAIXAR', payload: baixa(), chave: 'clique' });
    const b = await criarPedido(pool, config, { loja, usuario, tipo: 'CONTA_PAGAR_BAIXAR', payload: baixa(), chave: 'clique' });
    expect(b.repetido).toBe(true);
    expect(b.pedido.id).toBe(a.pedido.id);
    expect((await pool.query('SELECT count(*)::int AS n FROM painel.pedidos')).rows[0].n).toBe(1);
  });

  it('não deixa pedir baixa de conta que não existe, já paga, ou lançamento em empresa desativada', async () => {
    await expect(criarPedido(pool, config, { loja, usuario, tipo: 'CONTA_PAGAR_BAIXAR', payload: baixa({ contaId: 999 }) })).rejects.toMatchObject({ status: 404 });
    await expect(criarPedido(pool, config, { loja, usuario, tipo: 'CONTA_PAGAR_BAIXAR', payload: baixa({ contaId: 11 }) })).rejects.toMatchObject({ status: 409 });
    const lancar = { descricao: 'Internet', valor: 150, vencimento: hojeIso(), idEmpresa: 2 };
    await expect(criarPedido(pool, config, { loja, usuario, tipo: 'CONTA_PAGAR_LANCAR', payload: lancar })).rejects.toMatchObject({ status: 422 });
    const ok = await criarPedido(pool, config, { loja, usuario, tipo: 'CONTA_PAGAR_LANCAR', payload: { ...lancar, idEmpresa: 1, parcelas: 3 } });
    expect(ok.pedido.payload).toMatchObject({ descricao: 'Internet', parcelas: 3, periodicidade: 'MENSAL', idEmpresa: 1 });
  });

  it('pedido vencido expira e deixa de ser entregue; só dá para cancelar o que não foi buscado', async () => {
    const vencido = await criarPedido(pool, config, { loja, usuario, tipo: 'CONTA_PAGAR_BAIXAR', payload: baixa() });
    await pool.query("UPDATE painel.pedidos SET expira_em = now() - interval '1 minute' WHERE id = $1", [vencido.pedido.id]);
    expect(await entregarPedidos(pool, loja)).toHaveLength(0);
    expect((await pool.query('SELECT status FROM painel.pedidos WHERE id = $1', [vencido.pedido.id])).rows[0].status).toBe('EXPIRADO');
    expect(await expirarPedidos(pool)).toBe(0);

    const novo = await criarPedido(pool, config, { loja, usuario, tipo: 'CONTA_PAGAR_BAIXAR', payload: baixa({ valorPago: 5 }) });
    await cancelarPedido(pool, usuario, novo.pedido.id);
    expect((await pool.query('SELECT status FROM painel.pedidos WHERE id = $1', [novo.pedido.id])).rows[0].status).toBe('CANCELADO');
    expect(await entregarPedidos(pool, loja)).toHaveLength(0);

    const entregue = await criarPedido(pool, config, { loja, usuario, tipo: 'CONTA_PAGAR_BAIXAR', payload: baixa({ valorPago: 6 }) });
    await entregarPedidos(pool, loja);
    await expect(cancelarPedido(pool, usuario, entregue.pedido.id)).rejects.toMatchObject({ status: 409 });
  });

  it('uma loja não responde pedido de outra, e status inválido é recusado', async () => {
    const outra = await criarLoja(pool, 'Outra');
    const { pedido } = await criarPedido(pool, config, { loja, usuario, tipo: 'CONTA_PAGAR_BAIXAR', payload: baixa() });
    await expect(registrarResultado(pool, outra, pedido.id, { status: 'APLICADO' })).rejects.toMatchObject({ status: 404 });
    await expect(registrarResultado(pool, loja, pedido.id, { status: 'QUALQUER' })).rejects.toMatchObject({ status: 422 });
    await expect(registrarResultado(pool, loja, 'nao-e-uuid', { status: 'APLICADO' })).rejects.toMatchObject({ status: 404 });
  });
});

describe('pedidos pelas rotas da loja', () => {
  it('a loja busca e responde com o token; sem token é 401', async () => {
    const { pedido } = await criarPedido(pool, config, { loja, usuario, tipo: 'CONTA_PAGAR_BAIXAR', payload: baixa() });
    expect((await app.inject({ method: 'GET', url: '/api/loja/pedidos' })).statusCode).toBe(401);
    expect((await app.inject({ method: 'GET', url: '/api/loja/pedidos', headers: autorizacao('token-errado') })).statusCode).toBe(401);
    const lista = await app.inject({ method: 'GET', url: '/api/loja/pedidos', headers: autorizacao(loja.token) });
    expect(lista.json().pedidos[0].id).toBe(pedido.id);
    const res = await app.inject({ method: 'POST', url: `/api/loja/pedidos/${pedido.id}/resultado`, headers: autorizacao(loja.token), payload: { status: 'CONFLITO', mensagem: 'Já estava paga' } });
    expect(res.statusCode).toBe(200);
    const denovo = await app.inject({ method: 'POST', url: `/api/loja/pedidos/${pedido.id}/resultado`, headers: autorizacao(loja.token), payload: { status: 'APLICADO' } });
    expect(denovo.statusCode).toBe(409);
  });
});
