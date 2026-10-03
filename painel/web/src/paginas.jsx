import { useState } from 'react';
import { api, dataBr, dataHoraLoja, horaBr, moeda } from './api.js';
import { Barras, Cartao, Estado, Periodo, periodoInicial, Secao, Selo, Tabela, useDados } from './ui.jsx';

const dinheiroCol = (titulo, f) => ({ titulo, direita: true, render: (l) => moeda(f(l)) });

// ------------------------------------------------------------------ visão geral
export function VisaoGeral({ empresa, ir }) {
  const r = useDados('/api/painel/resumo', { empresa });
  return (
    <Estado r={r}>
      {(d) => (
        <div className="space-y-4">
          <div className="grid grid-cols-2 gap-3 lg:grid-cols-4">
            <Cartao titulo="Vendas hoje" valor={moeda(d.vendasHoje.total)} detalhe={`${d.vendasHoje.qtd} vendas`} onClick={() => ir('vendas')} />
            <Cartao titulo="Vendas no mês" valor={moeda(d.vendasMes.total)} detalhe={`${d.vendasMes.qtd} vendas`} onClick={() => ir('vendas')} />
            <Cartao titulo="Recebido hoje" valor={moeda(d.recebidoHoje)} detalhe="à vista + fiado recebido" tom="bom" />
            <Cartao titulo="A receber (fiado)" valor={moeda(d.aReceber.total)} detalhe={`${d.aReceber.clientes} clientes`} tom={d.aReceber.total > 0 ? 'alerta' : 'normal'} onClick={() => ir('receber')} />
            <Cartao titulo="Contas vencidas" valor={moeda(d.aPagar.vencidas.valor)} detalhe={`${d.aPagar.vencidas.qtd} contas`} tom={d.aPagar.vencidas.qtd ? 'ruim' : 'normal'} onClick={() => ir('pagar')} />
            <Cartao titulo="Vencem hoje" valor={moeda(d.aPagar.hoje.valor)} detalhe={`${d.aPagar.hoje.qtd} contas`} tom={d.aPagar.hoje.qtd ? 'alerta' : 'normal'} onClick={() => ir('pagar')} />
            <Cartao titulo="Próximos 7 dias" valor={moeda(d.aPagar.proximos7Dias.valor)} detalhe={`${d.aPagar.proximos7Dias.qtd} contas`} onClick={() => ir('pagar')} />
            <Cartao titulo="Diferença de caixa (mês)" valor={moeda(d.diferencaCaixaMes)} tom={Math.abs(d.diferencaCaixaMes) > 0.004 ? 'alerta' : 'normal'} onClick={() => ir('vendas')} />
          </div>
          <Secao titulo="Caixas abertos agora">
            <Tabela vazio="Nenhum caixa aberto." linhas={d.caixasAbertos} colunas={[
              { titulo: 'Caixa', render: (l) => l.terminal || `#${l.id}` },
              { titulo: 'Operador', render: (l) => l.operador ?? '—' },
              { titulo: 'Aberto em', render: (l) => dataHoraLoja(l.aberto_em) },
            ]} />
          </Secao>
          {d.produtosSemEstoque > 0 && (
            <p className="rounded-lg bg-amber-50 p-3 text-sm text-amber-900">
              {d.produtosSemEstoque} produtos estão sem estoque. <button className="underline" onClick={() => ir('estoque')}>Ver estoque</button>
            </p>
          )}
        </div>
      )}
    </Estado>
  );
}

