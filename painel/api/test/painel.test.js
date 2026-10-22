import { afterAll, beforeAll, beforeEach, describe, expect, it } from 'vitest';
import { configTeste, criarAppTeste, criarLoja, criarUsuario, enviarCiclo, limpar, linha, prepararBanco } from './ajuda.js';
import { codigoTotp, gerarSegredoTotp } from '../src/seguranca.js';

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

const H = { 'x-requested-with': 'painel' };

async function login(appUsado, usuario, extra = {}) {
  const r = await appUsado.inject({ method: 'POST', url: '/api/painel/login', headers: H, payload: { email: usuario.email, senha: usuario.senha, ...extra } });
  const cookie = (r.cookies ?? []).find((c) => c.name === 'sid');
  return { r, cookie: cookie ? `sid=${cookie.value}` : null };
}

describe('entrada no painel', () => {
  it('entra com senha certa, guarda a sessão em cookie httpOnly e reconhece o usuário', async () => {
    const u = await criarUsuario(pool);
    const { r, cookie } = await login(app, u);
    expect(r.statusCode).toBe(200);
    const sid = r.cookies.find((c) => c.name === 'sid');
    expect(sid.httpOnly).toBe(true);
    expect(sid.sameSite).toBe('Strict');
    const eu = await app.inject({ method: 'GET', url: '/api/painel/eu', headers: { cookie } });
    expect(eu.json().usuario).toMatchObject({ nome: 'Maria', perfil: 'gerente' });
    // o token da sessão nunca fica no banco em texto puro
    const guardado = await pool.query('SELECT id_hash FROM painel.sessoes');
    expect(guardado.rows[0].id_hash).not.toBe(sid.value);
    // sair apaga a sessão
    await app.inject({ method: 'POST', url: '/api/painel/logout', headers: { ...H, cookie } });
    expect((await app.inject({ method: 'GET', url: '/api/painel/eu', headers: { cookie } })).statusCode).toBe(401);
  });

  it('responde igual para e-mail errado e senha errada, e bloqueia depois de várias tentativas', async () => {
    const u = await criarUsuario(pool);
    const semUsuario = await login(app, { email: 'ninguem@exemplo.com', senha: 'x' });
    const senhaErrada = await login(app, { ...u, senha: 'errada-errada-1' });
    expect(semUsuario.r.statusCode).toBe(401);
    expect(senhaErrada.r.statusCode).toBe(401);
    expect(semUsuario.r.json().mensagem).toBe(senhaErrada.r.json().mensagem);

    for (let i = 0; i < 4; i++) await login(app, { ...u, senha: 'errada-errada-1' });
    const bloqueado = await login(app, u);                                 // até a senha certa fica bloqueada
    expect(bloqueado.r.statusCode).toBe(423);
    const eventos = await pool.query("SELECT acao FROM painel.auditoria WHERE acao = 'LOGIN_FALHOU'");
    expect(eventos.rowCount).toBeGreaterThanOrEqual(5);
  });

  it('com 2FA obrigatório exige o código certo', async () => {
    const appComDuasEtapas = criarAppTeste(pool, configTeste({ exigir2fa: true }));
    const segredo = gerarSegredoTotp();
    const u = await criarUsuario(pool);
    await pool.query('UPDATE painel.usuarios SET totp_segredo = $1, totp_ativo = true WHERE id = $2', [segredo, u.id]);

    expect((await login(appComDuasEtapas, u)).r.statusCode).toBe(401);                              // sem código
    expect((await login(appComDuasEtapas, u, { codigo: '000000' })).r.statusCode).toBe(401);       // código errado
    const ok = await login(appComDuasEtapas, u, { codigo: codigoTotp(segredo) });
    expect(ok.r.statusCode).toBe(200);

    const semConfig = await criarUsuario(pool, { email: 'sem2fa@exemplo.com', nome: 'Sem2fa' });
    const r = await login(appComDuasEtapas, semConfig, { codigo: '123456' });
    expect(r.r.statusCode).toBe(403);
    expect(r.r.json().mensagem).toContain('duas etapas');
    await appComDuasEtapas.close();
  });

  it('exige o cabeçalho de origem nas mudanças e sessão nas consultas', async () => {
    const u = await criarUsuario(pool);
    const semCabecalho = await app.inject({ method: 'POST', url: '/api/painel/login', payload: { email: u.email, senha: u.senha } });
    expect(semCabecalho.statusCode).toBe(403);
    expect((await app.inject({ method: 'GET', url: '/api/painel/resumo' })).statusCode).toBe(401);
    expect((await app.inject({ method: 'GET', url: '/api/painel/vendas', headers: { cookie: 'sid=inventado' } })).statusCode).toBe(401);
  });

  it('perfil "consulta" só consulta: não cria nem cancela pedido', async () => {
    const consulta = await criarUsuario(pool, { email: 'c@exemplo.com', nome: 'Só consulta', perfil: 'consulta' });
    const { cookie } = await login(app, consulta);
    const r = await app.inject({ method: 'POST', url: '/api/painel/pedidos', headers: { ...H, cookie }, payload: { tipo: 'CONTA_PAGAR_BAIXAR', payload: {} } });
    expect(r.statusCode).toBe(403);
    expect((await app.inject({ method: 'GET', url: '/api/painel/pedidos', headers: { cookie } })).statusCode).toBe(200);
  });
});

