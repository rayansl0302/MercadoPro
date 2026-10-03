import { useEffect, useState } from 'react';
import QRCode from 'qrcode';
import { api, dataBr } from './api.js';
import { Estado, Secao, Selo, Tabela, useDados } from './ui.jsx';

const entrada = 'w-full rounded border border-slate-300 px-3 py-2';
const Campo = ({ rotulo, children }) => <label className="mb-3 block text-sm"><span className="mb-1 block text-slate-600">{rotulo}</span>{children}</label>;

export function Modal({ titulo, onFechar, children }) {
  return (
    <div className="fixed inset-0 z-50 flex items-end justify-center bg-black/40 p-0 sm:items-center sm:p-4" onClick={onFechar}>
      <div className="max-h-[90vh] w-full max-w-md overflow-y-auto rounded-t-2xl bg-white p-5 shadow-xl sm:rounded-2xl" onClick={(e) => e.stopPropagation()}>
        <h3 className="mb-4 text-lg font-semibold">{titulo}</h3>
        {children}
      </div>
    </div>
  );
}

export const PERFIS = { consulta: 'Consulta (só vê)', gerente: 'Gerente (vê e pede ações)', admin: 'Administrador (também gerencia usuários)' };

// campo de senha com botão de mostrar/ocultar
function CampoSenha({ rotulo, valor, onChange, auto }) {
  const [ver, setVer] = useState(false);
  return (
    <Campo rotulo={rotulo}>
      <div className="relative">
        <input className={`${entrada} pr-16`} type={ver ? 'text' : 'password'} autoComplete={auto} value={valor} onChange={(e) => onChange(e.target.value)} required />
        <button type="button" className="absolute right-2 top-1/2 -translate-y-1/2 text-xs text-slate-500 underline" onClick={() => setVer((v) => !v)}>{ver ? 'ocultar' : 'mostrar'}</button>
      </div>
    </Campo>
  );
}
export { CampoSenha };

// ------------------------------------------------------------------ trocar a própria senha (voluntária ou obrigatória)
export function TrocarSenha({ obrigatoria, onFechar, onTrocada }) {
  const [f, setF] = useState({ atual: '', nova: '', repetir: '' });
  const [erro, setErro] = useState('');
  const [enviando, setEnviando] = useState(false);
  const enviar = async (e) => {
    e.preventDefault();
    if (f.nova !== f.repetir) { setErro('As duas senhas novas não são iguais.'); return; }
    setEnviando(true); setErro('');
    try {
      await api.post('/api/painel/senha', { atual: f.atual, nova: f.nova });
      onTrocada();
    } catch (x) { setErro(x.message); setEnviando(false); }
  };
  const corpo = (
    <form onSubmit={enviar}>
      {obrigatoria && <p className="mb-4 rounded bg-amber-50 p-3 text-sm text-amber-900">Você entrou com uma senha temporária. Defina agora uma senha só sua para continuar.</p>}
      <CampoSenha rotulo={obrigatoria ? 'Senha temporária (a que você recebeu)' : 'Senha atual'} valor={f.atual} onChange={(v) => setF({ ...f, atual: v })} auto="current-password" />
      <CampoSenha rotulo="Nova senha (mínimo 10 caracteres, com letras e números)" valor={f.nova} onChange={(v) => setF({ ...f, nova: v })} auto="new-password" />
      <CampoSenha rotulo="Repita a nova senha" valor={f.repetir} onChange={(v) => setF({ ...f, repetir: v })} auto="new-password" />
      {erro && <p className="mb-3 rounded bg-red-50 p-2 text-sm text-red-800">{erro}</p>}
      <div className="flex justify-end gap-2">
        {!obrigatoria && <button type="button" className="rounded px-3 py-2 text-sm" onClick={onFechar}>Cancelar</button>}
        <button disabled={enviando} className="rounded bg-marca px-4 py-2 text-sm font-medium text-white disabled:opacity-50">Salvar nova senha</button>
      </div>
    </form>
  );
  if (!obrigatoria) return <Modal titulo="Alterar minha senha" onFechar={onFechar}>{corpo}</Modal>;
  return (
    <div className="flex min-h-screen items-center justify-center p-4">
      <div className="w-full max-w-sm rounded-2xl bg-white p-6 shadow-lg ring-1 ring-slate-200">
        <img src="/logo.png" alt="MercadoPro" className="mb-4 h-12 w-auto" />
        <h2 className="mb-3 text-lg font-semibold">Defina sua senha</h2>
        {corpo}
      </div>
    </div>
  );
}

