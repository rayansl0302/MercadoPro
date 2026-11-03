// Rota /balanca: pesagem e etiqueta para produto vendido a granel (preço por kg).
// Passo 1 código do produto -> passo 2 peso -> passo 3 confirma o valor e imprime na Niimbot B1 -> nova impressão.
// A impressora fala com o navegador por Bluetooth (Web Bluetooth) ou USB (Web Serial), só em Chrome/Edge.
import { useCallback, useEffect, useRef, useState } from 'react';
import { api } from './api.js';
import { eanDeBalanca, lerPesoEmGramas, moedaDeCentavos, paraCentavos, pesoDeGramas, totalEmCentavos, unidadeEhQuilo } from './balanca.js';
import { ALTURAS, desenharEtiqueta } from './etiqueta.js';
import { Secao } from './ui.jsx';

const lerAltura = () => { try { return Number(localStorage.getItem('mp_etiqueta_altura')) || 240; } catch { return 240; } };

// A biblioteca da impressora só é baixada nesta tela. Fica pré-carregada para o clique em "Imprimir" ainda valer
// como gesto do usuário na hora de abrir o seletor de Bluetooth.
let carregandoLib = null;
// a biblioteca é CommonJS: conforme o empacotador os nomes vêm no módulo ou em `default`
const carregarLib = () => { carregandoLib ??= import('@mmote/niimbluelib').then((m) => (m.NiimbotBluetoothClient ? m : m.default)); return carregandoLib; };

function useImpressora() {
  const clienteRef = useRef(null);
  const viaRef = useRef('bluetooth');
  const [estado, setEstado] = useState({ status: 'desconectada', nome: '', modelo: '' });
  const [erro, setErro] = useState('');
  const temBluetooth = typeof navigator !== 'undefined' && 'bluetooth' in navigator;
  const temUsb = typeof navigator !== 'undefined' && 'serial' in navigator;

  useEffect(() => { carregarLib().catch(() => {}); }, []);
  useEffect(() => () => { try { clienteRef.current?.disconnect(); } catch { /* já desconectada */ } }, []);

  const conectar = useCallback(async (via) => {
    setErro('');
    setEstado((e) => ({ ...e, status: 'conectando' }));
    try {
      const lib = await carregarLib();
      try { await clienteRef.current?.disconnect(); } catch { /* sem conexão anterior */ }
      const cliente = via === 'usb' ? new lib.NiimbotSerialClient() : new lib.NiimbotBluetoothClient();
      cliente.on('disconnect', () => setEstado({ status: 'desconectada', nome: '', modelo: '' }));
      const info = await cliente.connect();
      clienteRef.current = cliente;
      viaRef.current = via;
      setEstado({ status: 'conectada', nome: info.deviceName ?? '', modelo: cliente.getModelMetadata()?.model ?? '' });
      return cliente;
    } catch (x) {
      clienteRef.current = null;
      setEstado({ status: 'desconectada', nome: '', modelo: '' });
      const cancelou = /cancel|chosen|no device selected|nenhum/i.test(String(x?.message ?? x));
      const porta = via === 'usb' && /failed to open serial port|networkerror|access denied|in use/i.test(String(x?.message ?? x));
      setErro(cancelou ? 'Nenhuma impressora foi escolhida.'
        : porta ? 'O Windows não conseguiu abrir a porta USB escolhida. Confira: (1) feche o aplicativo da Niimbot e outras abas que usem a impressora; (2) na lista, escolha a porta que aparece quando a impressora é ligada (Gerenciador de Dispositivos > Portas COM); (3) desligue e ligue a impressora e tente de novo; (4) se nenhuma porta nova aparece, o cabo pode ser só de carga ou falta o driver: use o Bluetooth.'
        : `Não foi possível conectar na impressora: ${x?.message ?? x}`);
      const tratado = new Error(String(x?.message ?? x));
      tratado.jaTratado = true;   // a mensagem para o operador já foi mostrada
      throw tratado;
    }
  }, []);

  const imprimir = useCallback(async (canvas) => {
    const lib = await carregarLib();
    let cliente = clienteRef.current;
    if (!cliente || !cliente.isConnected()) cliente = await conectar(viaRef.current);
    const meta = cliente.getModelMetadata();
    const imagem = lib.ImageEncoder.encodeCanvas(canvas, lib.PageColorType.SingleColor, meta?.printDirection ?? 'top');
    const tarefa = cliente.protocol.newPrintTask(cliente.getPrintTaskType() ?? 'B1', {
      totalPages: 1, density: meta?.densityDefault ?? 3, labelType: lib.LabelType.WithGaps,
    });
    try {
      await tarefa.printInit();
      await tarefa.printPage(imagem, 1);
      await tarefa.waitForPageFinished();
      await tarefa.waitForFinished();
    } finally {
      try { await cliente.protocol.printEnd(); } catch { /* a impressora pode já ter encerrado */ }
    }
  }, [conectar]);

  const desconectar = useCallback(async () => {
    try { await clienteRef.current?.disconnect(); } catch { /* já desconectada */ }
    clienteRef.current = null;
    setEstado({ status: 'desconectada', nome: '', modelo: '' });
  }, []);

  return { estado, erro, setErro, conectar, imprimir, desconectar, temBluetooth, temUsb };
}

