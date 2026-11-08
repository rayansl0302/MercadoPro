// Metas: despesas fixas do mês e ponto de equilíbrio (quanto vender para pagar as contas fixas, dada a margem).
import { useState } from 'react';
import { api, lerDinheiro, moeda } from './api.js';
import { Cartao, Estado, Secao, useDados } from './ui.jsx';

const entrada = 'w-full rounded border border-slate-300 px-3 py-2 text-sm';
const virgula = (n) => String(n).replace('.', ',');

export function Metas({ podeAgir }) {
  const r = useDados('/api/painel/metas', {});
  return <Estado r={r}>{(d) => <MetasTela d={d} podeAgir={podeAgir} recarregar={r.recarregar} />}</Estado>;
}

function MetasTela({ d, podeAgir, recarregar }) {
  const [nome, setNome] = useState('');
  const [valor, setValor] = useState('');
  const [editando, setEditando] = useState(null);
  const [margem, setMargem] = useState(virgula(d.margemPercentual));
  const [dias, setDias] = useState(String(d.diasFuncionamento));
  const [erro, setErro] = useState('');
  const [ocupado, setOcupado] = useState(false);

  const chamar = async (fn) => {
    setErro(''); setOcupado(true);
    try { await fn(); recarregar(); } catch (e) { setErro(e.message); } finally { setOcupado(false); }
  };
  const salvarDespesa = () => chamar(async () => {
    await api.post('/api/painel/metas/despesas', { id: editando ?? undefined, nome, valor: lerDinheiro(valor) });
    setNome(''); setValor(''); setEditando(null);
  });
  const remover = (id) => chamar(() => api.post(`/api/painel/metas/despesas/${id}/remover`));
  const salvarConfig = () => chamar(() => api.post('/api/painel/metas/config', { margemPercentual: lerDinheiro(margem), diasFuncionamento: Number(dias) }));

  const m = d.mes;
  const pct = Math.min(100, m.percentualDaMeta);
  return (
    <div className="space-y-4">
      <div className="grid grid-cols-2 gap-3 lg:grid-cols-4">
        <Cartao titulo="Despesas fixas por mês" valor={moeda(d.totalFixo)} />
        <Cartao titulo="Margem de lucro" valor={`${virgula(d.margemPercentual)}%`} detalhe="média sobre as vendas" />
        <Cartao titulo="Precisa vender no mês" valor={moeda(d.faturamentoNecessario)} tom="alerta" detalhe="para cobrir as despesas fixas" />
        <Cartao titulo="Por dia de funcionamento" valor={moeda(d.porDia)} detalhe={`em ${d.diasFuncionamento} dias`} />
      </div>

      <Secao titulo="Como está o mês">
        <p className="mb-2 text-sm text-slate-600">Vendido até hoje: <b>{moeda(m.vendido)}</b> de <b>{moeda(d.faturamentoNecessario)}</b> ({virgula(m.percentualDaMeta)}% da meta)</p>
        <div className="h-3 w-full overflow-hidden rounded-full bg-slate-100"><div className={`h-full ${m.percentualDaMeta >= 100 ? 'bg-emerald-500' : 'bg-amber-500'}`} style={{ width: `${pct}%` }} /></div>
        <dl className="mt-3 grid grid-cols-2 gap-3 text-sm lg:grid-cols-4">
          <div><dt className="text-xs uppercase text-slate-500">Falta vender</dt><dd className="font-semibold">{moeda(m.faltaVender)}</dd></div>
          <div><dt className="text-xs uppercase text-slate-500">Projeção do mês (no ritmo atual)</dt><dd className="font-semibold">{moeda(m.projecaoDoMes)}</dd></div>
          <div><dt className="text-xs uppercase text-slate-500">Dia {m.diaDoMes} de {m.diasNoMes}</dt><dd className="font-semibold">{m.projecaoDoMes >= d.faturamentoNecessario ? 'No caminho' : 'Abaixo da meta'}</dd></div>
          <div><dt className="text-xs uppercase text-slate-500">Resultado estimado do mês</dt><dd className={`font-semibold ${m.lucroEstimado < 0 ? 'text-red-700' : 'text-emerald-700'}`}>{moeda(m.lucroEstimado)}</dd></div>
        </dl>
        <p className="mt-2 text-xs text-slate-500">Resultado estimado = vendas do mês × margem − despesas fixas. É uma estimativa: depende de a margem informada ser a real.</p>
      </Secao>

      <Secao titulo="Despesas fixas do mês">
        {d.despesas.length === 0 && <p className="py-3 text-sm text-slate-500">Nenhuma despesa cadastrada. Adicione aluguel, funcionários, energia, IPTU…</p>}
        <ul className="divide-y divide-slate-100">
          {d.despesas.map((x) => (
            <li key={x.id} className="flex items-center justify-between gap-2 py-2 text-sm">
              <span>{x.nome}</span>
              <span className="flex items-center gap-2">
                <b className="tabular-nums">{moeda(x.valor)}</b>
                {podeAgir && <button className="rounded border border-slate-300 px-2 py-0.5 text-xs" onClick={() => { setEditando(x.id); setNome(x.nome); setValor(virgula(x.valor)); }}>Editar</button>}
                {podeAgir && <button className="rounded border border-red-300 px-2 py-0.5 text-xs text-red-700" onClick={() => remover(x.id)}>Remover</button>}
              </span>
            </li>
          ))}
          <li className="flex items-center justify-between py-2 text-sm font-semibold"><span>Total</span><span className="tabular-nums">{moeda(d.totalFixo)}</span></li>
        </ul>
        {podeAgir && (
          <div className="mt-3 flex flex-wrap items-end gap-2">
            <label className="min-w-[10rem] flex-1 text-xs text-slate-600">Despesa<input className={`${entrada} mt-0.5`} value={nome} onChange={(e) => setNome(e.target.value)} placeholder="Ex.: Aluguel" /></label>
            <label className="w-36 text-xs text-slate-600">Valor por mês (R$)<input className={`${entrada} mt-0.5`} inputMode="decimal" value={valor} onChange={(e) => setValor(e.target.value)} placeholder="2.000,00" /></label>
            <button disabled={ocupado} onClick={salvarDespesa} className="rounded bg-marca px-4 py-2 text-sm font-medium text-white disabled:opacity-50">{editando ? 'Salvar' : 'Adicionar'}</button>
            {editando && <button className="rounded px-3 py-2 text-sm" onClick={() => { setEditando(null); setNome(''); setValor(''); }}>Cancelar edição</button>}
          </div>
        )}
        {erro && <p className="mt-3 rounded bg-red-50 p-2 text-sm text-red-800">{erro}</p>}
      </Secao>

      <Secao titulo="Margem e dias de funcionamento">
        {podeAgir ? (
          <div className="flex flex-wrap items-end gap-2">
            <label className="w-40 text-xs text-slate-600">Margem de lucro (%)<input className={`${entrada} mt-0.5`} inputMode="decimal" value={margem} onChange={(e) => setMargem(e.target.value)} /></label>
            <label className="w-40 text-xs text-slate-600">Dias que a loja abre no mês<input className={`${entrada} mt-0.5`} inputMode="numeric" value={dias} onChange={(e) => setDias(e.target.value)} /></label>
            <button disabled={ocupado} onClick={salvarConfig} className="rounded bg-marca px-4 py-2 text-sm font-medium text-white disabled:opacity-50">Salvar</button>
          </div>
        ) : <p className="text-sm text-slate-600">Margem {virgula(d.margemPercentual)}% · {d.diasFuncionamento} dias.</p>}
        <p className="mt-2 text-xs text-slate-500">Conta: despesas fixas ÷ margem. Ex.: {moeda(d.totalFixo)} ÷ {virgula(d.margemPercentual)}% = {moeda(d.faturamentoNecessario)} de vendas por mês.</p>
      </Secao>

      <Secao titulo="E se a margem for outra?">
        <table className="w-full text-sm">
          <thead><tr className="border-b border-slate-200 text-left text-xs uppercase text-slate-500"><th className="py-2">Margem</th><th className="py-2 text-right">Precisa vender no mês</th><th className="py-2 text-right">Por dia</th></tr></thead>
          <tbody>
            {[15, 20, 25, 30, 35, 40].map((p) => {
              const total = d.totalFixo / (p / 100);
              return <tr key={p} className={`border-b border-slate-100 ${p === d.margemPercentual ? 'bg-amber-50 font-medium' : ''}`}><td className="py-2">{p}%</td><td className="py-2 text-right tabular-nums">{moeda(total)}</td><td className="py-2 text-right tabular-nums">{moeda(total / d.diasFuncionamento)}</td></tr>;
            })}
          </tbody>
        </table>
      </Secao>
    </div>
  );
}