// ---------------------------------------------------------------------------------------------------------
// Consultas: um conjunto de dados pequeno, com números conferidos à mão
// ---------------------------------------------------------------------------------------------------------

async function hojeLoja() {
  return (await pool.query("SELECT (now() AT TIME ZONE 'America/Sao_Paulo')::date AS d")).rows[0].d;
}
const dia = (base, delta) => {
  const d = new Date(base);
  d.setUTCDate(d.getUTCDate() + delta);
  return d.toISOString().slice(0, 10);
};

async function carregarDados() {
  const h = await hojeLoja();
  const hoje = new Date(Date.UTC(h.getFullYear(), h.getMonth(), h.getDate())).toISOString().slice(0, 10);
  const t = (delta, hora = '10:00:00') => `${dia(hoje, delta)} ${hora}`;
  const dados = {
    empresas: [linha('empresas', { id: 1, apelido: 'Emassa', cnpj: '69133888000137', ativa: 1 }), linha('empresas', { id: 2, apelido: 'Studio', ativa: 1 })],
    operadores: [linha('operadores', { id: 1, nome: 'Nalva', ativo: 1, gerente: 1 }), linha('operadores', { id: 2, nome: 'Maria', ativo: 1, gerente: 0 })],
    clientes: [
      linha('clientes', { id: 2, nome: 'Maria da Silva', telefone: '7188887777', limite_credito: '200.00', ativo: 1 }),
      linha('clientes', { id: 3, nome: 'João Pereira', ativo: 1 }),
    ],
    vendas2: [
      linha('vendas2', { id: 1, data_hora: t(0), total: '100.00', valor_final: '100.00', forma_pagamento: 'Pix', esta_pago: 1, id_empresa: 1, id_operador_sessao: 2, id_caixa: 1 }),
      linha('vendas2', { id: 2, data_hora: t(0, '11:00:00'), total: '50.00', valor_final: '50.00', forma_pagamento: 'Dinheiro', esta_pago: 1, id_empresa: 1, id_operador_sessao: 2, id_caixa: 1 }),
      linha('vendas2', { id: 3, data_hora: t(-15), total: '180.00', valor_final: '180.00', forma_pagamento: 'Prazo', esta_pago: 0, id_cliente: 3, id_empresa: 1, id_operador_sessao: 1, id_caixa: 1 }),
      linha('vendas2', { id: 4, data_hora: t(-1), total: '70.00', valor_final: '70.00', forma_pagamento: 'Pix', esta_pago: 1, id_empresa: 1, id_operador_sessao: 2, id_caixa: 2 }),
      linha('vendas2', { id: 5, data_hora: t(0, '15:00:00'), total: '40.00', valor_final: '40.00', forma_pagamento: 'Cartão', esta_pago: 1, id_empresa: 2, id_operador_sessao: 1, id_caixa: 1 }),
    ],
    entradas_vendas: [linha('entradas_vendas', { id: 1, id_venda: 3, total: '80.00', data_hora: t(0, '12:00:00'), forma_pagamento: 'Pix' })],
    dividas: [
      linha('dividas', { id: 1, id_cliente: 2, id_empresa: 1, data_compra: dia(hoje, -20), descricao: 'Compras do mês', valor_total: '320.50', status: 'ABERTA' }),
      linha('dividas', { id: 2, id_cliente: 2, id_empresa: 1, data_compra: dia(hoje, -3), descricao: 'Arroz', valor_total: '90.00', status: 'ABERTA' }),
      linha('dividas', { id: 3, id_cliente: 3, id_empresa: 1, data_compra: dia(hoje, -5), descricao: 'Cancelada', valor_total: '999.00', status: 'CANCELADA' }),
    ],
    pagamentos_divida: [
      linha('pagamentos_divida', { id: 1, id_divida: 1, data_pagamento: t(0, '09:00:00'), valor: '100.00', forma_pagamento: 'Pix', cancelado: 0 }),
      linha('pagamentos_divida', { id: 2, id_divida: 2, data_pagamento: t(0, '09:30:00'), valor: '500.00', forma_pagamento: 'Pix', cancelado: 1 }),
    ],
    contas_pagar: [
      linha('contas_pagar', { id: 1, id_empresa: 1, descricao: 'Mercadoria', categoria: 'Fornecedor', valor: '1250.50', vencimento: dia(hoje, -4), status: 'ABERTA' }),
      linha('contas_pagar', { id: 2, id_empresa: 1, descricao: 'Aluguel', categoria: 'Aluguel', valor: '2800.00', vencimento: dia(hoje, 0), status: 'ABERTA' }),
      linha('contas_pagar', { id: 3, id_empresa: 1, descricao: 'Energia', categoria: 'Energia', valor: '430.90', vencimento: dia(hoje, 3), status: 'ABERTA' }),
      linha('contas_pagar', { id: 4, id_empresa: 1, descricao: 'Parcela', categoria: 'Fornecedor', valor: '500.00', vencimento: dia(hoje, 30), status: 'ABERTA' }),
      linha('contas_pagar', { id: 5, id_empresa: 1, descricao: 'Internet', valor: '150.00', vencimento: dia(hoje, -10), status: 'PAGA', valor_pago: '150.00', forma_pagamento: 'Pix' }),
      linha('contas_pagar', { id: 6, id_empresa: 2, descricao: 'Cosméticos', categoria: 'Fornecedor', valor: '900.00', vencimento: dia(hoje, -1), status: 'ABERTA' }),
    ],
    caixas: [
      linha('caixas', { id: 1, id_operador: 2, terminal: 'CAIXA-1', status: 'ABERTO', aberto_em: t(0, '08:00:00'), troco_inicial: '50.00' }),
      linha('caixas', { id: 2, id_operador: 1, terminal: 'CAIXA-2', status: 'FECHADO', aberto_em: t(-1, '08:00:00'), fechado_em: t(-1, '18:00:00'), troco_inicial: '20.00' }),
    ],
    fechamentos_caixa: [linha('fechamentos_caixa', { id: 1, id_caixa: 2, forma_pagamento: 'Dinheiro', valor_esperado: '100.00', valor_informado: '105.50', diferenca: '5.50' })],
    movimentacoes_caixa: [
      linha('movimentacoes_caixa', { id: 1, id_caixa: 1, tipo: 'CANCELAMENTO', valor: '25.00', id_operador_sessao: 2, data_hora: t(0, '13:00:00') }),
      linha('movimentacoes_caixa', { id: 2, id_caixa: 1, tipo: 'SANGRIA', valor: '30.00', data_hora: t(0, '14:00:00') }),
    ],
    sessoes_operador: [linha('sessoes_operador', { id: 1, id_operador: 2, nome_operador: 'Maria', terminal: 'CAIXA-1', entrada_em: t(0, '08:00:00'), saida_em: t(0, '12:00:00') })],
    auditoria_acesso: [linha('auditoria_acesso', { id: 1, data_hora: t(0, '13:00:00'), nome_operador: 'Nalva', terminal: 'CAIXA-2', acao: 'LIMITE_LIBERADO', detalhe: 'Cliente X' })],
    produtos: [
      linha('produtos', { id: 1, descricao: 'SEM ESTOQUE', quantidade: '0', preco: '10.00' }),
      linha('produtos', { id: 2, descricao: 'CAFÉ', quantidade: '3', preco: '20.00', preco_fornecedor: '12.00' }),
      linha('produtos', { id: 3, descricao: 'PARADO', quantidade: '10', preco: '5.00', preco_fornecedor: '2.00' }),
    ],
    produtos_vendidos: [linha('produtos_vendidos', { id: 1, id_produto: 2, id_venda: 1, quantidade: '2', preco_vendido: '20.00' })],
    notas_fiscais: [
      linha('notas_fiscais', { id: 1, cstat: '100', nnf: 1, serie: '1', modelo: '65', tp_amb: 1, valor_total: '100.00', dhemi: t(0), finalidade: 'NORMAL' }),
      linha('notas_fiscais', { id: 2, cstat: 'CONTINGENCIA', nnf: 2, serie: '1', modelo: '65', tp_amb: 1, valor_total: '50.00', dhemi: t(0, '11:00:00'), finalidade: 'NORMAL' }),
    ],
  };
  const r = await enviarCiclo(app, loja.token, dados);
  expect(r.resp.statusCode, JSON.stringify(r.resp.json())).toBe(200);
  return { hoje };
}

