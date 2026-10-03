import { conferirSenha, gerarSegredoTotp, gerarSenhaTemporaria, hashSenha, hashToken, senhaForte, urlOtpauth } from './seguranca.js';
import { ErroHttp } from './ciclo.js';

export const PERFIS = ['consulta', 'gerente', 'admin'];

const auditar = (pool, ator, acao, detalhe, ip) =>
  pool.query('INSERT INTO painel.auditoria (usuario_id, usuario, acao, detalhe, ip) VALUES ($1, $2, $3, $4, $5)', [ator?.id ?? null, ator?.nome ?? null, acao, detalhe ?? null, ip ?? null]);

// ------------------------------------------------------------------ a própria conta
export async function alterarMinhaSenha(pool, usuario, tokenAtual, { atual, nova }, ip) {
  const r = await pool.query('SELECT senha_hash FROM painel.usuarios WHERE id = $1', [usuario.id]);
  if (!(await conferirSenha(String(atual ?? ''), r.rows[0].senha_hash))) throw new ErroHttp(422, 'A senha atual está incorreta.');
  if (!senhaForte(nova)) throw new ErroHttp(422, 'A nova senha precisa ter pelo menos 10 caracteres, com letras e números.');
  if (nova === atual) throw new ErroHttp(422, 'A nova senha precisa ser diferente da atual.');
  await pool.query('UPDATE painel.usuarios SET senha_hash = $2, trocar_senha = false, senha_trocada_em = now() WHERE id = $1', [usuario.id, await hashSenha(nova)]);
  // quem tinha a senha antiga é desconectado; esta sessão continua
  await pool.query('DELETE FROM painel.sessoes WHERE usuario_id = $1 AND id_hash <> $2', [usuario.id, hashToken(tokenAtual ?? '')]);
  await auditar(pool, usuario, 'SENHA_ALTERADA', null, ip);
  return { ok: true };
}

// ------------------------------------------------------------------ administração de usuários (perfil admin)
export async function listarUsuarios(pool) {
  const r = await pool.query(
    `SELECT u.id, u.nome, u.email, u.perfil, u.ativo, u.totp_ativo AS "doisFatores", u.trocar_senha AS "trocarSenha", u.criado_em AS "criadoEm",
            (u.bloqueado_ate IS NOT NULL AND u.bloqueado_ate > now()) AS bloqueado,
            (SELECT max(criada_em) FROM painel.sessoes s WHERE s.usuario_id = u.id) AS "ultimoAcesso"
     FROM painel.usuarios u ORDER BY u.ativo DESC, lower(u.nome)`,
  );
  return r.rows;
}

const normalizarEmail = (e) => String(e ?? '').trim().toLowerCase();
const emailValido = (e) => /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(e) && e.length <= 200;

export async function criarUsuario(pool, ator, { nome, email, perfil }, ip) {
  const n = String(nome ?? '').trim();
  const e = normalizarEmail(email);
  if (n.length < 2 || n.length > 100) throw new ErroHttp(422, 'Informe o nome (2 a 100 letras).');
  if (!emailValido(e)) throw new ErroHttp(422, 'E-mail inválido.');
  if (!PERFIS.includes(perfil)) throw new ErroHttp(422, 'Perfil inválido.');
  if ((await pool.query('SELECT 1 FROM painel.usuarios WHERE lower(email) = $1', [e])).rowCount) throw new ErroHttp(409, 'Já existe um usuário com esse e-mail.');
  const senha = gerarSenhaTemporaria();
  const segredo = gerarSegredoTotp();
  const r = await pool.query(
    `INSERT INTO painel.usuarios (nome, email, senha_hash, perfil, totp_segredo, totp_ativo, trocar_senha) VALUES ($1, $2, $3, $4, $5, true, true) RETURNING id`,
    [n, e, await hashSenha(senha), perfil, segredo],
  );
  await auditar(pool, ator, 'USUARIO_CRIADO', `${e} (${perfil})`, ip);
  return { id: r.rows[0].id, nome: n, email: e, perfil, senhaTemporaria: senha, segredo, otpauth: urlOtpauth(segredo, e) };
}

async function buscar(pool, id) {
  const r = await pool.query('SELECT id, nome, email, perfil, ativo FROM painel.usuarios WHERE id = $1', [Number(id) || 0]);
  if (!r.rowCount) throw new ErroHttp(404, 'Usuário não encontrado.');
  return r.rows[0];
}

// nunca deixa o painel sem nenhum administrador ativo
async function garantirOutroAdmin(pool, id) {
  const r = await pool.query("SELECT count(*)::int AS n FROM painel.usuarios WHERE perfil = 'admin' AND ativo AND id <> $1", [id]);
  if (r.rows[0].n === 0) throw new ErroHttp(409, 'O painel precisa ter ao menos um administrador ativo.');
}

export async function atualizarUsuario(pool, ator, id, { nome, perfil, ativo }, ip) {
  const alvo = await buscar(pool, id);
  const novoPerfil = perfil ?? alvo.perfil;
  const novoAtivo = ativo ?? alvo.ativo;
  if (!PERFIS.includes(novoPerfil)) throw new ErroHttp(422, 'Perfil inválido.');
  const n = nome === undefined ? alvo.nome : String(nome).trim();
  if (n.length < 2 || n.length > 100) throw new ErroHttp(422, 'Informe o nome (2 a 100 letras).');
  const perdeAdmin = alvo.perfil === 'admin' && alvo.ativo && (novoPerfil !== 'admin' || !novoAtivo);
  if (perdeAdmin) await garantirOutroAdmin(pool, alvo.id);
  await pool.query('UPDATE painel.usuarios SET nome = $2, perfil = $3, ativo = $4 WHERE id = $1', [alvo.id, n, novoPerfil, novoAtivo]);
  if (!novoAtivo || novoPerfil !== alvo.perfil) await pool.query('DELETE FROM painel.sessoes WHERE usuario_id = $1', [alvo.id]);
  await auditar(pool, ator, 'USUARIO_ALTERADO', `${alvo.email}: perfil ${novoPerfil}, ${novoAtivo ? 'ativo' : 'desativado'}`, ip);
  return { ok: true };
}

export async function redefinirSenha(pool, ator, id, ip) {
  const alvo = await buscar(pool, id);
  const senha = gerarSenhaTemporaria();
  await pool.query('UPDATE painel.usuarios SET senha_hash = $2, trocar_senha = true, falhas_login = 0, bloqueado_ate = NULL WHERE id = $1', [alvo.id, await hashSenha(senha)]);
  await pool.query('DELETE FROM painel.sessoes WHERE usuario_id = $1', [alvo.id]);
  await auditar(pool, ator, 'SENHA_REDEFINIDA', alvo.email, ip);
  return { email: alvo.email, senhaTemporaria: senha };
}

export async function redefinirDoisFatores(pool, ator, id, ip) {
  const alvo = await buscar(pool, id);
  const segredo = gerarSegredoTotp();
  await pool.query('UPDATE painel.usuarios SET totp_segredo = $2, totp_ativo = true WHERE id = $1', [alvo.id, segredo]);
  await pool.query('DELETE FROM painel.sessoes WHERE usuario_id = $1', [alvo.id]);
  await auditar(pool, ator, 'DOIS_FATORES_REDEFINIDO', alvo.email, ip);
  return { email: alvo.email, segredo, otpauth: urlOtpauth(segredo, alvo.email) };
}
