// Grade de análise: ordena clicando no título da coluna e filtra em cada coluna (como uma planilha).
// Serve para qualquer lista do painel. Colunas: { chave, titulo, tipo: 'texto'|'numero'|'moeda'|'opcoes', valor?, render?, somar? }
import { useMemo, useState } from 'react';
import { moeda } from './api.js';

const norm = (t) => String(t ?? '').normalize('NFD').replace(/[̀-ͯ]/g, '').toLowerCase();
const lerNum = (t) => {
  const s = String(t ?? '').trim().replace(',', '.');
  if (s === '') return null;
  const n = Number(s);
  return Number.isFinite(n) ? n : null;
};
const ehNumero = (c) => c.tipo === 'numero' || c.tipo === 'moeda';
const valorDe = (c, l) => (c.valor ? c.valor(l) : l[c.chave]);
const campo = 'w-full rounded border px-1.5 py-1 text-sm font-normal normal-case text-slate-800';
const borda = (ativo) => (ativo ? 'border-amber-500 bg-amber-50' : 'border-slate-300 bg-white');

function formatar(c, v) {
  if (v === null || v === undefined || v === '') return '—';
  if (c.tipo === 'moeda') return moeda(v);
  return String(v);
}

function Filtro({ c, f = {}, opcoes, mudar }) {
  if (c.tipo === 'opcoes') {
    return (
      <select value={f.t ?? ''} onChange={(e) => mudar({ t: e.target.value })} className={`${campo} ${borda(f.t)}`}>
        <option value="">todos</option>
        {opcoes.map((o) => <option key={o} value={o}>{o}</option>)}
      </select>
    );
  }
  if (ehNumero(c)) {
    return (
      <div className="flex gap-1">
        <input value={f.min ?? ''} onChange={(e) => mudar({ ...f, min: e.target.value })} inputMode="decimal" placeholder="de" aria-label={`${c.titulo} mínimo`} className={`${campo} min-w-0 ${borda(f.min)}`} />
        <input value={f.max ?? ''} onChange={(e) => mudar({ ...f, max: e.target.value })} inputMode="decimal" placeholder="até" aria-label={`${c.titulo} máximo`} className={`${campo} min-w-0 ${borda(f.max)}`} />
      </div>
    );
  }
  return <input value={f.t ?? ''} onChange={(e) => mudar({ t: e.target.value })} placeholder="contém…" aria-label={`${c.titulo} contém`} className={`${campo} ${borda(f.t)}`} />;
}

