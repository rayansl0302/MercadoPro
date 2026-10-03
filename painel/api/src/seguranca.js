import { createHash, createHmac, randomBytes, scrypt as scryptCb, timingSafeEqual } from 'node:crypto';
import { promisify } from 'node:util';

const scrypt = promisify(scryptCb);

// ---- tokens e identificadores (aleatórios; só o hash fica guardado) ----------------------------------------

export function gerarToken(bytes = 32) {
  return randomBytes(bytes).toString('base64url');
}

export function hashToken(token) {
  return createHash('sha256').update(token, 'utf8').digest('hex');
}

// comparação sem vazar informação pelo tempo
export function iguaisSeguro(a, b) {
  const ba = Buffer.from(String(a));
  const bb = Buffer.from(String(b));
  if (ba.length !== bb.length) return false;
  return timingSafeEqual(ba, bb);
}

// ---- senhas (scrypt do próprio Node: sem biblioteca nativa) -------------------------------------------------

const N = 16384;
const R = 8;
const P = 1;

export async function hashSenha(senha) {
  const sal = randomBytes(16);
  const derivada = await scrypt(senha.normalize('NFKC'), sal, 64, { N, r: R, p: P });
  return `scrypt$${N}$${R}$${P}$${sal.toString('base64')}$${derivada.toString('base64')}`;
}

export async function conferirSenha(senha, guardado) {
  const partes = String(guardado).split('$');
  if (partes.length !== 6 || partes[0] !== 'scrypt') return false;
  const [, n, r, p, sal, esperado] = partes;
  const derivada = await scrypt(senha.normalize('NFKC'), Buffer.from(sal, 'base64'), 64, {
    N: Number(n),
    r: Number(r),
    p: Number(p),
  });
  return timingSafeEqual(derivada, Buffer.from(esperado, 'base64'));
}

export function senhaForte(senha) {
  return typeof senha === 'string' && senha.length >= 10 && /[a-z]/i.test(senha) && /\d/.test(senha);
}

// senha temporária legível (sem 0/O/1/l/I) que sempre passa em senhaForte
export function gerarSenhaTemporaria() {
  const letras = 'abcdefghijkmnpqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ';
  const numeros = '23456789';
  const pega = (cj) => cj[randomBytes(1)[0] % cj.length];
  let s = '';
  for (let i = 0; i < 10; i++) s += pega(letras);
  s = s.slice(0, 5) + pega(numeros) + pega(numeros) + s.slice(5) + pega(numeros);
  return s;
}

// ---- verificação em duas etapas (TOTP, RFC 6238) ------------------------------------------------------------

const ALFABETO = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ234567';

export function base32Codificar(buffer) {
  let bits = '';
  for (const byte of buffer) bits += byte.toString(2).padStart(8, '0');
  let saida = '';
  for (let i = 0; i < bits.length; i += 5) saida += ALFABETO[Number.parseInt(bits.slice(i, i + 5).padEnd(5, '0'), 2)];
  return saida;
}

export function base32Decodificar(texto) {
  let bits = '';
  for (const c of texto.replace(/=+$/, '').toUpperCase()) {
    const v = ALFABETO.indexOf(c);
    if (v < 0) throw new Error('base32 inválido');
    bits += v.toString(2).padStart(5, '0');
  }
  const bytes = [];
  for (let i = 0; i + 8 <= bits.length; i += 8) bytes.push(Number.parseInt(bits.slice(i, i + 8), 2));
  return Buffer.from(bytes);
}

export function gerarSegredoTotp() {
  return base32Codificar(randomBytes(20));
}

export function codigoTotp(segredoBase32, agoraMs = Date.now(), passo = 30, digitos = 6) {
  const contador = Math.floor(agoraMs / 1000 / passo);
  const buf = Buffer.alloc(8);
  buf.writeBigUInt64BE(BigInt(contador));
  const hmac = createHmac('sha1', base32Decodificar(segredoBase32)).update(buf).digest();
  const deslocamento = hmac[hmac.length - 1] & 0x0f;
  const numero =
    ((hmac[deslocamento] & 0x7f) << 24) |
    ((hmac[deslocamento + 1] & 0xff) << 16) |
    ((hmac[deslocamento + 2] & 0xff) << 8) |
    (hmac[deslocamento + 3] & 0xff);
  return String(numero % 10 ** digitos).padStart(digitos, '0');
}

// aceita o código do passo atual e de um passo antes/depois (relógios levemente diferentes)
export function conferirTotp(segredoBase32, codigo, agoraMs = Date.now()) {
  if (!/^\d{6}$/.test(String(codigo ?? ''))) return false;
  for (const delta of [-1, 0, 1]) {
    if (iguaisSeguro(codigoTotp(segredoBase32, agoraMs + delta * 30000), codigo)) return true;
  }
  return false;
}

export function urlOtpauth(segredo, email, emissor = 'MercadoPro') {
  return `otpauth://totp/${encodeURIComponent(emissor)}:${encodeURIComponent(email)}?secret=${segredo}&issuer=${encodeURIComponent(emissor)}`;
}