// ------------------------------------------------------------------ vendas
export function Vendas({ empresa }) {
  const [per, setPer] = useState(periodoInicial());
  const [agrupar, setAgrupar] = useState('dia');
  const serie = useDados('/api/painel/vendas', { ...per, agrupar, empresa });
  const lista = useDados('/api/painel/vendas/lista', { ...per, empresa });
  const grupos = [['dia', 'Por dia'], ['mes', 'Por mês'], ['hora', 'Por hora'], ['forma', 'Por forma'], ['operador', 'Por operador']];
  return (
    <div className="space-y-4">
      <Periodo valor={per} onChange={setPer} />
      <Secao titulo="Vendas" acao={
        <select className="rounded border border-slate-300 bg-white px-2 py-1 text-sm" value={agrupar} onChange={(e) => setAgrupar(e.target.value)}>
          {grupos.map(([v, t]) => <option key={v} value={v}>{t}</option>)}
        </select>}>
        <Estado r={serie}>
          {(d) => {
            const total = d.serie.reduce((s, x) => s + x.total, 0);
            const qtd = d.serie.reduce((s, x) => s + x.qtd, 0);
            return (
              <>
                <p className="mb-3 text-sm text-slate-600">{qtd} vendas · <b>{moeda(total)}</b> · ticket médio {moeda(qtd ? total / qtd : 0)}</p>
                {d.serie.length === 0 ? <p className="py-6 text-center text-sm text-slate-500">Sem vendas no período.</p> : (
                  <Barras itens={d.serie.map((x) => ({ rotulo: agrupar === 'dia' ? dataBr(x.chave) : agrupar === 'hora' ? `${x.chave}h` : x.chave, valor: x.total }))} formato={moeda} />
                )}
              </>
            );
          }}
        </Estado>
      </Secao>
      <Secao titulo="Últimas vendas do período">
        <Estado r={lista}>
          {(d) => (
            <Tabela linhas={d.vendas} vazio="Sem vendas no período." colunas={[
              { titulo: 'Nº', render: (l) => l.id },
              { titulo: 'Data', render: (l) => dataHoraLoja(l.data_hora) },
              { titulo: 'Operador', render: (l) => l.operador ?? '—' },
              { titulo: 'Forma', render: (l) => <>{l.forma_pagamento ?? '—'} {l.forma_pagamento === 'Prazo' && !l.esta_pago && <Selo texto="fiado" tom="amarelo" />}</> },
              dinheiroCol('Valor', (l) => l.valor_final),
            ]} />
          )}
        </Estado>
      </Secao>
    </div>
  );
}

// ------------------------------------------------------------------ operadores
export function Operadores({ empresa }) {
  const [per, setPer] = useState(periodoInicial());
  const r = useDados('/api/painel/operadores', { ...per, empresa });
  return (
    <div className="space-y-4">
      <Periodo valor={per} onChange={setPer} />
      <Secao titulo="Desempenho por operador">
        <Estado r={r}>
          {(d) => (
            <Tabela linhas={d.operadores} colunas={[
              { titulo: 'Operador', render: (l) => <>{l.nome} {l.gerente && <Selo texto="gerente" tom="azul" />} {!l.ativo && <Selo texto="inativo" />}</> },
              { titulo: 'Vendas', direita: true, render: (l) => l.vendas },
              dinheiroCol('Total', (l) => l.total),
              dinheiroCol('Ticket médio', (l) => l.ticketMedio),
              { titulo: 'Cancel.', direita: true, render: (l) => (l.cancelamentos ? <span className="text-red-700">{l.cancelamentos} · {moeda(l.valorCancelado)}</span> : '—') },
              { titulo: 'Horas logado', direita: true, render: (l) => `${l.horasLogado} h` },
              { titulo: 'Dif. de caixa', direita: true, render: (l) => (l.caixasFechados ? <span className={Math.abs(l.diferencaCaixas) > 0.004 ? 'text-amber-700' : ''}>{moeda(l.diferencaCaixas)}</span> : '—') },
            ]} />
          )}
        </Estado>
      </Secao>
    </div>
  );
}

// ------------------------------------------------------------------ caixas
export function Caixas() {
  const [per, setPer] = useState({ ...periodoInicial() });
  const r = useDados('/api/painel/caixas', per);
  const duracao = (min) => (min == null ? '—' : `${Math.floor(min / 60)}h${String(min % 60).padStart(2, '0')}`);
  return (
    <div className="space-y-4">
      <Periodo valor={per} onChange={setPer} />
      <Secao titulo="Abertura e fechamento de caixa">
        <Estado r={r}>
          {(d) => (
            <Tabela linhas={d.caixas} vazio="Nenhum caixa no período." colunas={[
              { titulo: 'Caixa', render: (l) => l.terminal || `#${l.id}` },
              { titulo: 'Operador', render: (l) => l.operador ?? '—' },
              { titulo: 'Abriu', render: (l) => dataHoraLoja(l.aberto_em) },
              { titulo: 'Fechou', render: (l) => (l.status === 'ABERTO' ? <Selo texto="aberto" tom="verde" /> : dataHoraLoja(l.fechado_em)) },
              { titulo: 'Duração', render: (l) => duracao(l.minutos) },
              dinheiroCol('Vendas', (l) => l.vendas),
              dinheiroCol('Sangrias', (l) => l.sangrias),
              dinheiroCol('Esperado', (l) => l.esperado),
              dinheiroCol('Contado', (l) => l.contado),
              { titulo: 'Diferença', direita: true, render: (l) => (l.status === 'ABERTO' ? '—' : <span className={Math.abs(l.diferenca) > 0.004 ? 'font-medium text-amber-700' : ''}>{moeda(l.diferenca)}</span>) },
            ]} />
          )}
        </Estado>
      </Secao>
    </div>
  );
}

