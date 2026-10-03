// Comandos de administração (rodam no servidor, não pela internet):
//   node src/cli.js criar-loja "Nome da loja"
//   node src/cli.js criar-usuario "Nome" email@exemplo.com consulta|gerente [--sem-2fa]
// O token da loja e a senha do usuário aparecem UMA vez; só o hash fica guardado.
import { carregarConfig } from './config.js';
import { criarPool } from './db.js';
import { migrar } from './migrate.js';
import { gerarSegredoTotp, gerarToken, hashSenha, hashToken, urlOtpauth } from './seguranca.js';

const [, , comando, ...args] = process.argv;
const config = carregarConfig();
const pool = criarPool(config.databaseUrl);

function uso() {
  console.log('Uso:\n  criar-loja "Nome da loja"\n  criar-usuario "Nome" email perfil(consulta|gerente|admin) [--sem-2fa] [--senha=...]');
  process.exit(1);
}

try {
  await migrar(pool);
  if (comando === 'criar-loja') {
    const nome = args[0];
    if (!nome) uso();
    const token = gerarToken(32);
    const r = await pool.query('INSERT INTO painel.lojas (nome, token_hash) VALUES ($1, $2) RETURNING id', [nome, hashToken(token)]);
    console.log(`Loja criada (id ${r.rows[0].id}).`);
    console.log(`TOKEN DA LOJA (guarde agora, não será mostrado de novo):\n${token}`);
  } else if (comando === 'criar-usuario') {
    const [nome, email, perfil = 'consulta'] = args.filter((a) => !a.startsWith('--'));
    if (!nome || !email || !['consulta', 'gerente', 'admin'].includes(perfil)) uso();
    const sem2fa = args.includes('--sem-2fa');
    const senhaArg = args.find((a) => a.startsWith('--senha='));
    const senha = senhaArg ? senhaArg.slice(8) : gerarToken(9);
    const segredo = sem2fa ? null : gerarSegredoTotp();
    await pool.query(
      'INSERT INTO painel.usuarios (nome, email, senha_hash, perfil, totp_segredo, totp_ativo) VALUES ($1, $2, $3, $4, $5, $6)',
      [nome, email.toLowerCase(), await hashSenha(senha), perfil, segredo, Boolean(segredo)],
    );
    console.log(`Usuário criado: ${nome} <${email}> (${perfil}).`);
    if (!senhaArg) console.log(`SENHA INICIAL (troque assim que possível): ${senha}`);
    if (segredo) console.log(`Verificação em duas etapas — cadastre no aplicativo autenticador:\n${urlOtpauth(segredo, email)}\nSegredo: ${segredo}`);
  } else {
    uso();
  }
} catch (erro) {
  console.error('Erro:', erro.message);
  process.exitCode = 1;
} finally {
  await pool.end();
}
