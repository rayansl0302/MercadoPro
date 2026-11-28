// Orçamento de entrega: interpreta uma lista colada ("Sal 100", "250 Paes"...) e acha os produtos e preços.
// Preço = o de gôndola do estoque (base ÷ (1 − taxa), como o PDV cobra), EXCETO nos itens que têm preço de entrega
// (pães e lanches), que seguem a tabela de entrega do PDV (a mesma de itensPadrao() no desktop).
import { comTaxa, taxaDaLoja } from './precotaxa.js';

export const ENTREGA = [
  { id: 'e1', descricao: 'PÃO CASEIRO', unidade: 'un', preco: 0.4, qtdFaixa: 150, precoFaixa: 0.3, termos: 'pao caseiro', padrao: true },
  { id: 'e2', descricao: 'PÃO DELÍCIA SEM RECHEIO - BANDEJA 4 UN', unidade: 'bandeja', preco: 4.08, termos: 'pao delicia sem recheio' },
  { id: 'e3', descricao: 'PÃO DELÍCIA COM RECHEIO - BANDEJA 4 UN', unidade: 'bandeja', preco: 4.8, termos: 'pao delicia com recheio' },
  { id: 'e4', descricao: 'MINI ESFIRRA - PRATO 3 UN', unidade: 'prato', preco: 4.5, termos: 'mini esfirra' },
  { id: 'e5', descricao: 'MINI ENROLADINHO - PRATO 3 UN', unidade: 'prato', preco: 4.5, termos: 'mini enroladinho' },
  { id: 'e6', descricao: 'PÃO PIZZA - PRATO', unidade: 'prato', preco: 6, termos: 'pao pizza' },
  { id: 'e7', descricao: 'PÃO DE HOT DOG - PACOTE (KG)', unidade: 'kg', preco: 14.99, termos: 'pao hot dog' },
  { id: 'e8', descricao: 'PÃO DE HAMBÚRGUER - PACOTE (KG)', unidade: 'kg', preco: 14.99, termos: 'pao hamburguer' },
];

const UNIDADES = {
  kg: 'kg', kgs: 'kg', quilo: 'kg', quilos: 'kg', k: 'kg',
  g: 'g', gr: 'g', grs: 'g', grama: 'g', gramas: 'g',
  un: 'un', und: 'un', unid: 'un', unidade: 'un', unidades: 'un', pc: 'un', pç: 'un', pcs: 'un',
  pct: 'un', pcte: 'un', pacote: 'un', pacotes: 'un', cx: 'un', caixa: 'un', caixas: 'un', bandeja: 'un', bandejas: 'un', prato: 'un', pratos: 'un',
  l: 'un', lt: 'un', litro: 'un', litros: 'un',
};
const LIGACAO = new Set(['de', 'do', 'da', 'dos', 'das', 'e', 'com', 'x']);

export const semAcento = (t) => String(t ?? '').normalize('NFD').replace(/[̀-ͯ]/g, '').toLowerCase();
// singular simples: paes → pao, cebolas → cebola, tomates → tomate
const singular = (p) => (/(aes|oes)$/.test(p) ? p.slice(0, -3) + 'ao' : p.length > 3 && p.endsWith('s') ? p.slice(0, -1) : p);
const fichas = (t) => semAcento(t).replace(/[^a-z0-9\s]/g, ' ').split(/\s+/).filter((x) => x && !LIGACAO.has(x)).map(singular);