// ------------------------------------------------------------------ QR code do autenticador
function Qr({ texto }) {
  const [img, setImg] = useState('');
  useEffect(() => { QRCode.toDataURL(texto, { margin: 1, width: 200 }).then(setImg).catch(() => setImg('')); }, [texto]);
  return img ? <img src={img} alt="QR code para o aplicativo autenticador" className="mx-auto h-48 w-48" /> : null;
}

const Copiavel = ({ rotulo, valor }) => (
  <div className="mb-3 text-sm">
    <div className="mb-1 text-slate-600">{rotulo}</div>
    <div className="flex items-center gap-2">
      <code className="flex-1 break-all rounded bg-slate-100 px-2 py-1.5 font-mono">{valor}</code>
      <button type="button" className="rounded border border-slate-300 px-2 py-1 text-xs" onClick={() => navigator.clipboard?.writeText(valor)}>Copiar</button>
    </div>
  </div>
);

// mostra UMA vez os dados de acesso (a senha temporária e o 2FA não ficam guardados em texto)
function DadosDeAcesso({ titulo, dados, onFechar }) {
  return (
    <Modal titulo={titulo} onFechar={onFechar}>
      <p className="mb-3 rounded bg-amber-50 p-3 text-xs text-amber-900">Estes dados aparecem só agora. Passe para a pessoa por um canal seguro. No primeiro acesso ela vai definir a própria senha.</p>
      <Copiavel rotulo="E-mail" valor={dados.email} />
      {dados.senhaTemporaria && <Copiavel rotulo="Senha temporária" valor={dados.senhaTemporaria} />}
      {dados.segredo && (
        <>
          <p className="mb-2 text-sm text-slate-600">Verificação em duas etapas: escaneie no aplicativo autenticador (Google/Microsoft Authenticator, Authy) ou digite a chave.</p>
          <Qr texto={dados.otpauth} />
          <div className="mt-3"><Copiavel rotulo="Chave do autenticador" valor={dados.segredo} /></div>
        </>
      )}
      <div className="mt-2 flex justify-end"><button className="rounded bg-marca px-4 py-2 text-sm font-medium text-white" onClick={onFechar}>Já anotei, fechar</button></div>
    </Modal>
  );
}

