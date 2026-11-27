#include "entrega_service.h"
#include <QDateTime>
#include <QDebug>
#include <cmath>

namespace {
double arredonda2(double v) { return std::round(v * 100.0) / 100.0; }
double arredonda3(double v) { return std::round(v * 1000.0) / 1000.0; }

QString codigoBarrasDoItem(qlonglong idItem)
{
    return QString("ENT%1").arg(idItem, 5, 10, QLatin1Char('0'));
}
}

Entrega_service::Entrega_service(QObject *parent)
    : QObject(parent)
{
}

double Entrega_service::precoAplicado(const EntregaItemDTO &item, double quantidade)
{
    if (item.temFaixa() && quantidade >= item.qtdFaixa)
        return item.precoFaixa;
    return item.preco;
}

double Entrega_service::totalLinha(const EntregaItemDTO &item, double quantidade)
{
    if (quantidade <= 0)
        return 0;
    return arredonda2(quantidade * precoAplicado(item, quantidade));
}

double Entrega_service::totalPedido(const QList<EntregaLinhaDTO> &linhas)
{
    double total = 0;
    for (const EntregaLinhaDTO &l : linhas)
        total += totalLinha(l.item, l.quantidade);
    return arredonda2(total);
}

double Entrega_service::quantidadePorValor(const EntregaItemDTO &item, double valor)
{
    if (valor <= 0 || item.preco <= 0)
        return 0;

    auto quantos = [&item](double v, double preco) {
        const double q = v / preco;
        // +1e-9 evita perder 1 unidade por erro de ponto flutuante (ex.: 30 / 0,40)
        return item.fracionado() ? arredonda3(q) : std::floor(q + 1e-9);
    };

    double q = quantos(valor, item.preco);
    if (item.temFaixa() && q >= item.qtdFaixa)
        q = quantos(valor, item.precoFaixa);
    return q;
}

QList<EntregaItemDTO> Entrega_service::itensPadrao()
{
    auto item = [](const QString &desc, const QString &un, double preco, int ordem,
                   double qtdFaixa = 0, double precoFaixa = 0) {
        EntregaItemDTO i;
        i.descricao = desc;
        i.unidade = un;
        i.preco = preco;
        i.qtdFaixa = qtdFaixa;
        i.precoFaixa = precoFaixa;
        i.ordem = ordem;
        return i;
    };
    return {
        item("PAO CASEIRO", "un", 0.40, 1, 150, 0.30),
        item("PAO DELICIA SEM RECHEIO - BANDEJA 4 UN", "bandeja", 4.08, 2),
        item("PAO DELICIA COM RECHEIO - BANDEJA 4 UN", "bandeja", 4.80, 3),
        item("MINI ESFIRRA - PRATO 3 UN", "prato", 4.50, 4),
        item("MINI ENROLADINHO - PRATO 3 UN", "prato", 4.50, 5),
        item("PAO PIZZA - PRATO", "prato", 6.00, 6),
        item("PAO DE HOT DOG - PACOTE (KG)", "kg", 14.99, 7),
        item("PAO DE HAMBURGUER - PACOTE (KG)", "kg", 14.99, 8),
    };
}

qlonglong Entrega_service::garantirProduto(const EntregaItemDTO &item)
{
    ProdutoDTO existente = prodServ.getProdutoPelaDescricao(item.descricao);
    if (existente.id > 0)
        return existente.id;

    ProdutoDTO p;
    p.id = 0;
    p.quantidade = 0;
    p.descricao = item.descricao;
    p.preco = item.preco;
    p.codigoBarras = codigoBarrasDoItem(item.id);
    p.nf = false;
    // pão feito na hora: a venda não mexe na quantidade, então o estoque não fica negativo
    p.semControleEstoque = true;
    p.uCom = item.fracionado() ? "KG" : "UN";
    p.precoFornecedor = 0;
    p.percentLucro = 100;
    p.aliquotaIcms = 0;
    p.csosn = "102";
    p.pis = "49";

    auto r = prodServ.inserir(p);
    if (!r.ok) {
        qDebug() << "Entrega: nao criou produto" << item.descricao << r.msg;
        return 0;
    }
    return prodServ.getProdutoPelaDescricao(item.descricao).id;
}

QList<EntregaItemDTO> Entrega_service::carregarItens()
{
    if (repo.contarItens() == 0) {
        for (const EntregaItemDTO &padrao : itensPadrao()) {
            QString erro;
            if (!repo.inserirItem(padrao, &erro))
                qDebug() << "Entrega: nao cadastrou item padrao" << padrao.descricao << erro;
        }
    }

    QList<EntregaItemDTO> itens = repo.listarItens(true);
    for (EntregaItemDTO &i : itens) {
        if (i.idProduto > 0)
            continue;
        const qlonglong idProd = garantirProduto(i);
        if (idProd > 0 && repo.atualizarIdProduto(i.id, idProd))
            i.idProduto = idProd;
    }
    return itens;
}

Entrega_service::Resultado Entrega_service::registrarPedido(const EntregaPedidoDTO &pedido)
{
    Resultado res;

    QList<EntregaLinhaDTO> linhas;
    for (const EntregaLinhaDTO &l : pedido.linhas) {
        if (l.quantidade > 0)
            linhas << l;
    }
    if (linhas.isEmpty()) {
        res.msg = "Informe a quantidade de pelo menos um item.";
        return res;
    }

    QList<ProdutoVendidoDTO> produtos;
    for (const EntregaLinhaDTO &l : linhas) {
        if (l.item.idProduto <= 0) {
            res.msg = QString("O item \"%1\" não está ligado a um produto do estoque.").arg(l.item.descricao);
            return res;
        }
        ProdutoVendidoDTO pv;
        pv.idProduto = l.item.idProduto;
        pv.idVenda = 0;
        pv.quantidade = l.quantidade;
        pv.precoVendido = precoAplicado(l.item, l.quantidade);
        pv.descricao = l.item.descricao;
        produtos << pv;
    }

    const double total = totalPedido(linhas);
    const bool paga = pedido.formaPagamento != "Prazo";

    VendasDTO venda;
    venda.clienteNome = pedido.clienteNome;
    venda.dataHora = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    venda.total = total;
    venda.formaPagamento = pedido.formaPagamento;
    venda.valorRecebido = paga ? total : 0;
    venda.troco = 0;
    venda.taxa = 0;
    venda.valorFinal = total;
    venda.desconto = 0;
    venda.estaPago = paga;
    venda.idCliente = pedido.idCliente;
    venda.idOperadorSessao = pedido.idOperadorSessao;

    auto r = vendasServ.inserirVendaRegraDeNegocio(venda, produtos);
    if (!r.ok) {
        res.msg = r.msg;
        return res;
    }

    res.ok = true;
    res.idVenda = r.idVendaInserida;
    res.msg = "Pedido de entrega lançado.";
    venda.id = res.idVenda;
    res.venda = venda;
    res.produtos = produtos;

    QString erro;
    if (!repo.marcarVendaEntrega(res.idVenda, pedido.endereco, pedido.observacao, &erro)) {
        qDebug() << "Entrega: venda" << res.idVenda << "gravada sem a marca de entrega:" << erro;
        res.avisoMarca = true;
    }
    return res;
}

bool Entrega_service::vendaEhEntrega(qlonglong idVenda)
{
    return repo.vendaEhEntrega(idVenda);
}
