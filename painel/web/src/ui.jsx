import { useCallback, useEffect, useState } from 'react';
import { api, hojeIso, somaDias } from './api.js';

export function Cartao({ titulo, valor, detalhe, tom = 'normal', onClick }) {
  const cor = { normal: 'text-slate-900', ruim: 'text-red-700', bom: 'text-emerald-700', alerta: 'text-amber-700' }[tom];
  const Tag = onClick ? 'button' : 'div';
  return (
    <Tag onClick={onClick} className={`rounded-xl bg-white p-4 text-left shadow-sm ring-1 ring-slate-200 ${onClick ? 'hover:ring-marca' : ''}`}>
      <div className="text-xs font-medium uppercase tracking-wide text-slate-500">{titulo}</div>
      <div className={`mt-1 text-2xl font-semibold ${cor}`}>{valor}</div>
      {detalhe && <div className="mt-1 text-xs text-slate-500">{detalhe}</div>}
    </Tag>
  );
}

export function Secao({ titulo, acao, children }) {
  return (
    <section className="rounded-xl bg-white p-4 shadow-sm ring-1 ring-slate-200">
      <div className="mb-3 flex items-center justify-between gap-2">
        <h2 className="text-sm font-semibold text-slate-700">{titulo}</h2>
        {acao}
      </div>
      {children}
    </section>
  );
}

export function Tabela({ colunas, linhas, vazio = 'Nada para mostrar.' }) {
  if (!linhas.length) return <p className="py-6 text-center text-sm text-slate-500">{vazio}</p>;
  return (
    <div className="overflow-x-auto">
      <table className="w-full text-sm">
        <thead>
          <tr className="border-b border-slate-200 text-left text-xs uppercase text-slate-500">
            {colunas.map((c) => <th key={c.titulo} className={`px-2 py-2 font-medium ${c.direita ? 'text-right' : ''}`}>{c.titulo}</th>)}
          </tr>
        </thead>
        <tbody>
          {linhas.map((l, i) => (
            <tr key={l.id ?? i} className="border-b border-slate-100 last:border-0">
              {colunas.map((c) => <td key={c.titulo} className={`whitespace-nowrap px-2 py-2 ${c.direita ? 'text-right tabular-nums' : ''}`}>{c.render(l)}</td>)}
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

export function Selo({ texto, tom = 'cinza' }) {
  const cores = { cinza: 'bg-slate-100 text-slate-700', verde: 'bg-emerald-100 text-emerald-800', vermelho: 'bg-red-100 text-red-800', amarelo: 'bg-amber-100 text-amber-800', azul: 'bg-sky-100 text-sky-800' };
  return <span className={`inline-block rounded-full px-2 py-0.5 text-xs font-medium ${cores[tom]}`}>{texto}</span>;
}

export function Barras({ itens, formato }) {
  const max = Math.max(1, ...itens.map((i) => i.valor));
  return (
    <ul className="space-y-1.5">
      {itens.map((i) => (
        <li key={i.rotulo} className="flex items-center gap-2 text-sm">
          <span className="w-28 shrink-0 truncate text-slate-600 sm:w-40" title={i.rotulo}>{i.rotulo}</span>
          <span className="h-4 flex-1 rounded bg-slate-100">
            <span className="block h-4 rounded bg-marca" style={{ width: `${(i.valor / max) * 100}%` }} />
          </span>
          <span className="w-28 shrink-0 text-right tabular-nums">{formato(i.valor)}</span>
        </li>
      ))}
    </ul>
  );
}

// carrega dados de uma rota e recarrega quando os parâmetros mudam
export function useDados(url, params) {
  const [estado, setEstado] = useState({ carregando: true, dados: null, erro: null });
  const chave = JSON.stringify(params ?? {});
  const carregar = useCallback(() => {
    setEstado((e) => ({ ...e, carregando: true, erro: null }));
    api.get(url, JSON.parse(chave))
      .then((dados) => setEstado({ carregando: false, dados, erro: null }))
      .catch((erro) => setEstado({ carregando: false, dados: null, erro }));
  }, [url, chave]);
  useEffect(carregar, [carregar]);
  return { ...estado, recarregar: carregar };
}

export function Estado({ r, children }) {
  if (r.erro) return <p className="rounded-lg bg-red-50 p-3 text-sm text-red-800">{r.erro.message}</p>;
  if (!r.dados) return <p className="p-6 text-center text-sm text-slate-500">Carregando…</p>;
  return children(r.dados);
}

const ATALHOS = [
  ['Hoje', () => [hojeIso(), hojeIso()]],
  ['7 dias', () => [somaDias(hojeIso(), -6), hojeIso()]],
  ['30 dias', () => [somaDias(hojeIso(), -29), hojeIso()]],
  ['Este mês', () => [`${hojeIso().slice(0, 8)}01`, hojeIso()]],
];

export function Periodo({ valor, onChange }) {
  return (
    <div className="flex flex-wrap items-center gap-2 text-sm">
      {ATALHOS.map(([nome, f]) => (
        <button key={nome} className="rounded-full bg-white px-3 py-1 shadow-sm ring-1 ring-slate-200 hover:ring-marca" onClick={() => { const [de, ate] = f(); onChange({ de, ate }); }}>{nome}</button>
      ))}
      <input type="date" className="rounded border border-slate-300 bg-white px-2 py-1" value={valor.de} max={valor.ate} onChange={(e) => e.target.value && onChange({ ...valor, de: e.target.value })} />
      <span className="text-slate-500">até</span>
      <input type="date" className="rounded border border-slate-300 bg-white px-2 py-1" value={valor.ate} min={valor.de} onChange={(e) => e.target.value && onChange({ ...valor, ate: e.target.value })} />
    </div>
  );
}

export const periodoInicial = () => ({ de: somaDias(hojeIso(), -6), ate: hojeIso() });