async function sessao() {
  const u = await criarUsuario(pool);
  return { cookie: (await login(app, u)).cookie };
}
const obter = async (cookie, url) => {
  const r = await app.inject({ method: 'GET', url, headers: { cookie } });
  expect(r.statusCode, `${url}: ${r.body}`).toBe(200);
  return r.json();
};

describe('consultas do painel', () => {
  it('visão geral: números conferidos à mão', async () => {
    await carregarDados();
    const { cookie } = await sessao();
    const r = await obter(cookie, '/api/painel/resumo');
    expect(r.vendasHoje).toMatchObject({ qtd: 3, total: 190, aPrazo: 0 });        // vendas 1, 2 e 5 (hoje)
    expect(r.vendasMes.qtd).toBeGreaterThanOrEqual(3);
    expect(r.recebidoHoje).toBe(370);          // à vista 190 + entrada a prazo 80 + pagamento de dívida 100 (o estornado não conta)
    expect(r.aReceber).toEqual({ total: 410.5, clientes: 2 });   // dívidas 220,50 + 90 e venda a prazo 180-80=100
    expect(r.aPagar.vencidas).toEqual({ qtd: 2, valor: 2150.5 });
    expect(r.aPagar.hoje).toEqual({ qtd: 1, valor: 2800 });
    expect(r.aPagar.proximos7Dias).toEqual({ qtd: 1, valor: 430.9 });
    expect(r.aPagar.abertas).toEqual({ qtd: 5, valor: 5881.4 });
    expect(r.caixasAbertos).toHaveLength(1);
    expect(r.caixasAbertos[0]).toMatchObject({ terminal: 'CAIXA-1', operador: 'Maria' });
    expect(r.produtosSemEstoque).toBe(1);

    const emp1 = await obter(cookie, '/api/painel/resumo?empresa=1');
    expect(emp1.vendasHoje).toMatchObject({ qtd: 2, total: 150 });
    expect(emp1.aPagar.vencidas).toEqual({ qtd: 1, valor: 1250.5 });
    expect(emp1.aPagar.abertas.qtd).toBe(4);
    const emp2 = await obter(cookie, '/api/painel/resumo?empresa=2');
    expect(emp2.vendasHoje).toMatchObject({ qtd: 1, total: 40 });
    expect(emp2.aReceber.total).toBe(0);
  });

  it('vendas por dia, forma, operador e hora', async () => {
    const { hoje } = await carregarDados();
    const { cookie } = await sessao();
    const porDia = await obter(cookie, `/api/painel/vendas?agrupar=dia&de=${dia(hoje, -30)}&ate=${hoje}`);
    const hojeItem = porDia.serie.find((x) => x.chave === hoje);
    expect(hojeItem).toMatchObject({ qtd: 3, total: 190 });
    const porForma = await obter(cookie, `/api/painel/vendas?agrupar=forma&de=${dia(hoje, -30)}&ate=${hoje}`);
    expect(porForma.serie.find((x) => x.chave === 'Prazo')).toMatchObject({ qtd: 1, total: 180, aPrazo: 180 });
    const porOperador = await obter(cookie, `/api/painel/vendas?agrupar=operador&de=${dia(hoje, -30)}&ate=${hoje}`);
    expect(porOperador.serie.find((x) => x.chave === 'Maria')).toMatchObject({ qtd: 3, total: 220 });
    const porHora = await obter(cookie, `/api/painel/vendas?agrupar=hora&de=${hoje}&ate=${hoje}`);
    expect(porHora.serie.map((x) => x.chave)).toEqual(['10', '11', '15']);
    const lista = await obter(cookie, `/api/painel/vendas/lista?de=${hoje}&ate=${hoje}`);
    expect(lista.vendas).toHaveLength(3);
    const ruim = await app.inject({ method: 'GET', url: '/api/painel/vendas?agrupar=nada', headers: { cookie } });
    expect(ruim.statusCode).toBe(422);
  });

  it('operadores: vendas, cancelamentos, horas logado e diferença de caixa', async () => {
    const { hoje } = await carregarDados();
    const { cookie } = await sessao();
    const r = await obter(cookie, `/api/painel/operadores?de=${dia(hoje, -30)}&ate=${hoje}`);
    const maria = r.operadores.find((o) => o.nome === 'Maria');
    expect(maria).toMatchObject({ vendas: 3, total: 220, cancelamentos: 1, valorCancelado: 25, entradas: 1, horasLogado: 4 });
    expect(maria.ticketMedio).toBeCloseTo(73.33, 2);
    const nalva = r.operadores.find((o) => o.nome === 'Nalva');
    expect(nalva).toMatchObject({ vendas: 2, total: 220, diferencaCaixas: 5.5, caixasFechados: 1 });
  });

  it('caixas: horários, vendas e diferença', async () => {
    const { hoje } = await carregarDados();
    const { cookie } = await sessao();
    const r = await obter(cookie, `/api/painel/caixas?de=${dia(hoje, -3)}&ate=${hoje}`);
    expect(r.caixas).toHaveLength(2);
    const aberto = r.caixas.find((c) => c.id === '1' || c.id === 1);
    expect(aberto).toMatchObject({ status: 'ABERTO', operador: 'Maria', vendas: 190, qtd_vendas: 4, sangrias: 30, troco_inicial: 50 });
    const fechado = r.caixas.find((c) => c.id === '2' || c.id === 2);
    expect(fechado).toMatchObject({ status: 'FECHADO', operador: 'Nalva', esperado: 100, contado: 105.5, diferenca: 5.5, minutos: 600 });
  });

  it('devedores: total por cliente, idade e faixas', async () => {
    await carregarDados();
    const { cookie } = await sessao();
    const r = await obter(cookie, '/api/painel/devedores');
    expect(r.total).toBe(410.5);
    expect(r.devedores.map((d) => d.nome)).toEqual(['Maria da Silva', 'João Pereira']);
    const maria = r.devedores[0];
    expect(maria).toMatchObject({ total: 310.5, limite: 200, acimaDoLimite: true, idadeDias: 20, telefone: '7188887777' });
    expect(maria.faixas).toMatchObject({ ate30: 310.5, de31a60: 0, mais90: 0 });
    const joao = r.devedores[1];
    expect(joao).toMatchObject({ total: 100, acimaDoLimite: false, idadeDias: 15 });
    expect(r.faixas.ate30).toBe(410.5);
  });

  it('contas a pagar: filtros e resumo por categoria', async () => {
    await carregarDados();
    const { cookie } = await sessao();
    const abertas = await obter(cookie, '/api/painel/contas-pagar?status=ABERTA');
    expect(abertas.contas).toHaveLength(5);
    expect(abertas.contas[0].descricao).toBe('Mercadoria');                   // ordenado por vencimento
    const vencidas = await obter(cookie, '/api/painel/contas-pagar?status=VENCIDA');
    expect(vencidas.contas.map((c) => c.descricao).sort()).toEqual(['Cosméticos', 'Mercadoria']);
    const pagas = await obter(cookie, '/api/painel/contas-pagar?status=PAGA');
    expect(pagas.contas[0]).toMatchObject({ descricao: 'Internet', valor_pago: 150 });
    expect((await obter(cookie, '/api/painel/contas-pagar?status=TODAS')).contas).toHaveLength(6);
    expect((await obter(cookie, '/api/painel/contas-pagar?status=ABERTA&empresa=2')).contas).toHaveLength(1);
    expect(abertas.porCategoria[0]).toMatchObject({ categoria: 'Aluguel', total: 2800 });
  });

  it('fluxo, estoque, fiscal, segurança, empresas e sincronização', async () => {
    const { hoje } = await carregarDados();
    const { cookie } = await sessao();
    const fluxo = await obter(cookie, '/api/painel/fluxo?dias=30');
    expect(fluxo.aReceberTotal).toBe(410.5);
    expect(fluxo.aPagarNoPeriodo).toBe(5881.4);                              // as 5 abertas (a de +30 dias cabe na janela)
    const estoque = await obter(cookie, '/api/painel/estoque?limite=5');
    expect(estoque.baixoEstoque.map((p) => p.descricao)).toEqual(['SEM ESTOQUE', 'CAFÉ']);
    expect(estoque.parados60Dias.map((p) => p.descricao)).toEqual(['PARADO']);
    expect(estoque.maisVendidos30Dias[0]).toMatchObject({ descricao: 'CAFÉ', quantidade: 2, total: 40 });
    expect(estoque.resumo.valorACusto).toBe(36 + 20);                       // 3x12 + 10x2
    const fiscal = await obter(cookie, `/api/painel/fiscal?de=${hoje}&ate=${hoje}`);
    expect(fiscal.emContingencia).toBe(1);
    expect(fiscal.grupos.find((g) => g.cstat === '100')).toMatchObject({ qtd: 1, total: 100 });
    const seg = await obter(cookie, `/api/painel/seguranca?de=${hoje}&ate=${hoje}`);
    expect(seg.cancelamentos).toEqual({ qtd: 1, total: 25 });
    expect(seg.eventos[0].acao).toBe('LIMITE_LIBERADO');
    const emp = await obter(cookie, '/api/painel/empresas');
    expect(emp.empresas.map((e) => e.apelido)).toEqual(['Emassa', 'Studio']);
    const sinc = await obter(cookie, '/api/painel/sincronizacao');
    expect(sinc.ciclos[0].status).toBe('CONCLUIDO');
  });

  it('status da loja: online pelo sinal recente, pedidos pendentes e hora da última atualização', async () => {
    await carregarDados();
    const { cookie } = await sessao();
    let st = await obter(cookie, '/api/painel/loja');
    expect(st.online).toBe(false);                                           // ainda sem sinal
    expect(st.atualizadoEm).not.toBeNull();
    await app.inject({ method: 'POST', url: '/api/loja/heartbeat', headers: { authorization: `Bearer ${loja.token}` }, payload: { terminal: 'CAIXA-1', versaoApp: '1.0', contratoVersao: 1 } });
    st = await obter(cookie, '/api/painel/loja');
    expect(st).toMatchObject({ online: true, terminal: 'CAIXA-1', pedidosPendentes: 0 });
  });
});