// ------------------------------------------------------------------ devedores
export function Devedores({ empresa }) {
  const r = useDados('/api/painel/devedores', { empresa });
  return (
    <Estado r={r}>
      {(d) => (
        <div className="space-y-4">
          <div className="grid grid-cols-2 gap-3 lg:grid-cols-5">
            <Cartao titulo="Total a receber" valor={moeda(d.total)} tom={d.total > 0 ? 'alerta' : 'normal'} />
            <Cartao titulo="Até 30 dias" valor={moeda(d.faixas.ate30)} />
            <Cartao titulo="31 a 60 dias" valor={moeda(d.faixas.de31a60)} />
            <Cartao titulo="61 a 90 dias" valor={moeda(d.faixas.de61a90)} />
            <Cartao titulo="Mais de 90 dias" valor={moeda(d.faixas.mais90)} tom={d.faixas.mais90 > 0 ? 'ruim' : 'normal'} />
          </div>
          <Secao titulo="Clientes devedores">
            <Tabela linhas={d.devedores} vazio="Ninguém está devendo." colunas={[
              { titulo: 'Cliente', render: (l) => l.nome },
              { titulo: 'Telefone', render: (l) => l.telefone ?? '—' },
              dinheiroCol('Deve', (l) => l.total),
              { titulo: 'Limite', direita: true, render: (l) => (l.limite == null ? '—' : <span className={l.acimaDoLimite ? 'font-medium text-red-700' : ''}>{moeda(l.limite)}</span>) },
              { titulo: 'Mais antiga', render: (l) => <>{dataBr(l.maisAntiga)} <span className="text-xs text-slate-500">({l.idadeDias} d)</span></> },
              { titulo: 'Última compra', render: (l) => dataBr(l.ultimaCompra) },
            ]} />
          </Secao>
        </div>
      )}
    </Estado>
  );
}

// ------------------------------------------------------------------ contas a pagar (com ações)
const STATUS_TOM = { ABERTA: 'amarelo', PAGA: 'verde', CANCELADA: 'cinza', VENCIDA: 'vermelho' };

export function ContasPagar(props) {
  return <Agrupada abas={[['c', 'Contas e boletos', <ListaContas {...props} />], ['f', 'Calendário e fluxo', <Fluxo empresa={props.empresa} />]]} />;
}