function baixarCsv(nome, colunas, linhas) {
  const esc = (v) => {
    const t = typeof v === 'number' ? String(v).replace('.', ',') : String(v ?? '');
    return /[;"\n]/.test(t) ? `"${t.replace(/"/g, '""')}"` : t;
  };
  const corpo = [colunas.map((c) => esc(c.titulo)).join(';'), ...linhas.map((l) => colunas.map((c) => esc(valorDe(c, l))).join(';'))].join('\r\n');
  const url = URL.createObjectURL(new Blob(['﻿' + corpo], { type: 'text/csv;charset=utf-8' }));
  const a = document.createElement('a');
  a.href = url; a.download = `${nome}.csv`; a.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}

export function Grade({ colunas, linhas, inicial = {}, atalhos = [], nomeArquivo = 'dados', vazio = 'Nenhum registro com esses filtros.', porPagina = 100 }) {
  const [filtros, setFiltros] = useState(inicial.filtros ?? {});
  const [ordem, setOrdem] = useState(inicial.ordem ?? null);
  const [busca, setBusca] = useState('');
  const [visiveis, setVisiveis] = useState(porPagina);
  const [painel, setPainel] = useState(false);

  const opcoes = useMemo(() => {
    const o = {};
    for (const c of colunas) if (c.tipo === 'opcoes') o[c.chave] = [...new Set(linhas.map((l) => valorDe(c, l)).filter((v) => v !== null && v !== undefined))].sort();
    return o;
  }, [colunas, linhas]);

  const mudarFiltro = (chave, f) => { setFiltros((a) => ({ ...a, [chave]: f })); setVisiveis(porPagina); };
  const filtroAtivo = (c) => { const f = filtros[c.chave]; return f && (f.t || f.min || f.max); };
  const qtdAtivos = colunas.filter(filtroAtivo).length + (busca ? 1 : 0);

  const resultado = useMemo(() => {
    const termo = norm(busca);
    const textos = colunas.filter((c) => c.tipo === 'texto');
    const filtradas = linhas.filter((l) => {
      if (termo && !textos.some((c) => norm(valorDe(c, l)).includes(termo))) return false;
      for (const c of colunas) {
        const f = filtros[c.chave];
        if (!f) continue;
        const v = valorDe(c, l);
        if (c.tipo === 'texto' && f.t && !norm(v).includes(norm(f.t))) return false;
        if (c.tipo === 'opcoes' && f.t && v !== f.t) return false;
        if (ehNumero(c)) {
          const mn = lerNum(f.min);
          const mx = lerNum(f.max);
          if ((mn !== null || mx !== null) && (v === null || v === undefined)) return false;
          if (mn !== null && v < mn) return false;
          if (mx !== null && v > mx) return false;
        }
      }
      return true;
    });
    if (!ordem) return filtradas;
    const c = colunas.find((x) => x.chave === ordem.chave);
    if (!c) return filtradas;
    const d = ordem.dir === 'asc' ? 1 : -1;
    return [...filtradas].sort((a, b) => {
      const va = valorDe(c, a);
      const vb = valorDe(c, b);
      const nuloA = va === null || va === undefined || va === '';
      const nuloB = vb === null || vb === undefined || vb === '';
      if (nuloA || nuloB) return nuloA === nuloB ? 0 : nuloA ? 1 : -1; // vazios sempre no fim
      return d * (ehNumero(c) ? va - vb : String(va).localeCompare(String(vb), 'pt-BR', { numeric: true }));
    });
  }, [linhas, colunas, filtros, busca, ordem]);

  const clicarTitulo = (chave) => {
    // 1º clique: números do maior para o menor, textos de A a Z; 2º clique inverte; 3º tira a ordem
    const primeiro = ehNumero(colunas.find((c) => c.chave === chave)) ? 'desc' : 'asc';
    setOrdem((o) => {
      if (!o || o.chave !== chave) return { chave, dir: primeiro };
      return o.dir === primeiro ? { chave, dir: primeiro === 'desc' ? 'asc' : 'desc' } : null;
    });
  };
  const seta = (chave) => (ordem?.chave === chave ? (ordem.dir === 'asc' ? '▲' : '▼') : '↕');
  const limpar = () => { setFiltros({}); setBusca(''); setVisiveis(porPagina); };
  const aplicarAtalho = (a) => {
    setFiltros({ ...(a.filtros ?? {}) });
    setBusca('');
    if (a.ordem) setOrdem(a.ordem);
    setVisiveis(porPagina);
  };
  const somas = colunas.filter((c) => c.somar).map((c) => ({ c, total: resultado.reduce((s, l) => s + (Number(valorDe(c, l)) || 0), 0) }));
  const mostradas = resultado.slice(0, visiveis);
  const [primeira, ...demais] = colunas;

  return (
    <div className="space-y-3">
      <div className="flex flex-wrap items-center gap-2">
        <input value={busca} onChange={(e) => { setBusca(e.target.value); setVisiveis(porPagina); }} placeholder="Buscar em todas as colunas de texto…"
          className={`min-w-[12rem] flex-1 rounded-lg border px-3 py-2 text-sm ${borda(busca)}`} />
        <button type="button" onClick={() => setPainel((p) => !p)} className="rounded-lg border border-slate-300 bg-white px-3 py-2 text-sm md:hidden">
          Filtros e ordem{qtdAtivos ? ` (${qtdAtivos})` : ''}
        </button>
        <button type="button" onClick={limpar} disabled={!qtdAtivos} className="rounded-lg border border-slate-300 bg-white px-3 py-2 text-sm disabled:opacity-40">Limpar filtros</button>
        <button type="button" onClick={() => baixarCsv(nomeArquivo, colunas, resultado)} className="rounded-lg border border-slate-300 bg-white px-3 py-2 text-sm">Baixar planilha (CSV)</button>
      </div>

      {atalhos.length > 0 && (
        <div className="flex flex-wrap items-center gap-2 text-sm">
          <span className="text-slate-500">Atalhos:</span>
          {atalhos.map((a) => (
            <button key={a.nome} type="button" onClick={() => aplicarAtalho(a)} className="rounded-full border border-slate-300 bg-white px-3 py-1 text-slate-700">{a.nome}</button>
          ))}
        </div>
      )}

      {/* celular: filtros e ordem num painel; computador: filtros ficam na linha abaixo do título */}
      {painel && (
        <div className="space-y-3 rounded-xl border border-slate-200 bg-slate-50 p-3 md:hidden">
          <label className="block text-xs text-slate-600">Ordenar por
            <div className="mt-0.5 flex gap-2">
              <select value={ordem?.chave ?? ''} onChange={(e) => setOrdem(e.target.value ? { chave: e.target.value, dir: ordem?.dir ?? 'desc' } : null)} className={`${campo} ${borda(false)}`}>
                <option value="">sem ordem</option>
                {colunas.map((c) => <option key={c.chave} value={c.chave}>{c.titulo}</option>)}
              </select>
              <button type="button" onClick={() => setOrdem((o) => (o ? { ...o, dir: o.dir === 'asc' ? 'desc' : 'asc' } : o))} className="shrink-0 rounded border border-slate-300 bg-white px-3 text-sm">{ordem?.dir === 'asc' ? '▲ menor→maior' : '▼ maior→menor'}</button>
            </div>
          </label>
          {colunas.map((c) => (
            <label key={c.chave} className="block text-xs text-slate-600">{c.titulo}
              <div className="mt-0.5"><Filtro c={c} f={filtros[c.chave]} opcoes={opcoes[c.chave]} mudar={(f) => mudarFiltro(c.chave, f)} /></div>
            </label>
          ))}
        </div>
      )}

      <p className="text-sm text-slate-600">
        <b>{resultado.length}</b> de {linhas.length} registro(s)
        {somas.map(({ c, total }) => <span key={c.chave}> · {c.titulo}: <b>{c.tipo === 'moeda' ? moeda(total) : Math.round(total * 1000) / 1000}</b></span>)}
      </p>

      {resultado.length === 0 && <p className="py-6 text-center text-sm text-slate-500">{vazio}</p>}

      {resultado.length > 0 && (
        <>
          <ul className="space-y-2 md:hidden">
            {mostradas.map((l, i) => (
              <li key={l.id ?? i} className="rounded-xl border border-slate-200 bg-white p-3 shadow-sm">
                <div className="mb-2 text-sm font-semibold text-slate-900">{primeira.render ? primeira.render(l) : formatar(primeira, valorDe(primeira, l))}</div>
                <dl className="grid grid-cols-2 gap-x-3 gap-y-1.5 text-sm">
                  {demais.map((c) => (
                    <div key={c.chave}>
                      <dt className="text-[11px] uppercase text-slate-500">{c.titulo}</dt>
                      <dd className="tabular-nums text-slate-800">{c.render ? c.render(l) : formatar(c, valorDe(c, l))}</dd>
                    </div>
                  ))}
                </dl>
              </li>
            ))}
          </ul>

          <div className="hidden overflow-x-auto md:block">
            <table className="w-full text-sm">
              <thead>
                <tr className="border-b border-slate-200 text-left text-xs uppercase text-slate-500">
                  {colunas.map((c) => (
                    <th key={c.chave} className={`px-2 pt-2 font-medium ${ehNumero(c) ? 'text-right' : ''}`} aria-sort={ordem?.chave === c.chave ? (ordem.dir === 'asc' ? 'ascending' : 'descending') : 'none'}>
                      <button type="button" onClick={() => clicarTitulo(c.chave)} className={`inline-flex items-center gap-1 uppercase hover:text-slate-900 ${ordem?.chave === c.chave ? 'text-slate-900' : ''}`}>
                        {c.titulo} <span className="text-[10px]">{seta(c.chave)}</span>
                      </button>
                    </th>
                  ))}
                </tr>
                <tr className="border-b border-slate-200">
                  {colunas.map((c) => (
                    <th key={c.chave} className="px-2 pb-2 pt-1 align-top"><Filtro c={c} f={filtros[c.chave]} opcoes={opcoes[c.chave]} mudar={(f) => mudarFiltro(c.chave, f)} /></th>
                  ))}
                </tr>
              </thead>
              <tbody>
                {mostradas.map((l, i) => (
                  <tr key={l.id ?? i} className="border-b border-slate-100 last:border-0">
                    {colunas.map((c) => (
                      <td key={c.chave} className={`px-2 py-2 ${ehNumero(c) ? 'whitespace-nowrap text-right tabular-nums' : ''}`}>{c.render ? c.render(l) : formatar(c, valorDe(c, l))}</td>
                    ))}
                  </tr>
                ))}
              </tbody>
            </table>
          </div>

          {resultado.length > visiveis && (
            <div className="flex flex-wrap items-center gap-2 text-sm">
              <span className="text-slate-500">Mostrando {visiveis} de {resultado.length}.</span>
              <button type="button" onClick={() => setVisiveis((v) => v + porPagina)} className="rounded-lg border border-slate-300 bg-white px-3 py-1">Mostrar mais {porPagina}</button>
              <button type="button" onClick={() => setVisiveis(resultado.length)} className="rounded-lg border border-slate-300 bg-white px-3 py-1">Mostrar todos</button>
            </div>
          )}
        </>
      )}
    </div>
  );
}
