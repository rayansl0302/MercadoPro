import { afterAll, beforeAll, beforeEach, describe, expect, it } from 'vitest';
import { criarAppTeste, criarLoja, criarUsuario, limpar, prepararBanco } from './ajuda.js';
import { conferirTotp, codigoTotp } from '../src/seguranca.js';

let pool;
let app;

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
  await criarLoja(pool);
});

const H = { 'x-requested-with': 'painel' };

async function login(usuario, extra = {}) {
  const r = await app.inject({ method: 'POST', url: '/api/painel/login', headers: H, payload: { email: usuario.email, senha: usuario.senha, ...extra } });
  const c = (r.cookies ?? []).find((x) => x.name === 'sid');
  return { r, cookie: c ? `sid=${c.value}` : null, set: c };
}
const post = (cookie, url, payload = {}) => app.inject({ method: 'POST', url, headers: { ...H, cookie }, payload });
const get = (cookie, url) => app.inject({ method: 'GET', url, headers: { cookie } });

describe('manter conectado', () => {
  it('com "manter conectado" a sessão dura 30 dias; sem marcar, é curta e o cookie some ao fechar o navegador', async () => {
    const u = await criarUsuario(pool);
    const curta = await login(u);
    expect(curta.set.maxAge).toBeUndefined();
    const longa = await login(u, { lembrar: true });
    expect(longa.set.maxAge).toBe(30 * 24 * 3600);
    const r = await pool.query("SELECT extract(epoch FROM (expira_em - criada_em)) / 3600 AS horas FROM painel.sessoes ORDER BY criada_em");
    expect(Math.round(r.rows[0].horas)).toBe(12);
    expect(Math.round(r.rows[1].horas)).toBe(30 * 24);
  });
});

describe('alterar a própria senha', () => {
  it('confere a senha atual, exige senha forte, desconecta os outros aparelhos e vale no próximo login', async () => {
    const u = await criarUsuario(pool);
    const a = await login(u);
    const b = await login(u);
    expect((await post(a.cookie, '/api/painel/senha', { atual: 'errada-errada1', nova: 'Nova-Senha-456' })).statusCode).toBe(422);
    expect((await post(a.cookie, '/api/painel/senha', { atual: u.senha, nova: 'curta1' })).statusCode).toBe(422);
    expect((await post(a.cookie, '/api/painel/senha', { atual: u.senha, nova: u.senha })).statusCode).toBe(422);
    const ok = await post(a.cookie, '/api/painel/senha', { atual: u.senha, nova: 'Nova-Senha-456' });
    expect(ok.statusCode).toBe(200);
    expect((await get(a.cookie, '/api/painel/eu')).statusCode).toBe(200);   // este aparelho continua
    expect((await get(b.cookie, '/api/painel/eu')).statusCode).toBe(401);   // o outro caiu
    expect((await login(u)).r.statusCode).toBe(401);                         // senha antiga não entra mais
    expect((await login({ ...u, senha: 'Nova-Senha-456' })).r.statusCode).toBe(200);
    expect((await pool.query("SELECT 1 FROM painel.auditoria WHERE acao = 'SENHA_ALTERADA'")).rowCount).toBe(1);
  });
});

