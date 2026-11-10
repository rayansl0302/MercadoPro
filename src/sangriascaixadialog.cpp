#include "sangriascaixadialog.h"
#include "movimentacaocaixa.h"
#include "services/relatoriocaixa_service.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

SangriasCaixaDialog::SangriasCaixaDialog(bool iniciarComSuprimento, QWidget *parent)
    : QDialog(parent)
    , portugues(QLocale::Portuguese, QLocale::Brazil)
{
    setWindowTitle("Sangrias e suprimentos do caixa");
    resize(820, 480);

    auto *layout = new QVBoxLayout(this);
    resumo = new QLabel(this);
    resumo->setWordWrap(true);
    layout->addWidget(resumo);

    tabela = new QTableWidget(this);
    tabela->setColumnCount(6);
    tabela->setHorizontalHeaderLabels({"Hora", "Tipo", "Forma", "Valor (R$)", "Motivo", "Operador"});
    tabela->setSelectionBehavior(QAbstractItemView::SelectRows);
    tabela->setSelectionMode(QAbstractItemView::SingleSelection);
    tabela->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tabela->verticalHeader()->setVisible(false);
    connect(tabela, &QTableWidget::cellDoubleClicked, this, [this]() { editar(); });
    layout->addWidget(tabela, 1);

    auto *botoes = new QHBoxLayout;
    auto *btnSangria = new QPushButton("Nova sangria", this);
    auto *btnSuprimento = new QPushButton("Novo suprimento", this);
    auto *btnEditar = new QPushButton("Editar selecionado", this);
    auto *btnRemover = new QPushButton("Remover selecionado", this);
    auto *btnFechar = new QPushButton("Fechar", this);
    botoes->addWidget(btnSangria);
    botoes->addWidget(btnSuprimento);
    botoes->addSpacing(16);
    botoes->addWidget(btnEditar);
    botoes->addWidget(btnRemover);
    botoes->addStretch();
    botoes->addWidget(btnFechar);
    layout->addLayout(botoes);

    connect(btnSangria, &QPushButton::clicked, this, &SangriasCaixaDialog::novaSangria);
    connect(btnSuprimento, &QPushButton::clicked, this, &SangriasCaixaDialog::novoSuprimento);
    connect(btnEditar, &QPushButton::clicked, this, &SangriasCaixaDialog::editar);
    connect(btnRemover, &QPushButton::clicked, this, &SangriasCaixaDialog::remover);
    connect(btnFechar, &QPushButton::clicked, this, &QDialog::accept);

    recarregar();
    (iniciarComSuprimento ? btnSuprimento : btnSangria)->setFocus();
}

void SangriasCaixaDialog::recarregar()
{
    const CaixaDTO caixa = caixaServ.caixaAtual();
    itens = caixa.aberto() ? caixaServ.sangriasESuprimentosDoCaixa(caixa.id) : QList<MovimentacaoCaixaDTO>();

    double sangrias = 0, suprimentos = 0;
    tabela->setRowCount(itens.size());
    for (int i = 0; i < itens.size(); ++i) {
        const MovimentacaoCaixaDTO &m = itens.at(i);
        (m.tipo == "SANGRIA" ? sangrias : suprimentos) += m.valor;
        const QString quem = m.nomeOperadorSessao.isEmpty() ? m.nomeOperador : m.nomeOperadorSessao;
        const QStringList cel{RelatorioCaixa_service::formatarDataHora(m.dataHora),
                              m.tipo == "SANGRIA" ? "Sangria" : "Suprimento", m.formaPagamento,
                              portugues.toString(m.valor, 'f', 2), m.motivo, quem};
        for (int c = 0; c < cel.size(); ++c) {
            auto *item = new QTableWidgetItem(cel.at(c));
            if (c == 3)
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            tabela->setItem(i, c, item);
        }
    }
    tabela->resizeColumnsToContents();
    tabela->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);

    if (!caixa.aberto()) {
        resumo->setText("Nenhum caixa aberto neste terminal.");
        return;
    }
    resumo->setText(QString("Caixa #%1 · %2 — sangrias: R$ %3 · suprimentos: R$ %4. "
                            "Lançou duas vezes ou errou? Selecione a linha e use Editar ou Remover.")
                        .arg(caixa.id).arg(caixa.nomeOperador, portugues.toString(sangrias, 'f', 2),
                                           portugues.toString(suprimentos, 'f', 2)));
}