function ListaContas({ empresa, empresas, formas, podeAgir, avisar }) {
  const [status, setStatus] = useState('ABERTA');
  const r = useDados('/api/painel/contas-pagar', { status, empresa });
  const [baixando, setBaixando] = useState(null);
  const [lancando, setLancando] = useState(false);
  return (
    <div className="space-y-4">
      <div className="flex flex-wrap items-center justify-between gap-2">
        <div className="flex gap-1 text-sm">
          {[['ABERTA', 'Em aberto'], ['VENCIDA', 'Vencidas'], ['PAGA', 'Pagas'], ['TODAS', 'Todas']].map(([v, t]) => (
            <button key={v} onClick={() => setStatus(v)} className={`rounded-full px-3 py-1 ring-1 ${status === v ? 'bg-marca text-white ring-marca' : 'bg-white ring-slate-200'}`}>{t}</button>
          ))}
        </div>
        {podeAgir && <button onClick={() => setLancando(true)} className="rounded-lg bg-marca px-3 py-2 text-sm font-medium text-white hover:bg-marca-escuro">+ Lançar conta</button>}
      </div>
      <Estado r={r}>
        {(d) => (
          <>
            {status === 'ABERTA' && d.porCategoria.length > 0 && (
              <Secao titulo="Em aberto por categoria"><Barras itens={d.porCategoria.map((c) => ({ rotulo: `${c.categoria} (${c.qtd})`, valor: c.total }))} formato={moeda} /></Secao>
            )}
            <Secao titulo="Contas">
              <Tabela linhas={d.contas} vazio="Nenhuma conta nesse filtro." colunas={[
                { titulo: 'Vencimento', render: (l) => dataBr(l.vencimento) },
                { titulo: 'Descrição', render: (l) => <>{l.descricao}{l.total_parcelas > 1 && <span className="text-xs text-slate-500"> {l.parcela}/{l.total_parcelas}</span>}</> },
                { titulo: 'Tipo', render: (l) => <>{l.categoria ?? '—'}{l.documento && <div className="text-xs text-slate-500">{l.documento}</div>}</> },
                { titulo: 'Situação', render: (l) => {
                  const vencida = l.status === 'ABERTA' && l.vencimento < d.hoje;
                  return <Selo texto={vencida ? 'VENCIDA' : l.status} tom={STATUS_TOM[vencida ? 'VENCIDA' : l.status] ?? 'cinza'} />;
                } },
                dinheiroCol('Valor', (l) => l.valor),
                { titulo: '', direita: true, render: (l) => (podeAgir && l.status === 'ABERTA' ? <button className="rounded border border-marca px-2 py-1 text-xs text-marca hover:bg-marca-claro" onClick={() => setBaixando(l)}>Marcar como paga</button> : l.status === 'PAGA' ? <span className="text-xs text-slate-500">{moeda(l.valor_pago)} · {l.forma_pagamento}</span> : null) },
              ]} />
            </Secao>
          </>
        )}
      </Estado>
      {baixando && <ModalBaixa conta={baixando} formas={formas} onFechar={() => setBaixando(null)} onEnviado={() => { setBaixando(null); avisar('Pedido enviado. Será aplicado quando o programa da loja estiver aberto.'); r.recarregar(); }} />}
      {lancando && <ModalLancar empresas={empresas} empresaPadrao={empresa} onFechar={() => setLancando(false)} onEnviado={() => { setLancando(false); avisar('Pedido enviado. A conta aparece aqui depois que a loja aplicar.'); }} />}
    </div>
  );
}

function Modal({ titulo, onFechar, children }) {
  return (
    <div className="fixed inset-0 z-50 flex items-end justify-center bg-black/40 p-0 sm:items-center sm:p-4" onClick={onFechar}>
      <div className="max-h-[90vh] w-full max-w-md overflow-y-auto rounded-t-2xl bg-white p-5 shadow-xl sm:rounded-2xl" onClick={(e) => e.stopPropagation()}>
        <h3 className="mb-4 text-lg font-semibold">{titulo}</h3>
        {children}
      </div>
    </div>
  );
}

const Campo = ({ rotulo, children }) => <label className="mb-3 block text-sm"><span className="mb-1 block text-slate-600">{rotulo}</span>{children}</label>;
const entrada = 'w-full rounded border border-slate-300 px-3 py-2';

function useEnvio(onEnviado) {
  const [erro, setErro] = useState('');
  const [enviando, setEnviando] = useState(false);
  const [chave] = useState(() => crypto.randomUUID());   // duplo clique devolve o mesmo pedido
  const enviar = async (tipo, payload) => {
    setEnviando(true); setErro('');
    try { await api.post('/api/painel/pedidos', { tipo, payload, chave }); onEnviado(); }
    catch (e) { setErro(e.message); setEnviando(false); }
  };
  return { erro, enviando, enviar };
}

function ModalBaixa({ conta, formas, onFechar, onEnviado }) {
  const [valor, setValor] = useState(String(conta.valor));
  const [forma, setForma] = useState(formas[1] ?? formas[0]);
  const [data, setData] = useState(new Date().toLocaleDateString('sv-SE'));
  const { erro, enviando, enviar } = useEnvio(onEnviado);
  return (
    <Modal titulo="Marcar conta como paga" onFechar={onFechar}>
      <p className="mb-4 text-sm text-slate-600">{conta.descricao} · vence {dataBr(conta.vencimento)} · {moeda(conta.valor)}</p>
      <Campo rotulo="Valor pago (R$)"><input className={entrada} inputMode="decimal" value={valor} onChange={(e) => setValor(e.target.value)} /></Campo>
      <Campo rotulo="Forma de pagamento"><select className={entrada} value={forma} onChange={(e) => setForma(e.target.value)}>{formas.map((f) => <option key={f}>{f}</option>)}</select></Campo>
      <Campo rotulo="Data do pagamento"><input type="date" className={entrada} value={data} onChange={(e) => setData(e.target.value)} /></Campo>
      {erro && <p className="mb-3 rounded bg-red-50 p-2 text-sm text-red-800">{erro}</p>}
      <p className="mb-4 text-xs text-slate-500">O pedido vai para a loja e é aplicado quando o programa estiver aberto (vale por 24 h).</p>
      <div className="flex justify-end gap-2">
        <button className="rounded px-3 py-2 text-sm" onClick={onFechar}>Cancelar</button>
        <button disabled={enviando} className="rounded bg-marca px-4 py-2 text-sm font-medium text-white disabled:opacity-50"
          onClick={() => enviar('CONTA_PAGAR_BAIXAR', { contaId: Number(conta.id), valorPago: Number(String(valor).replace(',', '.')), forma, data })}>Enviar pedido</button>
      </div>
    </Modal>
  );
}

