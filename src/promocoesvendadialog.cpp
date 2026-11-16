#include "promocoesvendadialog.h"
#include "services/precotaxa_service.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

PromocoesVendaDialog::PromocoesVendaDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Promoções de venda"));
    resize(620, 420);

    auto *explicacao = new QLabel(
        QStringLiteral("A promoção aparece como uma caixa para marcar no pagamento da venda. Ao marcar, a taxa da "
                       "maquininha embutida no preço é tirada como desconto no final da venda (ex.: quem segue a loja "
                       "no Instagram e marca o @ num stories). Não mexe no estoque nem no preço cadastrado. Ligue (ON) e desligue (OFF) na mão, sem datas."),
        this);
    explicacao->setWordWrap(true);

    lblAviso = new QLabel(this);
    lblAviso->setWordWrap(true);
    lblAviso->setStyleSheet(QStringLiteral("color: rgb(180, 90, 0);"));

    tabela = new QTableWidget(0, 2, this);
    tabela->setHorizontalHeaderLabels({QStringLiteral("Promoção"), QStringLiteral("Ligada (ON/OFF)")});
    tabela->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tabela->setSelectionBehavior(QAbstractItemView::SelectRows);
    tabela->setSelectionMode(QAbstractItemView::SingleSelection);
    tabela->verticalHeader()->setVisible(false);
    tabela->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    tabela->setAlternatingRowColors(true);

    auto *btnNova = new QPushButton(QStringLiteral("Nova"), this);
    auto *btnRenomear = new QPushButton(QStringLiteral("Renomear"), this);
    auto *btnExcluir = new QPushButton(QStringLiteral("Excluir"), this);
    auto *btnFechar = new QPushButton(QStringLiteral("Fechar"), this);
    connect(btnNova, &QPushButton::clicked, this, &PromocoesVendaDialog::nova);
    connect(btnRenomear, &QPushButton::clicked, this, &PromocoesVendaDialog::renomear);
    // ligar/desligar na própria linha (sem agendamento: liga e desliga na mão)
    connect(tabela, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if (carregando || item->column() != 1 || item->row() >= lista.size())
            return;
        const PromocaoVendaDTO p = lista.at(item->row());
        const auto r = servico.salvar(p.id, p.nome, item->checkState() == Qt::Checked);
        if (!r.ok)
            QMessageBox::warning(this, QStringLiteral("Promoção"), r.msg);
        QTimer::singleShot(0, this, &PromocoesVendaDialog::recarregar);   // não trocar os itens dentro do próprio sinal
    });
    connect(btnExcluir, &QPushButton::clicked, this, &PromocoesVendaDialog::excluir);
    connect(btnFechar, &QPushButton::clicked, this, &QDialog::accept);
    connect(tabela, &QTableWidget::cellDoubleClicked, this, &PromocoesVendaDialog::renomear);

    auto *botoes = new QHBoxLayout;
    botoes->addWidget(btnNova);
    botoes->addWidget(btnRenomear);
    botoes->addWidget(btnExcluir);
    botoes->addStretch();
    botoes->addWidget(btnFechar);

    auto *lay = new QVBoxLayout(this);
    lay->addWidget(explicacao);
    lay->addWidget(lblAviso);
    lay->addWidget(tabela, 1);
    lay->addLayout(botoes);

    recarregar();
}

void PromocoesVendaDialog::recarregar()
{
    carregando = true;
    lista = servico.listar(false);
    tabela->setRowCount(lista.size());
    for (int i = 0; i < lista.size(); ++i) {
        tabela->setItem(i, 0, new QTableWidgetItem(lista.at(i).nome));
        auto *liga = new QTableWidgetItem(lista.at(i).ativa ? QStringLiteral("ON — aparece na venda")
                                                            : QStringLiteral("OFF — desligada"));
        liga->setFlags((liga->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
        liga->setCheckState(lista.at(i).ativa ? Qt::Checked : Qt::Unchecked);
        tabela->setItem(i, 1, liga);
    }
    tabela->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    carregando = false;
    const double taxa = PrecoTaxa::maiorTaxa();
    if (taxa <= 0) {
        lblAviso->setText(QStringLiteral("Atenção: nenhuma maquininha ativa com taxa cadastrada. Sem taxa, a promoção "
                                         "não dá desconto nenhum."));
    } else {
        lblAviso->setText(QStringLiteral("Taxa que a promoção anula hoje: %1% (a maior entre débito e crédito das "
                                         "maquininhas ativas).")
                              .arg(QLocale(QLocale::Portuguese, QLocale::Brazil).toString(taxa, 'f', 2)));
        lblAviso->setStyleSheet(QStringLiteral("color: rgb(29, 56, 74);"));
    }
}

qlonglong PromocoesVendaDialog::idSelecionado() const
{
    const int linha = tabela->currentRow();
    return (linha >= 0 && linha < lista.size()) ? lista.at(linha).id : 0;
}

void PromocoesVendaDialog::nova()
{
    bool ok = false;
    const QString nome = QInputDialog::getText(this, QStringLiteral("Nova promoção"), QStringLiteral("Nome da promoção:"),
                                               QLineEdit::Normal, QStringLiteral("Promoção rede social"), &ok);
    if (!ok)
        return;
    const auto r = servico.salvar(0, nome, true);
    if (!r.ok)
        QMessageBox::warning(this, QStringLiteral("Promoção"), r.msg);
    recarregar();
}

void PromocoesVendaDialog::renomear()
{
    const qlonglong id = idSelecionado();
    if (id <= 0)
        return;
    const PromocaoVendaDTO atual = lista.at(tabela->currentRow());
    bool ok = false;
    const QString nome = QInputDialog::getText(this, QStringLiteral("Renomear promoção"), QStringLiteral("Nome da promoção:"),
                                               QLineEdit::Normal, atual.nome, &ok);
    if (!ok)
        return;
    const auto r = servico.salvar(id, nome, atual.ativa);
    if (!r.ok)
        QMessageBox::warning(this, QStringLiteral("Promoção"), r.msg);
    recarregar();
}

void PromocoesVendaDialog::excluir()
{
    const qlonglong id = idSelecionado();
    if (id <= 0)
        return;
    const PromocaoVendaDTO atual = lista.at(tabela->currentRow());
    if (QMessageBox::question(this, QStringLiteral("Excluir promoção"),
                              QStringLiteral("Excluir \"%1\"?\n\nAs vendas já feitas continuam com o nome da promoção "
                                             "registrado. Para só parar de oferecer, desligue (OFF) na lista.")
                                  .arg(atual.nome)) != QMessageBox::Yes)
        return;
    const auto r = servico.excluir(id);
    if (!r.ok)
        QMessageBox::warning(this, QStringLiteral("Promoção"), r.msg);
    recarregar();
}
