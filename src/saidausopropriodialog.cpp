#include "saidausopropriodialog.h"
#include "services/Produto_service.h"
#include "services/saidausoproprio_service.h"
#include "services/sessao_service.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
const QLocale ptBR(QLocale::Portuguese, QLocale::Brazil);
QString dinheiro(double v) { return ptBR.toCurrencyString(v, "R$ "); }
QString numero(double v) { return ptBR.toString(v, 'g', 10); }

const char *kEstiloSecundario = "QPushButton { background: white; color: #142D45; border: 1px solid #C9D3DF; }"
                                "QPushButton:hover { background: #EAF6EC; }";
}

SaidaUsoProprioDialog::SaidaUsoProprioDialog(qlonglong idProduto, QWidget *parent)
    : QDialog(parent)
    , idProduto_(idProduto)
{
    setWindowTitle("Saída para uso próprio");
    setModal(true);
    setMinimumWidth(520);

    Produto_Service produtos;
    const ProdutoDTO p = produtos.getProduto(idProduto);
    estoque_ = p.quantidade;
    custo_ = p.precoFornecedor;
    semControle_ = produtos.semControleDeEstoque(idProduto);

    auto *titulo = new QLabel("Saída para uso próprio");
    titulo->setStyleSheet("font-size: 16pt; font-weight: 700; color: #142D45;");
    auto *nome = new QLabel(QStringLiteral("<b>%1</b>").arg(p.descricao.toHtmlEscaped()));
    nome->setTextFormat(Qt::RichText);
    nome->setWordWrap(true);
    auto *info = new QLabel(semControle_
        ? QStringLiteral("Produto sem controle de estoque: a saída fica registrada, mas a quantidade não muda.")
        : QStringLiteral("Em estoque: <b>%1</b> %2").arg(numero(estoque_), p.uCom.toHtmlEscaped()));
    info->setTextFormat(Qt::RichText);
    info->setWordWrap(true);
    info->setStyleSheet("color: #475569;");

    quantidade_ = new QDoubleSpinBox;
    quantidade_->setDecimals(3);
    quantidade_->setMinimum(0.0);
    quantidade_->setMaximum(semControle_ ? 1000000.0 : qMax(0.0, estoque_));
    quantidade_->setSingleStep(1.0);
    quantidade_->setValue(0.0);
    quantidade_->setLocale(ptBR);
    connect(quantidade_, &QDoubleSpinBox::valueChanged, this, &SaidaUsoProprioDialog::atualizarCusto);

    motivo_ = new QComboBox;
    motivo_->addItems(motivosSaidaUsoProprio());

    observacao_ = new QLineEdit;
    observacao_->setPlaceholderText("Opcional (ex.: lanche da equipe, caixa caída no depósito)");
    observacao_->setMaxLength(200);

    lblCusto_ = new QLabel;
    lblCusto_->setStyleSheet("color: #475569;");

    auto *form = new QFormLayout;
    form->addRow("Quantidade", quantidade_);
    form->addRow("Motivo", motivo_);
    form->addRow("Observação", observacao_);
    form->addRow("Custo da saída", lblCusto_);

    auto *btnOk = new QPushButton("Registrar saída");
    btnOk->setDefault(true);
    auto *btnCancelar = new QPushButton("Cancelar");
    btnCancelar->setStyleSheet(kEstiloSecundario);
    connect(btnOk, &QPushButton::clicked, this, &SaidaUsoProprioDialog::confirmar);
    connect(btnCancelar, &QPushButton::clicked, this, &QDialog::reject);

    auto *botoes = new QHBoxLayout;
    botoes->addStretch(1);
    botoes->addWidget(btnOk);
    botoes->addWidget(btnCancelar);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(10);
    layout->addWidget(titulo);
    layout->addWidget(nome);
    layout->addWidget(info);
    layout->addLayout(form);
    layout->addLayout(botoes);

    atualizarCusto();
    quantidade_->setFocus();
    quantidade_->selectAll();
}

void SaidaUsoProprioDialog::atualizarCusto()
{
    if (custo_ <= 0) {
        lblCusto_->setText("sem preço de fornecedor cadastrado: o custo será gravado como R$ 0,00");
        return;
    }
    lblCusto_->setText(QStringLiteral("%1  (%2 x %3)")
                           .arg(dinheiro(quantidade_->value() * custo_), numero(quantidade_->value()), dinheiro(custo_)));
}

