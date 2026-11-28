// Orçamento de entrega: cola a lista do cliente, o painel acha os produtos e os preços de entrega e monta o texto para enviar.
import { useMemo, useState } from 'react';
import { api, moeda } from './api.js';
import { Secao } from './ui.jsx';

const entrada = 'w-full rounded border border-slate-300 px-3 py-2 text-sm';
const arred = (v) => Math.round(Number(v) * 100) / 100;
const num = (t) => { const n = Number(String(t).replace(',', '.')); return Number.isFinite(n) ? n : 0; };
const ehKg = (u) => /^(kg|kgs|quilo|quilos)$/i.test(String(u ?? '').trim());
const bonito = (t) => String(t).toLowerCase().replace(/(^|[\s(-])(\S)/g, (_, a, b) => a + b.toUpperCase()).replace(/\b(De|Do|Da|Com|Sem|Un)\b/g, (x) => x.toLowerCase());

// quantidade na unidade do produto (kg ou unidade/bandeja/prato) e preço unitário aplicado (com faixa de desconto)
function calcular(l) {
  const c = l.candidatos[l.escolhido];
  if (!c) return null;
  const q = num(l.quantidade);
  const qProduto = ehKg(c.unidade) ? (l.unidadeQtd === 'g' ? q / 1000 : q) : q;
  const base = c.qtdFaixa && qProduto >= c.qtdFaixa && c.precoFaixa != null ? c.precoFaixa : c.preco;
  const preco = l.precoManual !== '' && l.precoManual != null ? num(l.precoManual) : base;
  return { c, qProduto, preco, total: arred(qProduto * preco) };
}
const fmtQtd = (n) => String(Math.round(n * 1000) / 1000).replace('.', ',');
const rotuloUnidade = (c) => (ehKg(c.unidade) ? 'kg' : c.unidade === 'bandeja' ? 'bandeja(s)' : c.unidade === 'prato' ? 'prato(s)' : 'un');

export function Orcamento() {
  const [texto, setTexto] = useState('');
  const [linhas, setLinhas] = useState(null);
  const [erro, setErro] = useState('');
  const [ocupado, setOcupado] = useState(false);
  const [copiado, setCopiado] = useState(false);

  const interpretar = async () => {
    setErro(''); setOcupado(true);
    try {
      const r = await api.post('/api/painel/orcamento/interpretar', { texto });
      setLinhas(r.linhas.map((l) => ({ ...l, precoManual: '' })));
    } catch (e) { setErro(e.message); } finally { setOcupado(false); }
  };
  const mudar = (i, parte) => setLinhas((ls) => ls.map((l, k) => (k === i ? { ...l, ...parte } : l)));
  const escolher = (i, idx) => setLinhas((ls) => ls.map((l, k) => {
    if (k !== i) return l;
    const c = l.candidatos[idx];
    const kg = c && ehKg(c.unidade);
    return { ...l, escolhido: idx, precoManual: '', unidadeQtd: kg ? (l.unidadeQtd === 'un' ? (num(l.quantidade) >= 50 ? 'g' : 'kg') : l.unidadeQtd) : 'un' };
  }));
  const buscar = async (i, termo) => {
    if (String(termo).trim().length < 2) return;
    try {
      const r = await api.get('/api/painel/orcamento/buscar', { q: termo });
      setLinhas((ls) => ls.map((l, k) => (k === i ? { ...l, candidatos: r.candidatos, escolhido: r.candidatos.length ? 0 : -1, unidadeQtd: r.candidatos[0] && ehKg(r.candidatos[0].unidade) ? (l.unidadeQtd === 'un' ? 'kg' : l.unidadeQtd) : 'un', precoManual: '' } : l)));
    } catch (e) { setErro(e.message); }
  };

  const calc = useMemo(() => (linhas ?? []).map(calcular), [linhas]);
  const total = arred(calc.reduce((s, x) => s + (x ? x.total : 0), 0));
  const caseiros = calc.reduce((s, x) => s + (x && x.c.id === 'e1' ? x.total : 0), 0);
  const semProduto = (linhas ?? []).filter((_, i) => !calc[i]).length;

  const mensagem = useMemo(() => {
    if (!linhas) return '';
    const itens = linhas.map((l, i) => {
      const x = calc[i];
      if (!x) return null;
      const un = ehKg(x.c.unidade) ? (l.unidadeQtd === 'g' ? `${fmtQtd(num(l.quantidade))} g` : `${fmtQtd(x.qProduto)} kg`) : `${fmtQtd(x.qProduto)} ${rotuloUnidade(x.c)}`;
      return `• ${bonito(x.c.descricao)}: ${un} × ${moeda(x.preco)} = ${moeda(x.total)}`;
    }).filter(Boolean);
    if (!itens.length) return '';
    return `*Orçamento*\n\n${itens.join('\n')}\n\n*Total: ${moeda(total)}*`;
  }, [linhas, calc, total]);

  const copiar = async () => {
    try { await navigator.clipboard.writeText(mensagem); setCopiado(true); setTimeout(() => setCopiado(false), 2000); } catch { setErro('Não consegui copiar sozinho: selecione o texto e copie.'); }
  };

  return (
    <div className="space-y-4">
      <Secao titulo="Orçamento de entrega">
        <p className="mb-2 text-xs text-slate-600">Cole a lista do cliente (um item por linha, com a quantidade antes ou depois do nome). Pães e lanches usam o preço de entrega; o resto usa o preço de venda do estoque.</p>
        <textarea className={`${entrada} h-40 font-mono`} value={texto} onChange={(e) => setTexto(e.target.value)} placeholder={'Sal 100\nMilho 50\nLeite 50\n250 Paes'} />
        <div className="mt-2 flex items-center gap-3">
          <button type="button" disabled={ocupado || !texto.trim()} onClick={interpretar} className="rounded-lg bg-marca px-4 py-2 text-sm font-medium text-white disabled:opacity-50">{ocupado ? 'Calculando…' : 'Calcular orçamento'}</button>
          {linhas && <button type="button" className="text-sm underline" onClick={() => { setLinhas(null); setTexto(''); }}>Limpar</button>}
        </div>
        {erro && <p className="mt-2 text-sm text-red-700">{erro}</p>}
      </Secao>

      {linhas && (
        <Secao titulo="Itens encontrados">
          {!linhas.length && <p className="text-sm text-slate-500">Não encontrei nenhuma linha.</p>}
          <div className="space-y-3">
            {linhas.map((l, i) => {
              const x = calc[i];
              const kg = x && ehKg(x.c.unidade);
              return (
                <div key={i} className={`rounded-lg border p-3 text-sm ${x ? 'border-slate-200' : 'border-red-300 bg-red-50'}`}>
                  <div className="mb-2 flex items-start justify-between gap-2">
                    <span className="text-xs text-slate-500">Linha: “{l.original}”</span>
                    <button type="button" className="text-xs text-slate-500 underline" onClick={() => setLinhas((ls) => ls.filter((_, k) => k !== i))}>remover</button>
                  </div>
                  {x ? (
                    <div className="grid gap-2 sm:grid-cols-[1fr_auto_auto_auto] sm:items-end">
                      <label className="block">
                        <span className="text-xs text-slate-500">Produto{l.candidatos.length > 1 ? ` (${l.candidatos.length} opções)` : ''}</span>
                        <select className={entrada} value={l.escolhido} onChange={(e) => escolher(i, Number(e.target.value))}>
                          {l.candidatos.map((c, k) => <option key={`${c.origem}${c.id}`} value={k}>{c.origem === 'entrega' ? '★ ' : ''}{bonito(c.descricao)} — {moeda(c.preco)}/{ehKg(c.unidade) ? 'kg' : c.unidade === 'un' || c.unidade === 'UN' ? 'un' : c.unidade}</option>)}
                        </select>
                      </label>
                      <label className="block">
                        <span className="text-xs text-slate-500">Quantidade</span>
                        <div className="flex gap-1">
                          <input className={`${entrada} w-24`} inputMode="decimal" value={l.quantidade} onChange={(e) => mudar(i, { quantidade: e.target.value })} />
                          {kg ? (
                            <select className="rounded border border-slate-300 px-2 text-sm" value={l.unidadeQtd} onChange={(e) => mudar(i, { unidadeQtd: e.target.value })}>
                              <option value="kg">kg</option><option value="g">g</option>
                            </select>
                          ) : <span className="self-center text-xs text-slate-500">{rotuloUnidade(x.c)}</span>}
                        </div>
                      </label>
                      <label className="block">
                        <span className="text-xs text-slate-500">Preço unit.</span>
                        <input className={`${entrada} w-24`} inputMode="decimal" value={l.precoManual === '' ? String(x.preco).replace('.', ',') : l.precoManual} onChange={(e) => mudar(i, { precoManual: e.target.value })} />
                      </label>
                      <div className="text-right font-semibold sm:pb-2">{moeda(x.total)}</div>
                    </div>
                  ) : (
                    <BuscaManual onBuscar={(t) => buscar(i, t)} sugestao={l.nome} />
                  )}
                  {x && l.adivinhado && kg && <p className="mt-1 text-xs text-amber-700">Entendi “{l.quantidade}” como {l.unidadeQtd === 'g' ? 'gramas' : 'quilos'}: confira e troque se precisar.</p>}
                  {x && l.semNumero && <p className="mt-1 text-xs text-amber-700">Não achei a quantidade na linha: usei 1.</p>}
                </div>
              );
            })}
          </div>
          {semProduto > 0 && <p className="mt-2 text-sm text-red-700">{semProduto} {semProduto === 1 ? 'linha sem produto' : 'linhas sem produto'}: procure pelo nome ou remova.</p>}
          {caseiros > 0 && caseiros < 30 && <p className="mt-2 text-sm text-amber-700">Pedido mínimo de pães caseiros é R$ 30,00 (75 pães): este tem {moeda(caseiros)}.</p>}
        </Secao>
      )}

      {mensagem && (
        <Secao titulo="Texto para enviar ao cliente">
          <textarea className={`${entrada} h-56 font-mono`} readOnly value={mensagem} onFocus={(e) => e.target.select()} />
          <div className="mt-2 flex items-center gap-3">
            <button type="button" onClick={copiar} className="rounded-lg bg-marca px-4 py-2 text-sm font-medium text-white">{copiado ? 'Copiado!' : 'Copiar'}</button>
            <span className="text-lg font-semibold">{moeda(total)}</span>
          </div>
        </Secao>
      )}
    </div>
  );
}

function BuscaManual({ onBuscar, sugestao }) {
  const [t, setT] = useState(sugestao ?? '');
  return (
    <div className="flex gap-2">
      <input className={entrada} value={t} onChange={(e) => setT(e.target.value)} onKeyDown={(e) => e.key === 'Enter' && onBuscar(t)} placeholder="Não achei: digite outro nome" />
      <button type="button" className="rounded border border-slate-300 px-3 text-sm" onClick={() => onBuscar(t)}>Buscar</button>
    </div>
  );
}