// ------------------------------------------------------------------ página de usuários (admin)
export function Usuarios({ eu, avisar }) {
  const r = useDados('/api/painel/usuarios', {});
  const [novo, setNovo] = useState(false);
  const [dados, setDados] = useState(null);
  const [erro, setErro] = useState('');

  const acao = async (fn, ok) => {
    setErro('');
    try { const res = await fn(); if (ok) avisar(ok); r.recarregar(); return res; } catch (x) { setErro(x.message); return null; }
  };
  const redefinirSenha = async (u) => {
    if (!window.confirm(`Gerar uma nova senha temporária para ${u.nome}? Ela será desconectada de todos os aparelhos.`)) return;
    const res = await acao(() => api.post(`/api/painel/usuarios/${u.id}/redefinir-senha`));
    if (res) setDados({ titulo: `Nova senha de ${u.nome}`, ...res });
  };
  const redefinir2fa = async (u) => {
    if (!window.confirm(`Gerar um novo código de verificação para ${u.nome}? O aplicativo antigo deixa de funcionar.`)) return;
    const res = await acao(() => api.post(`/api/painel/usuarios/${u.id}/redefinir-2fa`));
    if (res) setDados({ titulo: `Novo 2FA de ${u.nome}`, ...res });
  };

  return (
    <div className="space-y-4">
      <div className="flex items-center justify-between">
        <p className="text-sm text-slate-600">Quem pode entrar no painel e o que cada pessoa pode fazer.</p>
        <button onClick={() => setNovo(true)} className="rounded-lg bg-marca px-3 py-2 text-sm font-medium text-white hover:bg-marca-escuro">+ Novo usuário</button>
      </div>
      {erro && <p className="rounded-lg bg-red-50 p-3 text-sm text-red-800">{erro}</p>}
      <Secao titulo="Usuários">
        <Estado r={r}>
          {(d) => (
            <Tabela linhas={d.usuarios} colunas={[
              { titulo: 'Nome', render: (u) => <>{u.nome}{u.id === eu.id && <span className="text-xs text-slate-500"> (você)</span>}<div className="text-xs text-slate-500">{u.email}</div></> },
              { titulo: 'Perfil', render: (u) => (
                <select className="rounded border border-slate-300 bg-white px-1 py-1 text-xs" value={u.perfil} disabled={u.id === eu.id}
                  onChange={(e) => acao(() => api.post(`/api/painel/usuarios/${u.id}`, { perfil: e.target.value }), 'Perfil atualizado.')}>
                  {Object.entries(PERFIS).map(([v, t]) => <option key={v} value={v}>{t}</option>)}
                </select>) },
              { titulo: 'Situação', render: (u) => <>{u.ativo ? <Selo texto="ativo" tom="verde" /> : <Selo texto="desativado" />} {u.trocarSenha && <Selo texto="senha temporária" tom="amarelo" />} {u.bloqueado && <Selo texto="bloqueado" tom="vermelho" />}</> },
              { titulo: 'Último acesso', render: (u) => (u.ultimoAcesso ? dataBr(u.ultimoAcesso) : 'nunca') },
              { titulo: '', direita: true, render: (u) => (
                <div className="flex flex-wrap justify-end gap-2 text-xs">
                  <button className="text-marca underline" onClick={() => redefinirSenha(u)}>Nova senha</button>
                  <button className="text-marca underline" onClick={() => redefinir2fa(u)}>Novo 2FA</button>
                  {u.id !== eu.id && <button className={u.ativo ? 'text-red-700 underline' : 'text-marca underline'}
                    onClick={() => acao(() => api.post(`/api/painel/usuarios/${u.id}`, { ativo: !u.ativo }), u.ativo ? 'Usuário desativado.' : 'Usuário reativado.')}>{u.ativo ? 'Desativar' : 'Reativar'}</button>}
                </div>) },
            ]} />
          )}
        </Estado>
      </Secao>
      {novo && <NovoUsuario onFechar={() => setNovo(false)} onCriado={(res) => { setNovo(false); r.recarregar(); setDados({ titulo: `Acesso de ${res.nome}`, ...res }); }} />}
      {dados && <DadosDeAcesso titulo={dados.titulo} dados={dados} onFechar={() => setDados(null)} />}
    </div>
  );
}

function NovoUsuario({ onFechar, onCriado }) {
  const [f, setF] = useState({ nome: '', email: '', perfil: 'consulta' });
  const [erro, setErro] = useState('');
  const [enviando, setEnviando] = useState(false);
  const enviar = async (e) => {
    e.preventDefault();
    setEnviando(true); setErro('');
    try { onCriado(await api.post('/api/painel/usuarios', f)); } catch (x) { setErro(x.message); setEnviando(false); }
  };
  return (
    <Modal titulo="Novo usuário" onFechar={onFechar}>
      <form onSubmit={enviar}>
        <Campo rotulo="Nome"><input className={entrada} value={f.nome} onChange={(e) => setF({ ...f, nome: e.target.value })} required /></Campo>
        <Campo rotulo="E-mail"><input className={entrada} type="email" value={f.email} onChange={(e) => setF({ ...f, email: e.target.value })} required /></Campo>
        <Campo rotulo="O que pode fazer">
          <select className={entrada} value={f.perfil} onChange={(e) => setF({ ...f, perfil: e.target.value })}>
            {Object.entries(PERFIS).map(([v, t]) => <option key={v} value={v}>{t}</option>)}
          </select>
        </Campo>
        {erro && <p className="mb-3 rounded bg-red-50 p-2 text-sm text-red-800">{erro}</p>}
        <p className="mb-4 text-xs text-slate-500">O sistema gera uma senha temporária e o código de verificação em duas etapas. Você os recebe na próxima tela.</p>
        <div className="flex justify-end gap-2">
          <button type="button" className="rounded px-3 py-2 text-sm" onClick={onFechar}>Cancelar</button>
          <button disabled={enviando} className="rounded bg-marca px-4 py-2 text-sm font-medium text-white disabled:opacity-50">Criar usuário</button>
        </div>
      </form>
    </Modal>
  );
}