describe('busca de produtos', () => {
  it('acha por parte do nome (sem acento) ou código de barras e mostra os detalhes', async () => {
    await carregarDados();
    await pool.query("UPDATE espelho.produtos SET codigo_barras = '7891000100103', un_comercial = 'UN' WHERE id = 2");
    const { cookie } = await sessao();
    expect((await obter(cookie, '/api/painel/produtos?q=cafe')).produtos.map((p) => p.descricao)).toEqual(['CAFÉ']);
    expect((await obter(cookie, '/api/painel/produtos?q=7891000100103')).produtos[0].id).toBe('2');
    expect((await obter(cookie, '/api/painel/produtos?q=x')).produtos).toEqual([]);
    const d = await obter(cookie, '/api/painel/produtos?id=2');
    expect(d.produto).toMatchObject({ descricao: 'CAFÉ', quantidade: 3, preco: 20, custo: 12, margemPercentual: 66.7, valorEmEstoque: 60, codigoBarras: '7891000100103' });
    expect(d.vendas).toMatchObject({ qtd30: 2, total30: 40 });
    expect((await app.inject({ method: 'GET', url: '/api/painel/produtos?id=999', headers: { cookie } })).statusCode).toBe(404);
  });
});

describe('itens de lanche (códigos 200 a 500) ficam fora do estoque', () => {
  it('não entram em valor de estoque, estoque baixo, parados nem "sem estoque", e aparecem à parte', async () => {
    const { hoje } = await carregarDados();
    const L = loja.id;
    // um lanche com estoque de mentira (9999) vendido 3x, e outro lanche com quantidade 0
    await pool.query(
      `INSERT INTO espelho.produtos (loja_id, id, descricao, quantidade, preco, codigo_barras, preco_fornecedor) VALUES
         ($1, 50, 'X-BURGUER', '9999', '10.00', '205', '4.00'), ($1, 51, 'CAFE PRETO', '0', '2.50', '300', '1.00'), ($1, 52, 'PRODUTO COMUM 200L', '5', '3.00', '2000', '1.00')`, [L]);
    await pool.query(
      `INSERT INTO espelho.produtos_vendidos (loja_id, id, id_produto, id_venda, quantidade, preco_vendido) VALUES ($1, 90, 50, 1, '3', '10.00')`, [L]);
    const { cookie } = await sessao();

    const e = await obter(cookie, '/api/painel/estoque?limite=100000');
    expect(e.resumo.produtos).toBe(4);                              // os 3 de antes + o "2000" (não é lanche); lanches não contam
    expect(e.resumo.valorACusto).toBe(36 + 20 + 5);                  // sem os 9999 × 4,00 do lanche; o produto comum de 5 un. a 1,00 entra
    expect(e.baixoEstoque.map((p) => p.descricao)).not.toContain('X-BURGUER');
    expect(e.baixoEstoque.map((p) => p.descricao)).not.toContain('CAFE PRETO');
    expect(e.parados60Dias.map((p) => p.descricao)).not.toContain('X-BURGUER');
    expect(e.maisVendidos30Dias.map((p) => p.descricao)).not.toContain('X-BURGUER');
    expect(e.lanches.produtos).toBe(2);
    expect(e.lanches.faixa).toEqual({ de: 200, ate: 500 });
    expect(e.lanches.faturamento30Dias).toBe(30);
    expect(e.lanches.maisVendidos30Dias[0]).toMatchObject({ descricao: 'X-BURGUER', codigoBarras: '205', quantidade: 3, total: 30 });

    const resumo = await obter(cookie, '/api/painel/resumo');
    expect(resumo.produtosSemEstoque).toBe(1);                       // só o "SEM ESTOQUE" de antes; o lanche com 0 não conta

    const porTipo = await obter(cookie, `/api/painel/vendas?agrupar=tipo&de=${dia(hoje, -30)}&ate=${hoje}`);
    expect(porTipo.serie.find((x) => x.chave === 'Lanches')).toMatchObject({ total: 30 });
    expect(porTipo.serie.find((x) => x.chave === 'Mercadorias')).toMatchObject({ total: 40 });

    const detalhe = await obter(cookie, '/api/painel/produtos?id=50');
    expect(detalhe.produto.lanche).toBe(true);
    expect((await obter(cookie, '/api/painel/produtos?id=52')).produto.lanche).toBe(false);   // "2000" está fora da faixa
  });
});
