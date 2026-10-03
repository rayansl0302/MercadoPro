// Todas as chamadas ao servidor. Mudanças levam o cabeçalho de origem exigido pela API.
export class ErroApi extends Error {
  constructor(status, mensagem, detalhe) { super(mensagem); this.status = status; this.detalhe = detalhe; }
}

async function chamar(metodo, url, corpo) {
  const resp = await fetch(url, {
    method: metodo,
    headers: { ...(corpo ? { 'Content-Type': 'application/json' } : {}), ...(metodo !== 'GET' ? { 'X-Requested-With': 'painel' } : {}) },
    body: corpo ? JSON.stringify(corpo) : undefined,
    credentials: 'same-origin',
  });
  let dados = null;
  try { dados = await resp.json(); } catch { /* sem corpo */ }
  if (!resp.ok) throw new ErroApi(resp.status, dados?.mensagem ?? 'Não foi possível completar a ação.', dados?.detalhe);
  return dados;
}

export const api = {
  get: (url, params = {}) => {
    const q = new URLSearchParams(Object.entries(params).filter(([, v]) => v !== undefined && v !== null && v !== '')).toString();
    return chamar('GET', q ? `${url}?${q}` : url);
  },
  post: (url, corpo = {}) => chamar('POST', url, corpo),
};

export const moeda = (v) => Number(v ?? 0).toLocaleString('pt-BR', { style: 'currency', currency: 'BRL' });
export const dataBr = (iso) => {
  if (!iso) return '—';
  const s = String(iso).slice(0, 10);
  const [a, m, d] = s.split('-');
  return a && m && d ? `${d}/${m}/${a}` : s;
};
export const horaBr = (valor) => {
  if (!valor) return '—';
  const d = new Date(valor);
  if (Number.isNaN(d.getTime())) return String(valor).slice(11, 16) || '—';
  return d.toLocaleString('pt-BR', { day: '2-digit', month: '2-digit', hour: '2-digit', minute: '2-digit' });
};
// campos de data/hora do espelho chegam como texto "AAAA-MM-DD HH:MM:SS" (hora local da loja)
export const dataHoraLoja = (t) => {
  if (!t) return '—';
  const s = String(t);
  return `${dataBr(s)} ${s.slice(11, 16)}`.trim();
};
export const hojeIso = () => new Date().toLocaleDateString('sv-SE');
export const somaDias = (iso, n) => {
  const d = new Date(`${iso}T12:00:00`);
  d.setDate(d.getDate() + n);
  return d.toLocaleDateString('sv-SE');
};