describe('administração de usuários', () => {
  it('só admin gerencia; o novo usuário recebe senha temporária e 2FA e é obrigado a trocar a senha', async () => {
    const admin = await criarUsuario(pool, { email: 'admin@exemplo.com', nome: 'Dona', perfil: 'admin' });
    const gerente = await criarUsuario(pool, { email: 'g@exemplo.com', nome: 'Gerente', perfil: 'gerente' });
    const adm = await login(admin);
    const ger = await login(gerente);

    expect((await get(ger.cookie, '/api/painel/usuarios')).statusCode).toBe(403);
    expect((await post(ger.cookie, '/api/painel/usuarios', { nome: 'X Y', email: 'x@y.com', perfil: 'consulta' })).statusCode).toBe(403);

    expect((await post(adm.cookie, '/api/painel/usuarios', { nome: 'Joana', email: 'nao-e-email', perfil: 'consulta' })).statusCode).toBe(422);
    expect((await post(adm.cookie, '/api/painel/usuarios', { nome: 'Joana', email: 'joana@exemplo.com', perfil: 'rei' })).statusCode).toBe(422);
    const criado = await post(adm.cookie, '/api/painel/usuarios', { nome: 'Joana', email: 'Joana@Exemplo.com', perfil: 'gerente' });
    expect(criado.statusCode).toBe(200);
    const novo = criado.json();
    expect(novo.senhaTemporaria.length).toBeGreaterThanOrEqual(10);
    expect(novo.otpauth).toContain('issuer=MercadoPro');
    expect(conferirTotp(novo.segredo, codigoTotp(novo.segredo))).toBe(true);
    expect((await post(adm.cookie, '/api/painel/usuarios', { nome: 'Outra', email: 'joana@exemplo.com', perfil: 'consulta' })).statusCode).toBe(409);

    // a senha temporária entra, mas só deixa trocar a senha
    const j = await login({ email: 'joana@exemplo.com', senha: novo.senhaTemporaria });
    expect(j.r.statusCode).toBe(200);
    expect(j.r.json().usuario.trocarSenha).toBe(true);
    const bloqueado = await get(j.cookie, '/api/painel/resumo');
    expect(bloqueado.statusCode).toBe(403);
    expect(bloqueado.json().detalhe.codigo).toBe('TROCAR_SENHA');
    expect((await get(j.cookie, '/api/painel/eu')).statusCode).toBe(200);
    expect((await post(j.cookie, '/api/painel/senha', { atual: novo.senhaTemporaria, nova: 'Senha-Da-Joana-9' })).statusCode).toBe(200);
    expect((await get(j.cookie, '/api/painel/resumo')).statusCode).toBe(200);
    expect((await get(adm.cookie, '/api/painel/usuarios')).json().usuarios.find((x) => x.email === 'joana@exemplo.com').trocarSenha).toBe(false);
  });

  it('desativar ou mudar o perfil derruba as sessões; redefinir senha e 2FA gera dados novos; nunca fica sem administrador', async () => {
    const admin = await criarUsuario(pool, { email: 'admin@exemplo.com', nome: 'Dona', perfil: 'admin' });
    const maria = await criarUsuario(pool, { email: 'maria@exemplo.com', nome: 'Maria', perfil: 'gerente' });
    const adm = await login(admin);
    const mar = await login(maria);
    const idMaria = (await get(adm.cookie, '/api/painel/usuarios')).json().usuarios.find((x) => x.email === 'maria@exemplo.com').id;
    const idAdmin = (await get(adm.cookie, '/api/painel/usuarios')).json().usuarios.find((x) => x.email === 'admin@exemplo.com').id;

    // o único administrador não pode se rebaixar nem se desativar
    expect((await post(adm.cookie, `/api/painel/usuarios/${idAdmin}`, { perfil: 'consulta' })).statusCode).toBe(409);
    expect((await post(adm.cookie, `/api/painel/usuarios/${idAdmin}`, { ativo: false })).statusCode).toBe(409);

    expect((await post(adm.cookie, `/api/painel/usuarios/${idMaria}`, { perfil: 'consulta' })).statusCode).toBe(200);
    expect((await get(mar.cookie, '/api/painel/eu')).statusCode).toBe(401);
    const m2 = await login(maria);
    expect(m2.r.json().usuario.perfil).toBe('consulta');
    expect((await post(m2.cookie, '/api/painel/pedidos', { tipo: 'CONTA_PAGAR_BAIXAR', payload: {} })).statusCode).toBe(403);

    const nova = await post(adm.cookie, `/api/painel/usuarios/${idMaria}/redefinir-senha`);
    expect(nova.statusCode).toBe(200);
    expect((await login(maria)).r.statusCode).toBe(401);                                  // a antiga não vale mais
    expect((await login({ ...maria, senha: nova.json().senhaTemporaria })).r.json().usuario.trocarSenha).toBe(true);

    const d2 = await post(adm.cookie, `/api/painel/usuarios/${idMaria}/redefinir-2fa`);
    expect(d2.json().otpauth).toContain('otpauth://totp/');

    await post(adm.cookie, `/api/painel/usuarios/${idMaria}`, { ativo: false });
    expect((await login({ ...maria, senha: nova.json().senhaTemporaria })).r.statusCode).toBe(401);   // desativada não entra

    // com dois administradores, um pode sair
    await post(adm.cookie, `/api/painel/usuarios/${idMaria}`, { ativo: true, perfil: 'admin' });
    expect((await post(adm.cookie, `/api/painel/usuarios/${idAdmin}`, { perfil: 'gerente' })).statusCode).toBe(200);
    expect((await pool.query("SELECT count(*)::int AS n FROM painel.auditoria WHERE acao LIKE 'USUARIO_%' OR acao LIKE '%REDEFINIDA'")).rows[0].n).toBeGreaterThanOrEqual(5);
  });

  it('o administrador (e o gerente) podem pedir ações; consulta não', async () => {
    const admin = await criarUsuario(pool, { email: 'admin@exemplo.com', nome: 'Dona', perfil: 'admin' });
    const { cookie } = await login(admin);
    const r = await post(cookie, '/api/painel/pedidos', { tipo: 'CONTA_PAGAR_BAIXAR', payload: { contaId: 1 } });
    expect(r.statusCode).not.toBe(403);          // chega até a validação do pedido
  });
});