function ModalLancar({ empresas, empresaPadrao, onFechar, onEnviado }) {
  const [f, setF] = useState({ descricao: '', fornecedor: '', categoria: '', documento: '', valor: '', vencimento: new Date().toLocaleDateString('sv-SE'), idEmpresa: empresaPadrao ?? empresas[0]?.id, parcelas: 1 });
  const { erro, enviando, enviar } = useEnvio(onEnviado);
  const set = (k) => (e) => setF({ ...f, [k]: e.target.value });
  return (
    <Modal titulo="Lançar nova conta a pagar" onFechar={onFechar}>
      <Campo rotulo="Descrição"><input className={entrada} value={f.descricao} onChange={set('descricao')} /></Campo>
      <div className="grid grid-cols-2 gap-3">
        <Campo rotulo="Fornecedor"><input className={entrada} value={f.fornecedor} onChange={set('fornecedor')} /></Campo>
        <Campo rotulo="Tipo / categoria"><input className={entrada} list="cats" placeholder="Boleto, Aluguel…" value={f.categoria} onChange={set('categoria')} /><datalist id="cats">{['Boleto', 'Fornecedor', 'Aluguel', 'Energia', 'Água', 'Internet', 'Impostos', 'Salários', 'Outros'].map((c) => <option key={c} value={c} />)}</datalist></Campo>
        <Campo rotulo="Nº do boleto / documento"><input className={entrada} value={f.documento} onChange={set('documento')} /></Campo>
        <Campo rotulo="Valor de cada parcela (R$)"><input className={entrada} inputMode="decimal" value={f.valor} onChange={set('valor')} /></Campo>
        <Campo rotulo="Parcelas"><input type="number" min="1" max="60" className={entrada} value={f.parcelas} onChange={set('parcelas')} /></Campo>
        <Campo rotulo="1º vencimento"><input type="date" className={entrada} value={f.vencimento} onChange={set('vencimento')} /></Campo>
        <Campo rotulo="Empresa"><select className={entrada} value={f.idEmpresa} onChange={set('idEmpresa')}>{empresas.map((e) => <option key={e.id} value={e.id}>{e.apelido}</option>)}</select></Campo>
      </div>
      {erro && <p className="mb-3 rounded bg-red-50 p-2 text-sm text-red-800">{erro}</p>}
      <div className="flex justify-end gap-2">
        <button className="rounded px-3 py-2 text-sm" onClick={onFechar}>Cancelar</button>
        <button disabled={enviando} className="rounded bg-marca px-4 py-2 text-sm font-medium text-white disabled:opacity-50"
          onClick={() => enviar('CONTA_PAGAR_LANCAR', {
            descricao: f.descricao, fornecedor: f.fornecedor || undefined, categoria: f.categoria || undefined, documento: f.documento || undefined,
            valor: Number(String(f.valor).replace(',', '.')), vencimento: f.vencimento, idEmpresa: Number(f.idEmpresa), parcelas: Number(f.parcelas),
          })}>Enviar pedido</button>
      </div>
    </Modal>
  );
}