void SaidaUsoProprioDialog::confirmar()
{
    Sessao_service *sessao = Sessao_service::instancia();
    SaidaUsoProprio_service servico;
    const auto r = servico.registrar(idProduto_, quantidade_->value(), motivo_->currentText(), observacao_->text(),
                                     sessao->idOperador(), sessao->nomeOperador());
    if (!r.ok) {
        QMessageBox::warning(this, "Saída para uso próprio", r.msg);
        return;
    }
    registrou_ = true;
    accept();
}

HistoricoSaidasDialog::HistoricoSaidasDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Saídas para uso próprio");
    setModal(true);
    resize(960, 560);

    auto *titulo = new QLabel("Saídas para uso próprio");
    titulo->setStyleSheet("font-size: 16pt; font-weight: 700; color: #142D45;");

    const QDate hoje = QDate::currentDate();
    de_ = new QDateEdit(QDate(hoje.year(), hoje.month(), 1));
    ate_ = new QDateEdit(hoje);
    for (QDateEdit *d : {de_, ate_}) {
        d->setCalendarPopup(true);
        d->setDisplayFormat("dd/MM/yyyy");
    }
    motivo_ = new QComboBox;
    motivo_->addItem("Todos os motivos", QString());
    for (const QString &m : motivosSaidaUsoProprio())
        motivo_->addItem(m, m);

    auto *filtros = new QHBoxLayout;
    filtros->addWidget(new QLabel("De"));
    filtros->addWidget(de_);
    filtros->addWidget(new QLabel("até"));
    filtros->addWidget(ate_);
    filtros->addWidget(motivo_);
    filtros->addStretch(1);

    tabela_ = new QTableWidget(0, 7);
    tabela_->setHorizontalHeaderLabels({"Data", "Produto", "Qtd.", "Motivo", "Custo", "Operador", "Observação"});
    tabela_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tabela_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tabela_->setAlternatingRowColors(true);
    tabela_->verticalHeader()->setVisible(false);
    tabela_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    tabela_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
    for (int c : {0, 2, 3, 4, 5})
        tabela_->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);

    lblTotal_ = new QLabel;
    lblTotal_->setStyleSheet("font-weight: 700; color: #142D45;");
    auto *btnFechar = new QPushButton("Fechar");
    btnFechar->setStyleSheet(kEstiloSecundario);
    connect(btnFechar, &QPushButton::clicked, this, &QDialog::accept);

    auto *rodape = new QHBoxLayout;
    rodape->addWidget(lblTotal_, 1);
    rodape->addWidget(btnFechar);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(10);
    layout->addWidget(titulo);
    layout->addLayout(filtros);
    layout->addWidget(tabela_, 1);
    layout->addLayout(rodape);

    connect(de_, &QDateEdit::dateChanged, this, &HistoricoSaidasDialog::carregar);
    connect(ate_, &QDateEdit::dateChanged, this, &HistoricoSaidasDialog::carregar);
    connect(motivo_, &QComboBox::currentIndexChanged, this, &HistoricoSaidasDialog::carregar);
    carregar();
}

void HistoricoSaidasDialog::carregar()
{
    SaidaUsoProprio_service servico;
    const QList<SaidaUsoProprioDTO> saidas = servico.listar(de_->date(), ate_->date(), motivo_->currentData().toString());
    tabela_->setRowCount(saidas.size());
    for (int i = 0; i < saidas.size(); ++i) {
        const SaidaUsoProprioDTO &s = saidas.at(i);
        auto celula = [&](int col, const QString &texto, Qt::Alignment a = Qt::AlignLeft | Qt::AlignVCenter) {
            auto *it = new QTableWidgetItem(texto);
            it->setTextAlignment(a);
            tabela_->setItem(i, col, it);
        };
        const QDateTime d = QDateTime::fromString(s.dataHora, "yyyy-MM-dd HH:mm:ss");
        celula(0, d.isValid() ? d.toString("dd/MM/yyyy HH:mm") : s.dataHora);
        celula(1, s.descricaoProduto);
        celula(2, numero(s.quantidade), Qt::AlignCenter);
        celula(3, s.motivo);
        celula(4, dinheiro(s.custoTotal()), Qt::AlignRight | Qt::AlignVCenter);
        celula(5, s.nomeOperador);
        celula(6, s.observacao);
    }
    lblTotal_->setText(QStringLiteral("%1 saída(s) · custo total %2")
                           .arg(saidas.size()).arg(dinheiro(SaidaUsoProprio_service::custoTotal(saidas))));
}
