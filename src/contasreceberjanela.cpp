#include "contasreceberjanela.h"
#include "extratoclientedialog.h"
#include "services/empresa_service.h"
#include "util/icones.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QString dinheiro(double v) { return QLocale(QLocale::Portuguese, QLocale::Brazil).toCurrencyString(v, "R$ "); }

enum Coluna { Cliente, Telefone, TotalDevido, UltimaCompra, Limite, Situacao, kColunas };

QToolButton *novoCartao(const QString &cor)
{
    auto *b = new QToolButton;
    b->setEnabled(true);
    b->setMinimumHeight(70);
    b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    b->setToolButtonStyle(Qt::ToolButtonTextOnly);
    b->setStyleSheet(QStringLiteral(
        "QToolButton { background: white; color: %1; border: 2px solid %1; border-radius: 10px; font-weight: 700;"
        " padding: 6px 12px; text-align: left; }").arg(cor));
    return b;
}

QPushButton *botaoSecundario(const QString &texto, const QString &icone, const QString &cor)
{
    auto *b = new QPushButton(texto);
    b->setIcon(Icones::icone(icone, QColor(cor), 18));
    b->setStyleSheet(QStringLiteral(
        "QPushButton { background: white; color: %1; border: 1px solid #C9D3DF; }"
        "QPushButton:hover { background: #EAF6EC; border-color: #17963A; }"
        "QPushButton:disabled { background: #F1F5F9; color: #94A3B8; border-color: #E2E8F0; }").arg(cor));
    return b;
}
}

ContasReceberJanela::ContasReceberJanela(GateGerente g, qlonglong abrirClienteId, QWidget *parent)
    : QDialog(parent)
    , gate(std::move(g))
{
    setWindowTitle("Contas a receber");
    setModal(true);
    resize(1120, 660);
    montar();
    recarregar();
    if (abrirClienteId > 0) {
        // vem da tela de Clientes: abre direto o extrato daquele cliente
        QTimer::singleShot(0, this, [this, abrirClienteId]() {
            ExtratoClienteDialog dlg(abrirClienteId, gate, this);
            dlg.exec();
            recarregar();
        });
    }
}

void ContasReceberJanela::montar()
{
    auto *titulo = new QLabel("Contas a receber");
    titulo->setStyleSheet("font-size: 18pt; font-weight: 700; color: #142D45;");
    auto *sub = new QLabel("Fiado e caderneta: o que cada cliente deve, o que comprou a prazo e o que já pagou.");
    sub->setStyleSheet("color: #5B6B7F;");

    cartaoTotal = novoCartao("#B91C1C");
    cartaoClientes = novoCartao("#107A2E");
    cartaoMaior = novoCartao("#475569");
    auto *cartoes = new QHBoxLayout;
    cartoes->setSpacing(10);
    cartoes->addWidget(cartaoTotal);
    cartoes->addWidget(cartaoClientes);
    cartoes->addWidget(cartaoMaior);

    cbEmpresa = new QComboBox;
    cbEmpresa->addItem("Todas as empresas", 0);
    for (const EmpresaDTO &e : Empresa_service::instancia()->listar(false))
        cbEmpresa->addItem(e.apelido, e.id);
    cbEmpresa->setCurrentIndex(0);
    cbModo = new QComboBox;
    cbModo->addItem("Com dívida em aberto", int(ModoDevedores::ComDivida));
    cbModo->addItem("Todos os clientes", int(ModoDevedores::Todos));
    cbModo->addItem("Inativos", int(ModoDevedores::Inativos));
    edBusca = new QLineEdit;
    edBusca->setPlaceholderText("Buscar por nome ou telefone");
    edBusca->setClearButtonEnabled(true);
    auto *filtros = new QHBoxLayout;
    filtros->setSpacing(8);
    filtros->addWidget(cbEmpresa);
    filtros->addWidget(cbModo);
    filtros->addWidget(edBusca, 1);
    connect(cbEmpresa, &QComboBox::currentIndexChanged, this, [this]() { recarregar(); });
    connect(cbModo, &QComboBox::currentIndexChanged, this, [this]() { recarregar(); });
    connect(edBusca, &QLineEdit::textChanged, this, [this]() { recarregar(); });

    tabela = new QTableWidget(0, kColunas);
    tabela->setHorizontalHeaderLabels({"Cliente", "Telefone", "Total devido", "Última compra", "Limite", "Situação"});
    tabela->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tabela->setSelectionBehavior(QAbstractItemView::SelectRows);
    tabela->setSelectionMode(QAbstractItemView::SingleSelection);
    tabela->setAlternatingRowColors(true);
    tabela->verticalHeader()->setVisible(false);
    tabela->horizontalHeader()->setSectionResizeMode(Cliente, QHeaderView::Stretch);
    for (int c : {Telefone, TotalDevido, UltimaCompra, Limite, Situacao})
        tabela->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    connect(tabela, &QTableWidget::itemSelectionChanged, this, &ContasReceberJanela::atualizarBotoes);
    connect(tabela, &QTableWidget::cellDoubleClicked, this, [this]() { abrirExtrato(); });

    auto *btnLancar = new QPushButton("Lançar dívida");
    btnLancar->setIcon(Icones::icone("plus", QColor(Qt::white), 18));
    btnReceber = new QPushButton("Receber");
    btnReceber->setIcon(Icones::icone("circle-check", QColor(Qt::white), 18));
    btnExtrato = botaoSecundario("Extrato", "file-text", "#142D45");
    btnCobrar = botaoSecundario("Cobrar no WhatsApp", "mail", "#15803D");
    btnCredito = botaoSecundario("Dados de crédito", "settings", "#142D45");
    auto *btnFechar = botaoSecundario("Fechar", "x", "#142D45");

    connect(btnLancar, &QPushButton::clicked, this, [this]() { novaDivida(); });
    connect(btnReceber, &QPushButton::clicked, this, [this]() {
        const DevedorDTO d = selecionado();
        if (d.idCliente > 0 && receberComTela(this, d.idCliente))
            recarregar();
    });
    connect(btnExtrato, &QPushButton::clicked, this, &ContasReceberJanela::abrirExtrato);
    connect(btnCobrar, &QPushButton::clicked, this, [this]() {
        const DevedorDTO d = selecionado();
        if (d.idCliente > 0)
            cobrarPorWhatsApp(this, d.idCliente);
    });
    connect(btnCredito, &QPushButton::clicked, this, [this]() {
        const DevedorDTO d = selecionado();
        if (d.idCliente > 0) {
            editarCreditoComTela(this, gate, d.idCliente);
            recarregar();
        }
    });
    connect(btnFechar, &QPushButton::clicked, this, &QDialog::accept);

    auto *acoes = new QHBoxLayout;
    acoes->setSpacing(8);
    acoes->addWidget(btnLancar);
    acoes->addWidget(btnReceber);
    acoes->addWidget(btnExtrato);
    acoes->addWidget(btnCobrar);
    acoes->addWidget(btnCredito);
    acoes->addStretch(1);
    acoes->addWidget(btnFechar);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(10);
    layout->addWidget(titulo);
    layout->addWidget(sub);
    layout->addLayout(cartoes);
    layout->addLayout(filtros);
    layout->addWidget(tabela, 1);
    layout->addLayout(acoes);
}