// "Sal.   100" | "250 Paes" | "2kg de cebola" | "- leite 50" | "Milho 1,5 kg"
export function lerLinha(bruta) {
  const original = String(bruta ?? '').trim();
  if (!original) return null;
  const limpa = original.replace(/^[\s\-–•*·]+/, '');
  let numero = null;
  let unidade = null;
  let nome = limpa;
  const rx = /(\d+(?:[.,]\d+)?)\s*(kg|kgs|quilos?|gramas?|grs?|g|unidades?|unid|und|un|pcs?|pç|pcte|pct|pacotes?|caixas?|cx|bandejas?|pratos?|litros?|lt|l)?\b/i;
  const mm = limpa.match(rx);
  if (mm) {
    numero = Number(mm[1].replace(',', '.'));
    if (mm[2]) unidade = UNIDADES[mm[2].toLowerCase()] ?? null;
    nome = (limpa.slice(0, mm.index) + ' ' + limpa.slice(mm.index + mm[0].length)).trim();
  }
  const tokens = fichas(nome);
  return { original, numero, unidade, nome: nome.replace(/[.:;,]+\s*$/, '').replace(/\s+/g, ' ').trim(), tokens };
}

// melhor palpite da unidade de medida da quantidade, quando o texto não diz
export function interpretarQuantidade(numero, unidadeTexto, unidadeProduto) {
  const kg = /^(kg|kgs|quilo|quilos)$/i.test(String(unidadeProduto ?? '').trim());
  if (unidadeTexto && (!kg || unidadeTexto !== 'un')) return { quantidade: numero, unidade: kg ? unidadeTexto : 'un', adivinhado: false };
  if (kg) return numero >= 50 ? { quantidade: numero, unidade: 'g', adivinhado: true } : { quantidade: numero, unidade: 'kg', adivinhado: true };
  return { quantidade: numero, unidade: 'un', adivinhado: false };
}

const arred = (v) => Math.round(Number(v) * 100) / 100;

function candidatosEntrega(tokens) {
  if (!tokens.length) return [];
  return ENTREGA.filter((e) => {
    const alvo = fichas(e.descricao + ' ' + e.termos);
    return tokens.every((t) => alvo.some((a) => a === t || a.startsWith(t)));
  }).map((e) => ({ origem: 'entrega', id: e.id, descricao: e.descricao, unidade: e.unidade, preco: e.preco, qtdFaixa: e.qtdFaixa ?? null, precoFaixa: e.precoFaixa ?? null, padrao: !!e.padrao }));
}

async function candidatosEstoque(pool, lojaId, tokens, taxa) {
  if (!tokens.length) return [];
  const conds = tokens.map((_, i) => `painel.sem_acento(descricao) ILIKE '%' || $${i + 2} || '%'`).join(' AND ');
  const r = await pool.query(
    `SELECT id, descricao, un_comercial, preco FROM espelho.produtos WHERE loja_id = $1 AND ${conds}
     ORDER BY length(descricao), descricao LIMIT 8`,
    [lojaId, ...tokens],
  );
  return r.rows.map((x) => ({ origem: 'estoque', id: String(x.id), descricao: x.descricao, unidade: String(x.un_comercial ?? 'UN').trim() || 'UN', preco: arred(comTaxa(Number(x.preco ?? 0), taxa)), qtdFaixa: null, precoFaixa: null }));
}

export async function candidatos(pool, lojaId, tokens) {
  const taxa = await taxaDaLoja(pool, lojaId);
  const ent = candidatosEntrega(tokens);
  // se a lista de entrega tem o item, ele manda; o estoque entra só como alternativa
  const est = await candidatosEstoque(pool, lojaId, tokens, taxa);
  return [...ent, ...est];
}

export async function interpretarLista(pool, lojaId, texto) {
  const linhas = [];
  for (const bruta of String(texto ?? '').split(/\r?\n/).slice(0, 200)) {
    const l = lerLinha(bruta);
    if (!l) continue;
    const cands = await candidatos(pool, lojaId, l.tokens);
    const pref = cands.find((c) => c.padrao) ?? cands[0] ?? null;
    const numero = l.numero ?? 1;
    const q = pref ? interpretarQuantidade(numero, l.unidade, pref.unidade) : { quantidade: numero, unidade: l.unidade ?? 'un', adivinhado: false };
    linhas.push({ original: l.original, nome: l.nome, quantidade: q.quantidade, unidadeQtd: q.unidade, adivinhado: q.adivinhado, semNumero: l.numero === null, candidatos: cands, escolhido: pref ? 0 : -1 });
  }
  return { linhas };
}
