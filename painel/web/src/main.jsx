import { useCallback, useEffect, useState } from 'react';
import { createRoot } from 'react-dom/client';
import './index.css';
import { api, horaBr } from './api.js';
import { Agrupada, Caixas, ContasPagar, Devedores, Estoque, Fiscal, Fluxo, Operadores, Pedidos, Seguranca, Sincronizacao, VisaoGeral, Vendas } from './paginas.jsx';
import { useDados } from './ui.jsx';

const MENU = [
  ['geral', 'Início'], ['vendas', 'Vendas'], ['pagar', 'Contas a pagar'], ['receber', 'A receber'],
  ['estoque', 'Estoque'], ['fiscal', 'Fiscal e segurança'], ['loja', 'Loja e pedidos'],
];

function Login({ onEntrou }) {
  const [f, setF] = useState({ email: '', senha: '', codigo: '' });
  const [erro, setErro] = useState('');
  const [pedeCodigo, setPedeCodigo] = useState(false);
  const [enviando, setEnviando] = useState(false);
  const entrar = async (e) => {
    e.preventDefault();
    setEnviando(true); setErro('');
    try {
      await api.post('/api/painel/login', { email: f.email, senha: f.senha, codigo: f.codigo || undefined });
      onEntrou();
    } catch (x) {
      setErro(x.message);
      if (/c[óo]digo|duas etapas/i.test(x.message)) setPedeCodigo(true);
    } finally { setEnviando(false); }
  };
  const c = 'w-full rounded border border-slate-300 px-3 py-2';
  return (
    <div className="flex min-h-screen items-center justify-center p-4">
      <form onSubmit={entrar} className="w-full max-w-sm rounded-2xl bg-white p-6 shadow-lg ring-1 ring-slate-200">
        <img src="/logo.png" alt="MercadoPro" className="mb-1 h-12 w-auto" />
        <p className="mb-5 text-sm text-slate-500">Painel de gestão</p>
        <label className="mb-3 block text-sm">E-mail<input className={c} type="email" autoComplete="username" value={f.email} onChange={(e) => setF({ ...f, email: e.target.value })} required /></label>
        <label className="mb-3 block text-sm">Senha<input className={c} type="password" autoComplete="current-password" value={f.senha} onChange={(e) => setF({ ...f, senha: e.target.value })} required /></label>
        {pedeCodigo && <label className="mb-3 block text-sm">Código do aplicativo autenticador<input className={c} inputMode="numeric" autoComplete="one-time-code" maxLength={6} value={f.codigo} onChange={(e) => setF({ ...f, codigo: e.target.value })} /></label>}
        {erro && <p className="mb-3 rounded bg-red-50 p-2 text-sm text-red-800">{erro}</p>}
        <button disabled={enviando} className="w-full rounded bg-marca py-2 font-medium text-white hover:bg-marca-escuro disabled:opacity-50">Entrar</button>
        <button type="button" className="mt-3 w-full text-xs text-slate-500 underline" onClick={() => setPedeCodigo((v) => !v)}>{pedeCodigo ? 'Não uso código' : 'Tenho um código de verificação'}</button>
      </form>
    </div>
  );
}

function StatusLoja() {
  const r = useDados('/api/painel/loja', {});
  const { recarregar } = r;
  useEffect(() => { const t = setInterval(recarregar, 30000); return () => clearInterval(t); }, [recarregar]);
  const d = r.dados;
  if (!d) return null;
  return (
    <div className="flex flex-wrap items-center gap-x-3 gap-y-1 text-xs text-slate-600">
      <span className="flex items-center gap-1"><span className={`h-2 w-2 rounded-full ${d.online ? 'bg-emerald-500' : 'bg-slate-400'}`} />Loja {d.online ? 'online' : 'offline'}</span>
      <span>Dados de {d.atualizadoEm ? horaBr(d.atualizadoEm) : 'nunca'}</span>
      {d.pedidosPendentes > 0 && <span className="rounded-full bg-amber-100 px-2 py-0.5 text-amber-800">{d.pedidosPendentes} pedido(s) aguardando a loja</span>}
    </div>
  );
}