function Passos({ passo }) {
  const nomes = ['Produto', 'Peso', 'Confirmar'];
  return (
    <ol className="mb-4 flex items-center gap-2 text-sm">
      {nomes.map((n, i) => (
        <li key={n} className={`flex items-center gap-2 rounded-full px-3 py-1 ${passo === i + 1 ? 'bg-marca text-white' : passo > i + 1 ? 'bg-emerald-100 text-emerald-800' : 'bg-slate-100 text-slate-500'}`}>
          <span className="font-semibold">{i + 1}</span>{n}
        </li>
      ))}
    </ol>
  );
}

export function Balanca() {
  const [passo, setPasso] = useState(1); // 1 código, 2 peso, 3 confirmação, 4 impresso
  const [codigo, setCodigo] = useState('');
  const [produto, setProduto] = useState(null);
  const [pesoTxt, setPesoTxt] = useState('');
  const [erro, setErro] = useState('');
  const [buscando, setBuscando] = useState(false);
  const [imprimindo, setImprimindo] = useState(false);
  const [altura, setAltura] = useState(lerAltura);
  const previa = useRef(null);
  const campo = useRef(null);
  const imp = useImpressora();

  useEffect(() => { campo.current?.focus(); }, [passo]);

  const gramas = lerPesoEmGramas(pesoTxt);
  const precoKgCentavos = produto ? paraCentavos(produto.precoKg) : 0;
  const total = gramas && produto ? totalEmCentavos(gramas, precoKgCentavos) : 0;

  // etiqueta do passo 3 (a mesma imagem que vai para a impressora)
  let ean = '';
  let erroEtiqueta = '';
  if (passo >= 3 && gramas && produto) {
    try { ean = eanDeBalanca(produto.id, total); } catch (x) { erroEtiqueta = x.message; }
  }
  useEffect(() => {
    if (passo < 3 || !ean || !previa.current) return;
    desenharEtiqueta({ nome: produto.descricao, gramas, precoKgCentavos, totalCentavos: total, ean }, altura, previa.current);
  }, [passo, ean, altura, produto, gramas, precoKgCentavos, total]);

  const buscarProduto = async (e) => {
    e.preventDefault();
    const c = codigo.trim();
    if (!c) { setErro('Digite o código do produto.'); return; }
    setBuscando(true); setErro('');
    try {
      const r = await api.get('/api/publico/balanca/produto', { codigo: c });
      setProduto(r.produto);
      setPasso(2);
    } catch (x) { setErro(x.message); } finally { setBuscando(false); }
  };

  const irParaConfirmacao = (e) => {
    e.preventDefault();
    if (!gramas) { setErro('Peso inválido. Use kg com até 3 casas, por exemplo 1,250.'); return; }
    if (!(precoKgCentavos > 0)) { setErro('Este produto está sem preço por kg no cadastro.'); return; }
    setErro('');
    setPasso(3);
  };

  const imprimir = async () => {
    if (!ean) return;
    setImprimindo(true); setErro('');
    try {
      await imp.imprimir(previa.current);
      setPasso(4);
    } catch (x) {
      if (!x?.jaTratado) setErro(`Não foi possível imprimir: ${x?.message ?? x}`);
    } finally { setImprimindo(false); }
  };

  const novaImpressao = () => {
    setPasso(1); setCodigo(''); setProduto(null); setPesoTxt(''); setErro(''); imp.setErro('');
  };

  const mudarAltura = (v) => {
    setAltura(v);
    try { localStorage.setItem('mp_etiqueta_altura', String(v)); } catch { /* sem armazenamento */ }
  };

  const botao = 'rounded bg-marca px-5 py-2 font-medium text-white hover:bg-marca-escuro disabled:opacity-50';
  const botaoClaro = 'rounded border border-slate-300 bg-white px-4 py-2 text-sm hover:bg-slate-50 disabled:opacity-50';
  const entrada = 'w-full rounded border border-slate-300 px-3 py-3 text-2xl';
  const semSuporte = !imp.temBluetooth && !imp.temUsb;
  const mensagemErro = erro || imp.erro || erroEtiqueta;

  return (
    <div className="mx-auto max-w-xl space-y-4">
      <Secao titulo="Impressora">
        <div className="flex flex-wrap items-center gap-2 text-sm">
          <span className={`inline-flex items-center gap-1 rounded-full px-2 py-0.5 text-xs ${imp.estado.status === 'conectada' ? 'bg-emerald-100 text-emerald-800' : 'bg-slate-100 text-slate-600'}`}>
            <span className={`h-2 w-2 rounded-full ${imp.estado.status === 'conectada' ? 'bg-emerald-500' : 'bg-slate-400'}`} />
            {imp.estado.status === 'conectada' ? `Conectada${imp.estado.nome ? ` · ${imp.estado.nome}` : ''}` : imp.estado.status === 'conectando' ? 'Conectando…' : 'Desconectada'}
          </span>
          {imp.estado.status !== 'conectada' && imp.temBluetooth && <button className={botaoClaro} disabled={imp.estado.status === 'conectando'} onClick={() => imp.conectar('bluetooth').catch(() => {})}>Conectar por Bluetooth</button>}
          {imp.estado.status !== 'conectada' && imp.temUsb && <button className={botaoClaro} disabled={imp.estado.status === 'conectando'} onClick={() => imp.conectar('usb').catch(() => {})}>Conectar por USB</button>}
          {imp.estado.status === 'conectada' && <button className={botaoClaro} onClick={imp.desconectar}>Desconectar</button>}
          <label className="ml-auto flex items-center gap-1 text-xs text-slate-600">Etiqueta
            <select className="rounded border border-slate-300 bg-white px-1 py-0.5" value={altura} onChange={(e) => mudarAltura(Number(e.target.value))}>
              {ALTURAS.map(([nome, px]) => <option key={px} value={px}>50 × {nome.replace(' mm', '')} mm</option>)}
            </select>
          </label>
        </div>
        {semSuporte && <p className="mt-2 rounded bg-amber-50 p-2 text-sm text-amber-900">Este navegador não conecta em impressora. Use o Chrome ou o Edge (computador ou Android) em uma página https.</p>}
      </Secao>

      <Secao titulo="Etiqueta de balança">
        <Passos passo={Math.min(passo, 3)} />

        {passo === 1 && (
          <form onSubmit={buscarProduto} className="space-y-3">
            <label className="block text-sm font-medium">Código do produto
              <input ref={campo} className={entrada} inputMode="numeric" autoComplete="off" value={codigo} onChange={(e) => setCodigo(e.target.value)} />
            </label>
            <button className={botao} disabled={buscando}>{buscando ? 'Buscando…' : 'Próximo'}</button>
          </form>
        )}

        {passo === 2 && produto && (
          <form onSubmit={irParaConfirmacao} className="space-y-3">
            <div className="rounded bg-slate-50 p-3 text-sm">
              <div className="text-base font-semibold">{produto.descricao}</div>
              <div className="text-slate-600">{moedaDeCentavos(precoKgCentavos)} por kg · código {produto.codigoBarras}</div>
              {!unidadeEhQuilo(produto.unidade) && <div className="mt-1 text-amber-800">Atenção: este produto está cadastrado em “{produto.unidade || 'sem unidade'}”, não em KG.</div>}
            </div>
            <label className="block text-sm font-medium">Peso (kg)
              <input ref={campo} className={entrada} inputMode="decimal" autoComplete="off" placeholder="0,000" value={pesoTxt} onChange={(e) => setPesoTxt(e.target.value)} />
            </label>
            <div className="flex gap-2">
              <button type="button" className={botaoClaro} onClick={() => { setPasso(1); setErro(''); }}>Voltar</button>
              <button className={botao}>Próximo</button>
            </div>
          </form>
        )}

        {passo === 3 && produto && gramas && (
          <div className="space-y-3">
            <div className="rounded bg-slate-50 p-3 text-sm">
              <div className="text-base font-semibold">{produto.descricao}</div>
              <div className="text-slate-600">{pesoDeGramas(gramas)} × {moedaDeCentavos(precoKgCentavos)}/kg</div>
              <div className="mt-1 text-3xl font-bold text-marca-escuro">{moedaDeCentavos(total)}</div>
            </div>
            {ean && <canvas ref={previa} className="w-full rounded border border-slate-300 bg-white" style={{ imageRendering: 'pixelated' }} />}
            <div className="flex gap-2">
              <button className={botaoClaro} disabled={imprimindo} onClick={() => { setPasso(2); setErro(''); }}>Voltar</button>
              <button className={botao} disabled={imprimindo || !ean} onClick={imprimir}>{imprimindo ? 'Imprimindo…' : 'Imprimir'}</button>
            </div>
          </div>
        )}

        {passo === 4 && (
          <div className="space-y-3">
            <p className="rounded bg-emerald-50 p-3 text-sm text-emerald-900">Etiqueta enviada para a impressora: {produto?.descricao} · {pesoDeGramas(gramas)} · {moedaDeCentavos(total)}.</p>
            <button className={botao} onClick={novaImpressao}>Nova impressão</button>
          </div>
        )}

        {mensagemErro && <p className="mt-3 rounded bg-red-50 p-2 text-sm text-red-800">{mensagemErro}</p>}
      </Secao>
    </div>
  );
}