qlonglong SangriasCaixaDialog::idSelecionado()
{
    const int linha = tabela->currentRow();
    if (linha < 0 || linha >= itens.size()) {
        QMessageBox::information(this, "Caixa", "Selecione um lançamento na lista.");
        return 0;
    }
    return itens.at(linha).id;
}

void SangriasCaixaDialog::novo(bool suprimento)
{
    MovimentacaoCaixa dlg(suprimento ? MovimentacaoCaixa::Tipo::Suprimento : MovimentacaoCaixa::Tipo::Sangria, this);
    dlg.exec();
    recarregar();
}

void SangriasCaixaDialog::novaSangria() { novo(false); }
void SangriasCaixaDialog::novoSuprimento() { novo(true); }

void SangriasCaixaDialog::editar()
{
    const qlonglong id = idSelecionado();
    if (id <= 0)
        return;
    MovimentacaoCaixaDTO atual;
    for (const MovimentacaoCaixaDTO &m : itens)
        if (m.id == id)
            atual = m;

    // uma unica janela com tudo que pode ser corrigido
    QDialog ed(this);
    ed.setWindowTitle("Editar lançamento");
    ed.setMinimumWidth(480);
    auto *form = new QFormLayout;
    auto *sbValor = new QDoubleSpinBox(&ed);
    sbValor->setRange(0.01, 99999999.99);
    sbValor->setDecimals(2);
    sbValor->setLocale(portugues);
    sbValor->setValue(atual.valor);
    auto *edMotivo = new QLineEdit(atual.motivo, &ed);
    auto *cbForma = new QComboBox(&ed);
    const QStringList formas{"Dinheiro", "Crédito", "Débito", "Pix"};
    cbForma->addItems(formas);
    cbForma->setCurrentIndex(qMax(0, int(formas.indexOf(atual.formaPagamento))));
    auto *edCorrecao = new QLineEdit(&ed);
    edCorrecao->setPlaceholderText("ex.: digitei errado (fica registrado no log)");
    form->addRow("Valor correto (R$):", sbValor);
    form->addRow("Motivo:", edMotivo);
    form->addRow("Forma de pagamento:", cbForma);
    form->addRow("Por que está corrigindo?", edCorrecao);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &ed);
    connect(bb, &QDialogButtonBox::accepted, &ed, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &ed, &QDialog::reject);
    auto *lay = new QVBoxLayout(&ed);
    lay->addLayout(form);
    lay->addWidget(bb);
    edMotivo->setFocus();
    edMotivo->selectAll();
    if (ed.exec() != QDialog::Accepted)
        return;
    const double valor = sbValor->value();
    const QString motivo = edMotivo->text().trimmed();
    const QString forma = cbForma->currentText();
    const QString correcao = edCorrecao->text().trimmed();
    const auto r = caixaServ.editarSangriaSuprimento(id, valor, motivo, correcao, forma);
    if (!r.ok)
        QMessageBox::warning(this, "Caixa", r.msg);
    recarregar();
}

void SangriasCaixaDialog::remover()
{
    const qlonglong id = idSelecionado();
    if (id <= 0)
        return;
    MovimentacaoCaixaDTO atual;
    for (const MovimentacaoCaixaDTO &m : itens)
        if (m.id == id)
            atual = m;
    bool ok = false;
    const QString motivo = QInputDialog::getText(
        this, "Remover lançamento",
        QString("Remover %1 de R$ %2 (%3)?\nO caixa passa a não contar esse valor.\n\n"
                "Por que está removendo? (ex.: lançada em duplicidade)")
            .arg(atual.tipo == "SANGRIA" ? "a sangria" : "o suprimento",
                 portugues.toString(atual.valor, 'f', 2), atual.motivo),
        QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok)
        return;
    const auto r = caixaServ.removerSangriaSuprimento(id, motivo);
    if (!r.ok)
        QMessageBox::warning(this, "Caixa", r.msg);
    recarregar();
}