function App() {
  const [usuario, setUsuario] = useState(undefined);
  const [formas, setFormas] = useState([]);
  const [pagina, setPagina] = useState('geral');
  const [empresa, setEmpresa] = useState('');
  const [aviso, setAviso] = useState('');
  const [aberto, setAberto] = useState(false);
  const [empresas, setEmpresas] = useState([]);

  const carregarEu = useCallback(() => {
    api.get('/api/painel/eu').then((d) => { setUsuario(d.usuario); setFormas(d.formasPagamento); }).catch(() => setUsuario(null));
  }, []);
  useEffect(carregarEu, [carregarEu]);
  useEffect(() => { if (usuario) api.get('/api/painel/empresas').then((d) => setEmpresas(d.empresas)).catch(() => {}); }, [usuario]);
  useEffect(() => { if (aviso) { const t = setTimeout(() => setAviso(''), 6000); return () => clearTimeout(t); } return undefined; }, [aviso]);

  if (usuario === undefined) return null;
  if (!usuario) return <Login onEntrou={carregarEu} />;

  const podeAgir = usuario.perfil === 'gerente';
  const emp = empresa ? Number(empresa) : undefined;
  const ir = (p) => { setPagina(p); setAberto(false); };
  const telas = {
    geral: <VisaoGeral empresa={emp} ir={ir} />,
    vendas: <Agrupada abas={[['v', 'Vendas', <Vendas empresa={emp} />], ['o', 'Operadores', <Operadores empresa={emp} />], ['c', 'Caixas (abertura e fechamento)', <Caixas />]]} />,
    pagar: <ContasPagar empresa={emp} empresas={empresas} formas={formas} podeAgir={podeAgir} avisar={setAviso} />,
    receber: <Devedores empresa={emp} />,
    estoque: <Estoque />,
    fiscal: <Agrupada abas={[['f', 'Notas fiscais', <Fiscal />], ['s', 'Segurança', <Seguranca />]]} />,
    loja: <Agrupada abas={[['p', 'Pedidos enviados', <Pedidos podeAgir={podeAgir} />], ['s', 'Sincronização', <Sincronizacao />]]} />,
  };
  const sair = async () => { await api.post('/api/painel/logout'); setUsuario(null); };

  return (
    <div className="min-h-screen lg:flex">
      <nav className={`${aberto ? 'block' : 'hidden'} fixed inset-0 z-40 overflow-y-auto bg-white p-4 lg:static lg:block lg:w-56 lg:shrink-0 lg:border-r lg:border-slate-200`}>
        <img src="/logo.png" alt="MercadoPro" className="mb-4 hidden h-10 w-auto lg:block" />
        <ul className="space-y-1">
          {MENU.map(([id, nome]) => (
            <li key={id}><button onClick={() => ir(id)} className={`w-full rounded-lg px-3 py-2 text-left text-sm ${pagina === id ? 'bg-marca-claro font-medium text-marca-escuro' : 'hover:bg-slate-100'}`}>{nome}</button></li>
          ))}
        </ul>
        <div className="mt-6 border-t border-slate-200 pt-4 text-xs text-slate-500">
          <div>{usuario.nome} · {usuario.perfil}</div>
          <button className="mt-1 underline" onClick={sair}>Sair</button>
        </div>
      </nav>
      <div className="min-w-0 flex-1">
        <header className="sticky top-0 z-30 flex flex-wrap items-center justify-between gap-2 border-b border-slate-200 bg-white px-4 py-3">
          <div className="flex items-center gap-3">
            <button className="rounded border border-slate-300 px-2 py-1 text-sm lg:hidden" onClick={() => setAberto((v) => !v)}>{aberto ? 'Fechar' : 'Menu'}</button>
            <h1 className="text-base font-semibold">{MENU.find(([id]) => id === pagina)[1]}</h1>
          </div>
          <div className="flex flex-wrap items-center gap-3">
            {empresas.length > 1 && (
              <select className="rounded border border-slate-300 bg-white px-2 py-1 text-sm" value={empresa} onChange={(e) => setEmpresa(e.target.value)}>
                <option value="">Todas as empresas</option>
                {empresas.map((x) => <option key={x.id} value={x.id}>{x.apelido}</option>)}
              </select>
            )}
            <StatusLoja />
          </div>
        </header>
        {aviso && <div className="mx-4 mt-3 rounded-lg bg-emerald-50 p-3 text-sm text-emerald-900">{aviso}</div>}
        <main className="mx-auto max-w-6xl p-4" key={`${pagina}-${empresa}`}>{telas[pagina]}</main>
      </div>
    </div>
  );
}

createRoot(document.getElementById('raiz')).render(<App />);