// ------------------------------------------------------------------ fluxo
export function Fluxo({ empresa }) {
  const [dias, setDias] = useState(30);
  const r = useDados('/api/painel/fluxo', { empresa, dias });
  return (
    <Estado r={r}>
      {(d) => (
        <div className="space-y-4">
          <div className="flex gap-1 text-sm">{[7, 30, 60, 90].map((n) => <button key={n} onClick={() => setDias(n)} className={`rounded-full px-3 py-1 ring-1 ${dias === n ? 'bg-marca text-white ring-marca' : 'bg-white ring-slate-200'}`}>{n} dias</button>)}</div>
          <div className="grid grid-cols-2 gap-3 lg:grid-cols-3">
            <Cartao titulo={`A pagar em ${d.dias} dias`} valor={moeda(d.aPagarNoPeriodo)} tom="alerta" />
            <Cartao titulo="A receber (fiado)" valor={moeda(d.aReceberTotal)} />
            <Cartao titulo="Venda média diária (30 d)" valor={moeda(d.vendaMediaDiaria)} detalhe={`≈ ${moeda(d.vendaMediaDiaria * d.dias)} em ${d.dias} dias`} tom="bom" />
          </div>
          <Secao titulo="Contas a pagar por dia">
            {d.aPagarPorDia.length === 0 ? <p className="py-6 text-center text-sm text-slate-500">Nada a pagar no período.</p>
              : <Barras itens={d.aPagarPorDia.map((x) => ({ rotulo: `${dataBr(x.dia)}${x.dia < d.hoje ? ' (venc.)' : ''}`, valor: x.total }))} formato={moeda} />}
          </Secao>
        </div>
      )}
    </Estado>
  );
}

// ------------------------------------------------------------------ estoque
export function Estoque() {
  const r = useDados('/api/painel/estoque', {});
  return (
    <Estado r={r}>
      {(d) => (
        <div className="space-y-4">
          <BuscaProduto />
          <div className="grid grid-cols-2 gap-3 lg:grid-cols-3">
            <Cartao titulo="Produtos cadastrados" valor={d.resumo.produtos} />
            <Cartao titulo="Estoque a preço de venda" valor={moeda(d.resumo.valorAPrecoDeVenda)} />
            <Cartao titulo="Estoque a custo" valor={moeda(d.resumo.valorACusto)} />
          </div>
          <Secao titulo={`Estoque baixo (até ${d.limite} un.)`}>
            <Tabela linhas={d.baixoEstoque} vazio="Nenhum produto com estoque baixo." colunas={[
              { titulo: 'Produto', render: (l) => l.descricao },
              { titulo: 'Qtd', direita: true, render: (l) => <span className={l.quantidade <= 0 ? 'font-medium text-red-700' : ''}>{l.quantidade}</span> },
              dinheiroCol('Preço', (l) => l.preco),
            ]} />
          </Secao>
          <Secao titulo="Mais vendidos (30 dias)">
            <Tabela linhas={d.maisVendidos30Dias} vazio="Sem vendas." colunas={[
              { titulo: 'Produto', render: (l) => l.descricao }, { titulo: 'Qtd', direita: true, render: (l) => l.quantidade }, dinheiroCol('Total', (l) => l.total),
            ]} />
          </Secao>
          <Secao titulo="Parados há 60 dias (com estoque)">
            <Tabela linhas={d.parados60Dias} vazio="Nenhum produto parado." colunas={[
              { titulo: 'Produto', render: (l) => l.descricao }, { titulo: 'Qtd', direita: true, render: (l) => l.quantidade }, dinheiroCol('Preço', (l) => l.preco),
            ]} />
          </Secao>
        </div>
      )}
    </Estado>
  );
}

// ------------------------------------------------------------------ fiscal
export function Fiscal() {
  const [per, setPer] = useState(periodoInicial());
  const r = useDados('/api/painel/fiscal', per);
  const tom = (c) => (c === '100' ? 'verde' : c === 'CONTINGENCIA' ? 'amarelo' : 'vermelho');
  return (
    <div className="space-y-4">
      <Periodo valor={per} onChange={setPer} />
      <Estado r={r}>
        {(d) => (
          <>
            {d.emContingencia > 0 && <p className="rounded-lg bg-amber-50 p-3 text-sm text-amber-900">{d.emContingencia} notas em contingência aguardando transmissão.</p>}
            <Secao titulo="Notas por situação">
              <Tabela linhas={d.grupos} vazio="Sem notas no período." colunas={[
                { titulo: 'Modelo', render: (l) => (l.modelo === '65' ? 'NFC-e' : l.modelo === '55' ? 'NF-e' : l.modelo) },
                { titulo: 'Finalidade', render: (l) => l.finalidade },
                { titulo: 'Situação (cStat)', render: (l) => <Selo texto={l.cstat} tom={tom(l.cstat)} /> },
                { titulo: 'Ambiente', render: (l) => (String(l.tp_amb) === '1' ? 'Produção' : 'Homologação') },
                { titulo: 'Qtd', direita: true, render: (l) => l.qtd },
                dinheiroCol('Total', (l) => l.total),
              ]} />
            </Secao>
          </>
        )}
      </Estado>
    </div>
  );
}

