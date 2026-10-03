#include "contaspagarjanela.h"
#include "contapagardialogs.h"
#include "services/empresa_service.h"
#include "util/icones.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QLocale ptBR() { return QLocale(QLocale::Portuguese, QLocale::Brazil); }
QString dinheiro(double v) { return ptBR().toCurrencyString(v, "R$ "); }

enum Coluna { Vencimento, Descricao, Fornecedor, Categoria, Parcela, Valor, Situacao, Pagamento, Empresa, kColunas };

QToolButton *novoCartao(const QString &cor)
{
    auto *b = new QToolButton;
    b->setCursor(Qt::PointingHandCursor);
    b->setMinimumHeight(70);
    b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    b->setToolButtonStyle(Qt::ToolButtonTextOnly);
    b->setStyleSheet(QStringLiteral(
        "QToolButton { background: white; color: %1; border: 2px solid %1; border-radius: 10px; font-weight: 700;"
        " padding: 6px 12px; text-align: left; }"
        "QToolButton:hover { background: #F2F7F4; }").arg(cor));
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

ContasPagarJanela::ContasPagarJanela(std::function<bool(const QString &)> gate, const QString &statusInicial,
                                     QWidget *parent)
    : QDialog(parent)
    , exigirGerente(std::move(gate))
{
    setWindowTitle("Contas a pagar");
    setModal(true);
    resize(1180, 680);
    montar();
    if (!statusInicial.isEmpty())
        definirStatus(statusInicial);
    recarregar();
}

void ContasPagarJanela::definirStatus(const QString &status)
{
    const int idx = cbStatus->findData(status);
    if (idx >= 0)
        cbStatus->setCurrentIndex(idx);
}

void ContasPagarJanela::montar()
{
    auto *titulo = new QLabel("Contas a pagar");
    titulo->setStyleSheet("font-size: 18pt; font-weight: 700; color: #142D45;");

    cartaoVencidas = novoCartao("#B91C1C");
    cartaoHoje = novoCartao("#B45309");
    cartaoSemana = novoCartao("#107A2E");
    cartaoAberto = novoCartao("#475569");
    connect(cartaoVencidas, &QToolButton::clicked, this, [this]() { definirStatus("VENCIDA"); });
    connect(cartaoHoje, &QToolButton::clicked, this, [this]() {
        definirStatus(kContaAberta);
        chPeriodo->setChecked(true);
        dtDe->setDate(QDate::currentDate());
        dtAte->setDate(QDate::currentDate());
    });
    connect(cartaoSemana, &QToolButton::clicked, this, [this]() {
        definirStatus(kContaAberta);
        chPeriodo->setChecked(true);
        dtDe->setDate(QDate::currentDate());
        dtAte->setDate(QDate::currentDate().addDays(7));
    });
    connect(cartaoAberto, &QToolButton::clicked, this, [this]() {
        definirStatus(kContaAberta);
        chPeriodo->setChecked(false);
    });

    auto *cartoes = new QHBoxLayout;
    cartoes->setSpacing(10);
    cartoes->addWidget(cartaoVencidas);
    cartoes->addWidget(cartaoHoje);
    cartoes->addWidget(cartaoSemana);
    cartoes->addWidget(cartaoAberto);

    // filtros
    cbEmpresa = new QComboBox;
    cbEmpresa->addItem("Todas as empresas", 0);
    for (const EmpresaDTO &e : Empresa_service::instancia()->listar(false))
        cbEmpresa->addItem(e.apelido, e.id);
    const int idxAtiva = cbEmpresa->findData(Empresa_service::instancia()->idAtiva());
    if (idxAtiva >= 0)
        cbEmpresa->setCurrentIndex(idxAtiva);

    cbStatus = new QComboBox;
    cbStatus->addItem("Em aberto", QString(kContaAberta));
    cbStatus->addItem("Vencidas", QStringLiteral("VENCIDA"));
    cbStatus->addItem("Pagas", QString(kContaPaga));
    cbStatus->addItem("Canceladas", QString(kContaCancelada));
    cbStatus->addItem("Todas", QString());

    edBusca = new QLineEdit;
    edBusca->setPlaceholderText("Buscar por descrição, fornecedor ou documento");
    edBusca->setClearButtonEnabled(true);

    chPeriodo = new QCheckBox("Vencimento de");
    dtDe = new QDateEdit(QDate::currentDate().addDays(-30));
    dtAte = new QDateEdit(QDate::currentDate().addDays(30));
    for (QDateEdit *d : {dtDe, dtAte}) {
        d->setCalendarPopup(true);
        d->setDisplayFormat("dd/MM/yyyy");
        d->setLocale(ptBR());
        d->setEnabled(false);
    }
    auto *lblAte = new QLabel("até");

    auto *filtros = new QHBoxLayout;
    filtros->setSpacing(8);
    filtros->addWidget(cbEmpresa);
    filtros->addWidget(cbStatus);
    filtros->addWidget(edBusca, 1);
    filtros->addWidget(chPeriodo);
    filtros->addWidget(dtDe);
    filtros->addWidget(lblAte);
    filtros->addWidget(dtAte);

    connect(cbEmpresa, &QComboBox::currentIndexChanged, this, [this]() { recarregar(); });
    connect(cbStatus, &QComboBox::currentIndexChanged, this, [this]() { recarregar(); });
    connect(edBusca, &QLineEdit::textChanged, this, [this]() { recarregar(); });
    connect(chPeriodo, &QCheckBox::toggled, this, [this](bool on) {
        dtDe->setEnabled(on);
        dtAte->setEnabled(on);
        recarregar();
    });
    connect(dtDe, &QDateEdit::dateChanged, this, [this]() { recarregar(); });
    connect(dtAte, &QDateEdit::dateChanged, this, [this]() { recarregar(); });

    // tabela
    tabela = new QTableWidget(0, kColunas);
    tabela->setHorizontalHeaderLabels({"Vencimento", "Descrição", "Fornecedor", "Categoria", "Parcela", "Valor",
                                       "Situação", "Pagamento", "Empresa"});
    tabela->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tabela->setSelectionBehavior(QAbstractItemView::SelectRows);
    tabela->setSelectionMode(QAbstractItemView::SingleSelection);
    tabela->setAlternatingRowColors(true);
    tabela->verticalHeader()->setVisible(false);
    tabela->horizontalHeader()->setSectionResizeMode(Descricao, QHeaderView::Stretch);
    tabela->horizontalHeader()->setSectionResizeMode(Fornecedor, QHeaderView::ResizeToContents);
    tabela->horizontalHeader()->setSectionResizeMode(Categoria, QHeaderView::ResizeToContents);
    tabela->horizontalHeader()->setSectionResizeMode(Situacao, QHeaderView::ResizeToContents);
    tabela->horizontalHeader()->setSectionResizeMode(Pagamento, QHeaderView::ResizeToContents);
    tabela->horizontalHeader()->setSectionResizeMode(Empresa, QHeaderView::ResizeToContents);
    tabela->horizontalHeader()->setSectionResizeMode(Vencimento, QHeaderView::ResizeToContents);
    tabela->horizontalHeader()->setSectionResizeMode(Parcela, QHeaderView::ResizeToContents);
    tabela->horizontalHeader()->setSectionResizeMode(Valor, QHeaderView::ResizeToContents);
    connect(tabela, &QTableWidget::itemSelectionChanged, this, &ContasPagarJanela::atualizarBotoes);
    connect(tabela, &QTableWidget::cellDoubleClicked, this, [this]() {
        const ContaPagarDTO c = selecionada();
        if (c.aberta())
            baixar();
    });

    lblTotais = new QLabel;
    lblTotais->setStyleSheet("color: #142D45; font-weight: 700;");

    // ações
    auto *btnNova = new QPushButton("Nova conta");
    btnNova->setIcon(Icones::icone("plus", QColor(Qt::white), 18));
    btnBaixar = new QPushButton("Baixar (pagar)");
    btnBaixar->setIcon(Icones::icone("circle-check", QColor(Qt::white), 18));
    btnEditar = botaoSecundario("Editar", "pencil", "#142D45");
    btnEstornar = botaoSecundario("Estornar pagamento", "undo-2", "#B45309");
    btnCancelar = botaoSecundario("Cancelar conta", "ban", "#B91C1C");
    auto *btnFechar = botaoSecundario("Fechar", "x", "#142D45");

    connect(btnNova, &QPushButton::clicked, this, &ContasPagarJanela::novaConta);
    connect(btnBaixar, &QPushButton::clicked, this, &ContasPagarJanela::baixar);
    connect(btnEditar, &QPushButton::clicked, this, &ContasPagarJanela::editar);
    connect(btnEstornar, &QPushButton::clicked, this, &ContasPagarJanela::estornar);
    connect(btnCancelar, &QPushButton::clicked, this, &ContasPagarJanela::cancelar);
    connect(btnFechar, &QPushButton::clicked, this, &QDialog::accept);

    auto *acoes = new QHBoxLayout;
    acoes->addWidget(btnNova);
    acoes->addWidget(btnBaixar);
    acoes->addWidget(btnEditar);
    acoes->addSpacing(12);
    acoes->addWidget(btnEstornar);
    acoes->addWidget(btnCancelar);
    acoes->addStretch(1);
    acoes->addWidget(lblTotais);
    acoes->addSpacing(12);
    acoes->addWidget(btnFechar);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(10);
    layout->addWidget(titulo);
    layout->addLayout(cartoes);
    layout->addLayout(filtros);
    layout->addWidget(tabela, 1);
    layout->addLayout(acoes);
}

ContaPagarDTO ContasPagarJanela::selecionada() const
{
    const int linha = tabela->currentRow();
    if (linha < 0 || linha >= contas.size())
        return ContaPagarDTO();
    return contas.at(linha);
}

void ContasPagarJanela::atualizarResumo()
{
    const qlonglong emp = cbEmpresa->currentData().toLongLong();
    const ResumoContasPagarDTO r = servico.resumo(emp);
    auto texto = [](const QString &titulo, int qtd, double valor) {
        return QStringLiteral("%1\n%2   (%3)").arg(titulo, dinheiro(valor),
                                                     qtd == 1 ? QStringLiteral("1 conta") : QStringLiteral("%1 contas").arg(qtd));
    };
    cartaoVencidas->setText(texto("VENCIDAS", r.qtdVencidas, r.valorVencidas));
    cartaoHoje->setText(texto("VENCEM HOJE", r.qtdHoje, r.valorHoje));
    cartaoSemana->setText(texto("PRÓXIMOS 7 DIAS", r.qtdProximos7Dias, r.valorProximos7Dias));
    cartaoAberto->setText(texto("TOTAL EM ABERTO", r.qtdAbertas, r.valorAbertas));
}

void ContasPagarJanela::recarregar()
{
    FiltroContasPagarDTO f;
    f.idEmpresa = cbEmpresa->currentData().toLongLong();
    f.status = cbStatus->currentData().toString();
    f.texto = edBusca->text();
    if (chPeriodo->isChecked()) {
        f.vencimentoDe = dtDe->date().toString(Qt::ISODate);
        f.vencimentoAte = dtAte->date().toString(Qt::ISODate);
    }
    contas = servico.listar(f);

    const qlonglong selecionadaId = selecionada().id;
    tabela->setRowCount(contas.size());
    const QDate hoje = QDate::currentDate();
    double totalLista = 0;
    int selecionarLinha = -1;

    auto celula = [&](int linha, int coluna, const QString &texto, const QColor &cor, bool negrito = false,
                      Qt::Alignment alinh = Qt::AlignLeft | Qt::AlignVCenter, bool riscado = false) {
        auto *it = new QTableWidgetItem(texto);
        it->setForeground(cor);
        it->setTextAlignment(alinh);
        QFont fonte = it->font();
        fonte.setBold(negrito);
        fonte.setStrikeOut(riscado);
        it->setFont(fonte);
        tabela->setItem(linha, coluna, it);
    };

    for (int i = 0; i < contas.size(); ++i) {
        const ContaPagarDTO &c = contas.at(i);
        if (c.id == selecionadaId)
            selecionarLinha = i;
        const QDate venc = c.dataVencimento();
        QString situacao;
        QColor cor("#1F2937");
        if (c.status == kContaPaga) {
            situacao = "Paga";
            cor = QColor("#15803D");
        } else if (c.status == kContaCancelada) {
            situacao = "Cancelada";
            cor = QColor("#94A3B8");
        } else {
            const qint64 dias = venc.isValid() ? hoje.daysTo(venc) : 0;
            if (dias < 0) {
                situacao = QStringLiteral("Vencida há %1 dia(s)").arg(-dias);
                cor = QColor("#B91C1C");
            } else if (dias == 0) {
                situacao = "Vence hoje";
                cor = QColor("#B45309");
            } else {
                situacao = QStringLiteral("Vence em %1 dia(s)").arg(dias);
                cor = dias <= 7 ? QColor("#107A2E") : QColor("#475569");
            }
            totalLista += c.valor;
        }
        const bool cancelada = c.status == kContaCancelada;
        celula(i, Vencimento, venc.toString("dd/MM/yyyy"), cor, c.vencida(hoje), Qt::AlignCenter, cancelada);
        celula(i, Descricao, c.descricao, cor, false, Qt::AlignLeft | Qt::AlignVCenter, cancelada);
        celula(i, Fornecedor, c.fornecedor, cor, false, Qt::AlignLeft | Qt::AlignVCenter, cancelada);
        celula(i, Categoria, c.categoria, cor, false, Qt::AlignLeft | Qt::AlignVCenter, cancelada);
        celula(i, Parcela, c.totalParcelas > 1 ? QStringLiteral("%1/%2").arg(c.parcela).arg(c.totalParcelas)
                                                : QStringLiteral("—"), cor, false, Qt::AlignCenter, cancelada);
        celula(i, Valor, dinheiro(c.valor), cor, false, Qt::AlignRight | Qt::AlignVCenter, cancelada);
        celula(i, Situacao, situacao, cor, true);
        QString pagamento = QStringLiteral("—");
        if (c.status == kContaPaga) {
            pagamento = QStringLiteral("%1 · %2 · %3")
                            .arg(QDateTime::fromString(c.pagoEm, "yyyy-MM-dd HH:mm:ss").toString("dd/MM/yy"),
                                 dinheiro(c.valorPago), c.formaPagamento);
        } else if (cancelada && !c.motivoCancelamento.isEmpty()) {
            pagamento = c.motivoCancelamento;
        }
        celula(i, Pagamento, pagamento, cor, false, Qt::AlignLeft | Qt::AlignVCenter, false);
        const EmpresaDTO e = Empresa_service::instancia()->getPorId(c.idEmpresa);
        celula(i, Empresa, e.valida() ? e.apelido : QStringLiteral("—"), cor, false, Qt::AlignLeft | Qt::AlignVCenter,
               cancelada);
    }
    if (selecionarLinha >= 0)
        tabela->selectRow(selecionarLinha);

    lblTotais->setText(QStringLiteral("%1 conta(s) na lista · em aberto: %2").arg(contas.size()).arg(dinheiro(totalLista)));
    atualizarResumo();
    atualizarBotoes();
}

void ContasPagarJanela::atualizarBotoes()
{
    const ContaPagarDTO c = selecionada();
    btnBaixar->setEnabled(c.id > 0 && c.aberta());
    btnEditar->setEnabled(c.id > 0 && c.status != kContaCancelada);
    btnCancelar->setEnabled(c.id > 0 && c.aberta());
    btnEstornar->setEnabled(c.id > 0 && c.status == kContaPaga);
}

void ContasPagarJanela::novaConta()
{
    NovaContaDialog dlg(ContaPagarDTO(), this);
    while (dlg.exec() == QDialog::Accepted) {
        const auto r = servico.lancar(dlg.conta(), dlg.parcelas(), dlg.periodicidade());
        if (r.ok) {
            recarregar();
            return;
        }
        QMessageBox::warning(this, "Nova conta", r.msg);
    }
}

void ContasPagarJanela::editar()
{
    const ContaPagarDTO c = selecionada();
    if (c.id <= 0 || c.status == kContaCancelada)
        return;
    NovaContaDialog dlg(c, this);
    while (dlg.exec() == QDialog::Accepted) {
        const auto r = servico.alterar(dlg.conta());
        if (r.ok) {
            recarregar();
            return;
        }
        QMessageBox::warning(this, "Editar conta", r.msg);
    }
}

void ContasPagarJanela::baixar()
{
    const ContaPagarDTO c = selecionada();
    if (c.id <= 0 || !c.aberta())
        return;
    BaixaContaDialog dlg(c, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const auto r = servico.baixar(c.id, dlg.valorPago(), dlg.forma(), dlg.quando());
    if (!r.ok)
        QMessageBox::warning(this, "Baixar conta", r.msg);
    recarregar();
}

void ContasPagarJanela::estornar()
{
    const ContaPagarDTO c = selecionada();
    if (c.id <= 0 || c.status != kContaPaga)
        return;
    if (exigirGerente && !exigirGerente(QStringLiteral("Estornar pagamento")))
        return;
    const auto resp = QMessageBox::question(this, "Estornar pagamento",
        QStringLiteral("Estornar o pagamento de \"%1\" (%2)?\nA conta volta para em aberto.")
            .arg(c.descricao, dinheiro(c.valorPago)),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (resp != QMessageBox::Yes)
        return;
    const auto r = servico.estornarBaixa(c.id);
    if (!r.ok)
        QMessageBox::warning(this, "Estornar pagamento", r.msg);
    recarregar();
}

void ContasPagarJanela::cancelar()
{
    const ContaPagarDTO c = selecionada();
    if (c.id <= 0 || !c.aberta())
        return;
    if (exigirGerente && !exigirGerente(QStringLiteral("Cancelar conta a pagar")))
        return;
    bool ok = false;
    const QString motivo = QInputDialog::getText(this, "Cancelar conta",
        QStringLiteral("Motivo do cancelamento de \"%1\" (%2):").arg(c.descricao, dinheiro(c.valor)),
        QLineEdit::Normal, QString(), &ok);
    if (!ok)
        return;
    const auto r = servico.cancelar(c.id, motivo);
    if (!r.ok)
        QMessageBox::warning(this, "Cancelar conta", r.msg);
    recarregar();
}
