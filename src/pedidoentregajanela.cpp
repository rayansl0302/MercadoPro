#include "pedidoentregajanela.h"
#include "vendas.h"
#include "services/contasreceber_service.h"
#include "services/sessao_service.h"
#include "services/config_service.h"
#include "services/notafiscal_service.h"
#include "services/fiscalemitter_service.h"
#include "services/vendas_service.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStringListModel>
#include <QTableWidget>
#include <QVBoxLayout>

PedidoEntregaJanela::PedidoEntregaJanela(QWidget *parent)
    : QWidget(parent, Qt::Window)
    , portugues(QLocale::Portuguese, QLocale::Brazil)
{
    setWindowTitle("Pedido de Entrega");
    resize(880, 620);
    montarTela();
    carregarItens();
    atualizarTotal();
}

void PedidoEntregaJanela::montarTela()
{
    auto *raiz = new QVBoxLayout(this);

    // ---- cliente, endereço e forma de pagamento
    auto *form = new QFormLayout;

    leCliente = new QLineEdit;
    const QStringList clientes = clienteServ.listarClientesParaCompleter();
    auto *completer = new QCompleter(clientes, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    leCliente->setCompleter(completer);
    if (!clientes.isEmpty())
        leCliente->setText(clientes.first());
    leCliente->setPlaceholderText("Digite o nome do cliente");
    form->addRow("Cliente:", leCliente);

    leEndereco = new QLineEdit;
    leEndereco->setPlaceholderText("Endereço ou ponto de entrega (opcional)");
    form->addRow("Entrega em:", leEndereco);

    cbForma = new QComboBox;
    cbForma->addItems({"Dinheiro", "Pix", "Débito", "Crédito", "Prazo"});
    form->addRow("Pagamento:", cbForma);
    raiz->addLayout(form);

    // ---- itens
    tabela = new QTableWidget(0, TotalColunas);
    tabela->setHorizontalHeaderLabels({"Item", "Unid.", "Preço", "Quantidade", "Quero R$", "Total"});
    tabela->verticalHeader()->setVisible(false);
    tabela->setSelectionMode(QAbstractItemView::NoSelection);
    tabela->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tabela->horizontalHeader()->setSectionResizeMode(ColItem, QHeaderView::Stretch);
    for (int c : {ColUnidade, ColPreco, ColQuantidade, ColValor, ColTotal})
        tabela->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    raiz->addWidget(tabela, 1);

    // ---- observação e total
    leObservacao = new QLineEdit;
    leObservacao->setPlaceholderText("Observação do pedido (opcional)");
    raiz->addWidget(leObservacao);

    auto *rodape = new QHBoxLayout;
    // sem nota fiscal o cupom sai sempre; com nota fiscal ligada a caixa decide se sai o cupom também
    chkRecibo = new QCheckBox("Imprimir cupom também");
    chkRecibo->setVisible(Config_service().carregarTudo().emitNfFiscal);
    rodape->addWidget(chkRecibo);
    rodape->addStretch();
    lblTotal = new QLabel;
    QFont f = lblTotal->font();
    f.setPointSize(f.pointSize() + 8);
    f.setBold(true);
    lblTotal->setFont(f);
    rodape->addWidget(lblTotal);
    raiz->addLayout(rodape);

    auto *botoes = new QHBoxLayout;
    botoes->addStretch();
    auto *btnLimpar = new QPushButton("Limpar");
    btnLancar = new QPushButton("Lançar pedido");
    btnLancar->setDefault(true);
    btnLancar->setMinimumHeight(40);
    botoes->addWidget(btnLimpar);
    botoes->addWidget(btnLancar);
    raiz->addLayout(botoes);

    connect(btnLimpar, &QPushButton::clicked, this, &PedidoEntregaJanela::limpar);
    connect(btnLancar, &QPushButton::clicked, this, &PedidoEntregaJanela::lancarPedido);
}

void PedidoEntregaJanela::carregarItens()
{
    itens = entregaServ.carregarItens();
    tabela->setRowCount(itens.size());

    for (int row = 0; row < itens.size(); ++row) {
        const EntregaItemDTO &item = itens.at(row);

        tabela->setItem(row, ColItem, new QTableWidgetItem(item.descricao));
        tabela->setItem(row, ColUnidade, new QTableWidgetItem(item.unidade));
        tabela->setItem(row, ColPreco, new QTableWidgetItem);
        tabela->setItem(row, ColTotal, new QTableWidgetItem);

        auto *qtd = new QDoubleSpinBox;
        qtd->setDecimals(item.fracionado() ? 3 : 0);
        qtd->setRange(0, 99999);
        qtd->setGroupSeparatorShown(false);
        tabela->setCellWidget(row, ColQuantidade, qtd);

        auto *valor = new QDoubleSpinBox;
        valor->setDecimals(2);
        valor->setRange(0, 999999);
        valor->setPrefix("R$ ");
        tabela->setCellWidget(row, ColValor, valor);

        // digitou a quantidade: recalcula a linha
        connect(qtd, &QDoubleSpinBox::valueChanged, this, [this, row]() {
            if (atualizando) return;
            atualizarLinha(row);
            atualizarTotal();
        });
        // digitou o valor em reais: converte em quantidade
        connect(valor, &QDoubleSpinBox::valueChanged, this, [this, row](double v) {
            if (atualizando) return;
            atualizando = true;
            if (auto *q = qobject_cast<QDoubleSpinBox *>(tabela->cellWidget(row, ColQuantidade)))
                q->setValue(Entrega_service::quantidadePorValor(itens.at(row), v));
            atualizando = false;
            atualizarLinha(row);
            atualizarTotal();
        });

        atualizarLinha(row);
    }
}

double PedidoEntregaJanela::quantidadeDaLinha(int row) const
{
    if (auto *q = qobject_cast<QDoubleSpinBox *>(tabela->cellWidget(row, ColQuantidade)))
        return q->value();
    return 0;
}

void PedidoEntregaJanela::atualizarLinha(int row)
{
    const EntregaItemDTO &item = itens.at(row);
    const double qtd = quantidadeDaLinha(row);
    const double preco = Entrega_service::precoAplicado(item, qtd);

    QString txtPreco = portugues.toCurrencyString(preco, "R$ ");
    if (item.temFaixa() && qtd >= item.qtdFaixa)
        txtPreco += " (faixa " + QString::number(item.qtdFaixa, 'f', 0) + "+)";
    tabela->item(row, ColPreco)->setText(txtPreco);
    tabela->item(row, ColTotal)->setText(portugues.toCurrencyString(Entrega_service::totalLinha(item, qtd), "R$ "));
}

QList<EntregaLinhaDTO> PedidoEntregaJanela::linhasDoPedido() const
{
    QList<EntregaLinhaDTO> linhas;
    for (int row = 0; row < itens.size(); ++row) {
        const double qtd = quantidadeDaLinha(row);
        if (qtd > 0)
            linhas << EntregaLinhaDTO{itens.at(row), qtd};
    }
    return linhas;
}

void PedidoEntregaJanela::atualizarTotal()
{
    lblTotal->setText("Total: " + portugues.toCurrencyString(Entrega_service::totalPedido(linhasDoPedido()), "R$ "));
}

bool PedidoEntregaJanela::validarCliente(qlonglong *idCliente, QString *nome)
{
    auto r = clienteServ.validarClienteTexto(leCliente->text().trimmed());
    if (!r.ok) {
        QMessageBox::warning(this, "Cliente", r.msg);
        leCliente->setFocus();
        return false;
    }
    *idCliente = r.clienteId;
    *nome = r.nomeCorrigido;
    return true;
}

void PedidoEntregaJanela::limpar()
{
    atualizando = true;
    for (int row = 0; row < itens.size(); ++row) {
        if (auto *q = qobject_cast<QDoubleSpinBox *>(tabela->cellWidget(row, ColQuantidade)))
            q->setValue(0);
        if (auto *v = qobject_cast<QDoubleSpinBox *>(tabela->cellWidget(row, ColValor)))
            v->setValue(0);
    }
    atualizando = false;
    for (int row = 0; row < itens.size(); ++row)
        atualizarLinha(row);
    leEndereco->clear();
    leObservacao->clear();
    atualizarTotal();
}

void PedidoEntregaJanela::lancarPedido()
{
    EntregaPedidoDTO pedido;
    pedido.linhas = linhasDoPedido();
    if (pedido.linhas.isEmpty()) {
        QMessageBox::information(this, "Pedido de entrega", "Informe a quantidade de pelo menos um item.");
        return;
    }
    if (!validarCliente(&pedido.idCliente, &pedido.clienteNome))
        return;

    pedido.formaPagamento = cbForma->currentText();
    pedido.endereco = leEndereco->text().trimmed();
    pedido.observacao = leObservacao->text().trimmed();
    pedido.idOperadorSessao = Sessao_service::instancia()->idOperador();

    const double total = Entrega_service::totalPedido(pedido.linhas);

    // venda a prazo (fiado): cliente identificado, ativo e dentro do limite de crédito
    if (pedido.formaPagamento == "Prazo") {
        ContasReceber_service receber;
        auto prazo = receber.validarVendaPrazo(pedido.idCliente, total);
        if (!prazo.ok) {
            QMessageBox::warning(this, "Venda a prazo", prazo.msg);
            return;
        }
    }

    const auto resp = QMessageBox::question(
        this, "Confirmar pedido",
        QString("Lançar o pedido de %1 no valor de %2 (%3)?")
            .arg(pedido.clienteNome, portugues.toCurrencyString(total, "R$ "), pedido.formaPagamento),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (resp != QMessageBox::Yes)
        return;

    auto r = entregaServ.registrarPedido(pedido);
    if (!r.ok) {
        QMessageBox::warning(this, "Pedido de entrega", r.msg);
        return;
    }

    QString msg = QString("Pedido lançado (venda %1).").arg(r.idVenda);

    // igual à venda normal: sem nota fiscal na loja, o cupom sai sempre na térmica;
    // com nota fiscal ligada, emite a NFC-e (o cupom só sai se a caixa estiver marcada)
    const ConfigDTO cfg = Config_service().carregarTudo();
    if (cfg.emitNfFiscal) {
        ClienteDTO cli = clienteServ.getClienteByID(pedido.idCliente);
        NotaFiscal_service notaServ;
        FiscalEmitter_service fiscalServ;
        Vendas_service vendaServ;

        const qlonglong nnf = notaServ.getProximoNNF(cfg.tpAmbFiscal, ModeloNota::NFCe);
        QApplication::setOverrideCursor(Qt::WaitCursor);
        auto nf = fiscalServ.enviarNfcePadrao(r.venda, r.produtos, nnf, cli, true, false);
        QApplication::restoreOverrideCursor();

        if (!nf.ok && nf.erro == FiscalEmitterErro::NCMInvalido) {
            const auto continuar = QMessageBox::question(
                this, "Atenção", nf.msg + "\nDeseja continuar mesmo assim?",
                QMessageBox::Yes | QMessageBox::No);
            if (continuar == QMessageBox::Yes) {
                QApplication::setOverrideCursor(Qt::WaitCursor);
                nf = fiscalServ.enviarNfcePadrao(r.venda, r.produtos, nnf, cli, true, true);
                QApplication::restoreOverrideCursor();
            }
        }
        if (!nf.ok) {
            // sem a nota a venda não vale: desfaz, como a venda normal faz
            auto desfeita = vendaServ.deletarVendaRegraNegocio(r.idVenda, false, "Falha ao emitir a nota fiscal.");
            QMessageBox::warning(this, "Nota fiscal",
                                 nf.msg + (desfeita.ok ? "\n\nO pedido foi cancelado."
                                                       : "\n\nNão foi possível cancelar o pedido: " + desfeita.msg));
            emit pedidoLancado();
            return;
        }
        msg += "\n" + nf.msg;
        Vendas::imprimirAposVenda(r.idVenda, chkRecibo->isChecked());
    } else {
        // se a impressora falhar, a venda já está gravada: a própria função avisa o erro
        Vendas::imprimirAposVenda(r.idVenda, true);
    }

    if (r.avisoMarca)
        msg += "\n\nAtenção: a venda foi gravada, mas não ficou marcada como entrega.";
    QMessageBox::information(this, "Pedido de entrega", msg);

    emit pedidoLancado();
    limpar();
}