// ------------------------------------------------------------------ segurança
export function Seguranca() {
  const [per, setPer] = useState(periodoInicial());
  const r = useDados('/api/painel/seguranca', per);
  return (
    <div className="space-y-4">
      <Periodo valor={per} onChange={setPer} />
      <Estado r={r}>
        {(d) => (
          <>
            <div className="grid grid-cols-2 gap-3 lg:grid-cols-4">
              <Cartao titulo="Cancelamentos" valor={d.cancelamentos.qtd} detalhe={moeda(d.cancelamentos.total)} tom={d.cancelamentos.qtd ? 'alerta' : 'normal'} />
              {d.resumo.slice(0, 3).map((x) => <Cartao key={x.acao} titulo={x.acao.replaceAll('_', ' ').toLowerCase()} valor={x.qtd} />)}
            </div>
            <Secao titulo="Eventos de acesso e exceções">
              <Tabela linhas={d.eventos} vazio="Nenhum evento no período." colunas={[
                { titulo: 'Quando', render: (l) => dataHoraLoja(l.data_hora) },
                { titulo: 'Operador', render: (l) => l.nome_operador ?? '—' },
                { titulo: 'Caixa', render: (l) => l.terminal ?? '—' },
                { titulo: 'Ação', render: (l) => l.acao },
                { titulo: 'Detalhe', render: (l) => l.detalhe ?? '' },
              ]} />
            </Secao>
          </>
        )}
      </Estado>
    </div>
  );
}

// ------------------------------------------------------------------ pedidos enviados à loja
const PEDIDO_TOM = { PENDENTE: 'amarelo', ENTREGUE: 'azul', APLICADO: 'verde', RECUSADO: 'vermelho', CONFLITO: 'vermelho', EXPIRADO: 'cinza', CANCELADO: 'cinza' };
const PEDIDO_TIPO = { CONTA_PAGAR_BAIXAR: 'Baixar conta a pagar', CONTA_PAGAR_LANCAR: 'Lançar conta a pagar' };

export function Pedidos({ podeAgir }) {
  const r = useDados('/api/painel/pedidos', {});
  const cancelar = async (id) => { await api.post(`/api/painel/pedidos/${id}/cancelar`); r.recarregar(); };
  const resumo = (p) => (p.tipo === 'CONTA_PAGAR_BAIXAR'
    ? `Conta ${p.payload.contaId} · ${moeda(p.payload.valorPago)} · ${p.payload.forma}`
    : `${p.payload.descricao} · ${moeda(p.payload.valor)}${p.payload.parcelas > 1 ? ` × ${p.payload.parcelas}` : ''}`);
  return (
    <Secao titulo="Pedidos enviados à loja" acao={<button className="text-sm text-marca underline" onClick={r.recarregar}>Atualizar</button>}>
      <Estado r={r}>
        {(d) => (
          <Tabela linhas={d.pedidos} vazio="Nenhum pedido enviado ainda." colunas={[
            { titulo: 'Enviado', render: (l) => horaBr(l.criado_em) },
            { titulo: 'Quem', render: (l) => l.solicitante_nome },
            { titulo: 'Pedido', render: (l) => <><b>{PEDIDO_TIPO[l.tipo] ?? l.tipo}</b><div className="text-xs text-slate-500">{resumo(l)}</div></> },
            { titulo: 'Situação', render: (l) => <Selo texto={l.status} tom={PEDIDO_TOM[l.status]} /> },
            { titulo: 'Resultado', render: (l) => <span className="whitespace-normal text-xs">{l.resultado_mensagem ?? (l.status === 'PENDENTE' ? `Vale até ${horaBr(l.expira_em)}` : '')}</span> },
            { titulo: '', direita: true, render: (l) => (podeAgir && l.status === 'PENDENTE' ? <button className="text-xs text-red-700 underline" onClick={() => cancelar(l.id)}>Cancelar</button> : null) },
          ]} />
        )}
      </Estado>
    </Secao>
  );
}

