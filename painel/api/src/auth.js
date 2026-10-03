import { conferirSenha, conferirTotp, gerarToken, hashSenha, hashToken } from './seguranca.js';
import { ErroHttp } from './ciclo.js';

// senha de mentira para gastar o mesmo tempo quando o e-mail não existe (não revela quem tem conta)
let senhaFalsa;
async function hashFalso() {
  senhaFalsa ??= await hashSenha('senha-falsa-para-igualar-o-tempo');
  return senhaFalsa;
}

export async function entrar(pool, config, { email, senha, codigo, ip }) {
  const mensagemGenerica = 'E-mail, senha ou código incorretos.';
  const r = await pool.query('SELECT * FROM painel.usuarios WHERE lower(email) = lower($1)', [String(email ?? '').trim()]);
  const usuario = r.rows[0];

  if (usuario?.bloqueado_ate && usuario.bloqueado_ate > new Date()) {
    throw new ErroHttp(423, 'Muitas tentativas. Aguarde alguns minutos e tente de novo.');
  }
  const senhaOk = await conferirSenha(String(senha ?? ''), usuario?.senha_hash ?? (await hashFalso()));
  let ok = Boolean(usuario) && usuario.ativo && senhaOk;

  if (ok && config.exigir2fa) {
    if (!usuario.totp_ativo || !usuario.totp_segredo) {
      throw new ErroHttp(403, 'A verificação em duas etapas ainda não foi configurada para este usuário. Peça ao administrador.');
    }
    ok = conferirTotp(usuario.totp_segredo, codigo);
  }

  if (!ok) {
    if (usuario) {
      const falhas = usuario.falhas_login + 1;
      const bloquear = falhas >= config.loginMaxTentativas;
      await pool.query(
        'UPDATE painel.usuarios SET falhas_login = $2, bloqueado_ate = CASE WHEN $3 THEN now() + make_interval(mins => $4) ELSE bloqueado_ate END WHERE id = $1',
        [usuario.id, bloquear ? 0 : falhas, bloquear, config.loginBloqueioMinutos],
      );
      await pool.query('INSERT INTO painel.auditoria (usuario_id, usuario, acao, detalhe, ip) VALUES ($1, $2, $3, $4, $5)', [
        usuario.id, usuario.nome, 'LOGIN_FALHOU', bloquear ? 'Conta bloqueada por tentativas' : 'Senha ou código incorretos', ip ?? null,
      ]);
    }
    throw new ErroHttp(401, mensagemGenerica);
  }

  await pool.query('UPDATE painel.usuarios SET falhas_login = 0, bloqueado_ate = NULL WHERE id = $1', [usuario.id]);
  const token = gerarToken(32);
  await pool.query(
    'INSERT INTO painel.sessoes (id_hash, usuario_id, expira_em, ip) VALUES ($1, $2, now() + make_interval(hours => $3), $4)',
    [hashToken(token), usuario.id, config.sessaoHoras, ip ?? null],
  );
  await pool.query('INSERT INTO painel.auditoria (usuario_id, usuario, acao, ip) VALUES ($1, $2, $3, $4)', [usuario.id, usuario.nome, 'LOGIN', ip ?? null]);
  return { token, usuario: { id: usuario.id, nome: usuario.nome, email: usuario.email, perfil: usuario.perfil } };
}

export async function usuarioDaSessao(pool, token) {
  if (!token) return null;
  const r = await pool.query(
    `SELECT u.id, u.nome, u.email, u.perfil FROM painel.sessoes s JOIN painel.usuarios u ON u.id = s.usuario_id
     WHERE s.id_hash = $1 AND s.expira_em > now() AND u.ativo`,
    [hashToken(token)],
  );
  return r.rows[0] ?? null;
}

export async function sair(pool, token) {
  if (token) await pool.query('DELETE FROM painel.sessoes WHERE id_hash = $1', [hashToken(token)]);
}

export async function lojaDoToken(pool, token) {
  if (!token) return null;
  const r = await pool.query('SELECT id, nome, fuso FROM painel.lojas WHERE token_hash = $1 AND ativa', [hashToken(token)]);
  return r.rows[0] ?? null;
}

export async function limparSessoesVencidas(pool) {
  await pool.query('DELETE FROM painel.sessoes WHERE expira_em < now()');
}