DevedorDTO ContasReceberJanela::selecionado() const
{
    const int linha = tabela->currentRow();
    if (linha < 0 || linha >= devedores.size())
        return DevedorDTO();
    return devedores.at(linha);
}

void ContasReceberJanela::novaDivida()
{
    const DevedorDTO d = selecionado();
    if (lancarDividaComTela(this, gate, d.idCliente))
        recarregar();
}

void ContasReceberJanela::abrirExtrato()
{
    const DevedorDTO d = selecionado();
    if (d.idCliente <= 0)
        return;
    ExtratoClienteDialog dlg(d.idCliente, gate, this);
    dlg.exec();
    recarregar();
}

void ContasReceberJanela::recarregar()
{
    FiltroDevedoresDTO f;
    f.idEmpresa = cbEmpresa->currentData().toLongLong();
    f.modo = ModoDevedores(cbModo->currentData().toInt());
    f.texto = edBusca->text();

    const qlonglong anterior = selecionado().idCliente;
    devedores = servico.listarDevedores(f);

    tabela->setRowCount(devedores.size());
    int selecionar = -1;
    for (int i = 0; i < devedores.size(); ++i) {
        const DevedorDTO &d = devedores.at(i);
        if (d.idCliente == anterior)
            selecionar = i;
        QColor cor("#1F2937");
        QString situacao = d.ativo ? QStringLiteral("Em dia") : QStringLiteral("Inativo");
        if (d.totalDevido > 0.004) {
            situacao = d.ativo ? QStringLiteral("Deve") : QStringLiteral("Inativo, com dívida");
            cor = QColor("#B91C1C");
        }
        if (d.acimaDoLimite()) {
            situacao = QStringLiteral("Acima do limite");
            cor = QColor("#B91C1C");
        } else if (d.totalDevido <= 0.004) {
            cor = d.ativo ? QColor("#15803D") : QColor("#94A3B8");
        }
        auto celula = [&](int col, const QString &texto, Qt::Alignment a = Qt::AlignLeft | Qt::AlignVCenter, bool negrito = false) {
            auto *it = new QTableWidgetItem(texto);
            it->setForeground(cor);
            it->setTextAlignment(a);
            QFont fonte = it->font();
            fonte.setBold(negrito);
            it->setFont(fonte);
            tabela->setItem(i, col, it);
        };
        celula(Cliente, d.nome);
        celula(Telefone, d.whatsapp.isEmpty() ? d.telefone : d.whatsapp);
        celula(TotalDevido, dinheiro(d.totalDevido), Qt::AlignRight | Qt::AlignVCenter, true);
        const QDate ult = QDate::fromString(d.ultimaCompra, Qt::ISODate);
        celula(UltimaCompra, ult.isValid() ? ult.toString("dd/MM/yyyy") : QStringLiteral("—"), Qt::AlignCenter);
        celula(Limite, d.temLimite ? dinheiro(d.limite) : QStringLiteral("sem limite"), Qt::AlignRight | Qt::AlignVCenter);
        celula(Situacao, situacao, Qt::AlignLeft | Qt::AlignVCenter, true);
    }
    if (selecionar >= 0)
        tabela->selectRow(selecionar);

    const ResumoReceberDTO r = servico.resumo(f.idEmpresa);
    cartaoTotal->setText(QStringLiteral("TOTAL A RECEBER\n%1").arg(dinheiro(r.totalReceber)));
    cartaoClientes->setText(QStringLiteral("CLIENTES DEVENDO\n%1").arg(r.clientesDevendo));
    cartaoMaior->setText(r.clientesDevendo > 0
        ? QStringLiteral("MAIOR DÍVIDA\n%1 · %2").arg(dinheiro(r.maiorDivida), r.maiorDevedor)
        : QStringLiteral("MAIOR DÍVIDA\n—"));
    atualizarBotoes();
}

void ContasReceberJanela::atualizarBotoes()
{
    const DevedorDTO d = selecionado();
    const bool tem = d.idCliente > 0;
    btnExtrato->setEnabled(tem);
    btnReceber->setEnabled(tem && d.totalDevido > 0.004);
    btnCobrar->setEnabled(tem && d.totalDevido > 0.004);
    btnCredito->setEnabled(tem);
}