// ------------------------------------------------------------------ sincronização
export function Sincronizacao() {
  const r = useDados('/api/painel/sincronizacao', {});
  return (
    <Secao titulo="Envios da loja para a nuvem">
      <Estado r={r}>
        {(d) => (
          <Tabela linhas={d.ciclos} vazio="A loja ainda não enviou dados." colunas={[
            { titulo: 'Início', render: (l) => horaBr(l.iniciado_em) },
            { titulo: 'Fim', render: (l) => horaBr(l.concluido_em) },
            { titulo: 'Computador', render: (l) => l.terminal ?? '—' },
            { titulo: 'Situação', render: (l) => <Selo texto={l.status} tom={l.status === 'CONCLUIDO' ? 'verde' : l.status === 'ERRO' ? 'vermelho' : 'cinza'} /> },
            { titulo: 'Linhas', direita: true, render: (l) => l.linhas ?? '—' },
            { titulo: 'Erro', render: (l) => <span className="whitespace-normal text-xs text-red-700">{l.erro ?? ''}</span> },
          ]} />
        )}
      </Estado>
    </Secao>
  );
}

export function Abas({ aba, onChange, abas, children }) {
  return (
    <div className="space-y-4">
      <div className="flex gap-1 overflow-x-auto border-b border-slate-200 text-sm">
        {abas.map(([id, nome]) => (
          <button key={id} onClick={() => onChange(id)} className={`-mb-px whitespace-nowrap border-b-2 px-4 py-2 ${aba === id ? 'border-marca font-medium text-marca' : 'border-transparent text-slate-500 hover:text-slate-800'}`}>{nome}</button>
        ))}
      </div>
      {children}
    </div>
  );
}

export function Agrupada({ abas }) {
  const [aba, setAba] = useState(abas[0][0]);
  return <Abas aba={aba} onChange={setAba} abas={abas}>{abas.find(([id]) => id === aba)[2]}</Abas>;
}

// ------------------------------------------------------------------ busca de produto (nome ou código de barras)
function BuscaProduto() {
  const [texto, setTexto] = useState('');
  const [escolhido, setEscolhido] = useState(null);
  const lista = useDados('/api/painel/produtos', { q: texto.trim().length >= 2 ? texto.trim() : undefined });
  const achados = lista.dados?.produtos ?? [];
  return (
    <Secao titulo="Consultar produto">
      <input className="w-full rounded border border-slate-300 px-3 py-2 text-sm" placeholder="Digite o nome ou o código de barras…" value={texto} onChange={(e) => { setTexto(e.target.value); setEscolhido(null); }} />
      {texto.trim().length >= 2 && !escolhido && (
        <div className="mt-3">
          <Tabela linhas={achados} vazio="Nenhum produto encontrado." colunas={[
            { titulo: 'Produto', render: (l) => <button className="text-left font-medium text-marca underline" onClick={() => setEscolhido(l.id)}>{l.descricao}</button> },
            { titulo: 'Código de barras', render: (l) => l.codigo_barras ?? '—' },
            { titulo: 'Qtd', direita: true, render: (l) => l.quantidade },
            dinheiroCol('Preço', (l) => l.preco),
          ]} />
        </div>
      )}
      {escolhido && <DetalheProduto id={escolhido} />}
    </Secao>
  );
}

function DetalheProduto({ id }) {
  const r = useDados('/api/painel/produtos', { id });
  return (
    <div className="mt-3">
      <Estado r={r}>
        {({ produto: p, vendas: v }) => (
          <div className="space-y-3">
            <div>
              <div className="text-lg font-semibold">{p.descricao}</div>
              <div className="text-xs text-slate-500">Código de barras: {p.codigoBarras ?? '—'} · Local: {p.local ?? '—'} · Unidade: {p.unidade ?? '—'} {p.fiscal && <Selo texto="com nota fiscal" tom="azul" />}</div>
            </div>
            <div className="grid grid-cols-2 gap-3 lg:grid-cols-4">
              <Cartao titulo="Em estoque" valor={p.quantidade} tom={p.quantidade <= 0 ? 'ruim' : 'normal'} detalhe={`valor ${moeda(p.valorEmEstoque)}`} />
              <Cartao titulo="Preço de venda" valor={moeda(p.preco)} />
              <Cartao titulo="Custo" valor={p.custo == null ? '—' : moeda(p.custo)} detalhe={p.margemPercentual == null ? undefined : `margem ${p.margemPercentual}%`} />
              <Cartao titulo="Vendido em 30 dias" valor={v.qtd30} detalhe={`${moeda(v.total30)} · 90 dias: ${v.qtd90}`} />
            </div>
            <p className="text-xs text-slate-500">Última venda: {v.ultimaVenda ? dataHoraLoja(v.ultimaVenda) : 'nunca vendido'} · Atualizado em {dataHoraLoja(p.atualizadoEm)}</p>
          </div>
        )}
      </Estado>
    </div>
  );
}
