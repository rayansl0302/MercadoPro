#include "saidausopropriodialog.h"
#include "services/Produto_service.h"
#include "services/saidausoproprio_service.h"
#include "services/sessao_service.h"

#include <QComboBox>
#include <QListWidget>
#include <QSqlQueryModel>
#include <QSqlRecord>
#include <QDialogButtonBox>
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
    if (motivo_->currentText() == QLatin1String("Outro") && observacao_->text().trimmed().size() < 3) {
        QMessageBox::warning(this, "Saída para uso próprio", "Em \"Outro\", explique o motivo no campo de observação.");
        observacao_->setFocus();
        return;
    }
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
    auto *btnNova = new QPushButton("Nova saída...");
    btnNova->setToolTip("Tirar um produto do estoque (consumo, quebra, vencido...) com motivo");
    connect(btnNova, &QPushButton::clicked, this, &HistoricoSaidasDialog::novaSaida);

    auto *btnRemover = new QPushButton("Remover saída...");
    btnRemover->setStyleSheet(kEstiloSecundario);
    btnRemover->setToolTip("Apagar a saída selecionada (lançada por engano) e devolver a quantidade ao estoque");
    connect(btnRemover, &QPushButton::clicked, this, &HistoricoSaidasDialog::removerSelecionada);

    auto *rodape = new QHBoxLayout;
    rodape->addWidget(lblTotal_, 1);
    rodape->addWidget(btnRemover);
    rodape->addWidget(btnNova);
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

// Escolhe o produto pelo código de barras (leitor) ou por parte do nome e abre a saída dele.
void HistoricoSaidasDialog::novaSaida()
{
    QDialog busca(this);
    busca.setWindowTitle("Nova saída de estoque");
    busca.resize(560, 420);
    auto *lay = new QVBoxLayout(&busca);
    lay->addWidget(new QLabel("Leia o código de barras ou digite parte do nome do produto:"));
    auto *campo = new QLineEdit;
    lay->addWidget(campo);
    auto *lista = new QListWidget;
    lay->addWidget(lista, 1);
    auto *botoes = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    botoes->button(QDialogButtonBox::Ok)->setText("Continuar");
    botoes->button(QDialogButtonBox::Cancel)->setText("Cancelar");
    lay->addWidget(botoes);
    connect(botoes, &QDialogButtonBox::rejected, &busca, &QDialog::reject);
    connect(botoes, &QDialogButtonBox::accepted, &busca, &QDialog::accept);
    connect(lista, &QListWidget::itemDoubleClicked, &busca, &QDialog::accept);

    auto buscar = [&]() {
        lista->clear();
        const QString t = campo->text().trimmed();
        if (t.size() < 2)
            return;
        Produto_Service ps;
        const ProdutoDTO exato = ps.getProdutoPeloCodBarras(t);
        if (exato.id > 0) {
            auto *it = new QListWidgetItem(exato.descricao);
            it->setData(Qt::UserRole, exato.id);
            lista->addItem(it);
            lista->setCurrentRow(0);
            return;
        }
        QSqlQueryModel modelo;
        ps.pesquisar(t, &modelo);
        while (modelo.canFetchMore())
            modelo.fetchMore();
        for (int i = 0; i < modelo.rowCount() && i < 200; ++i) {
            const QSqlRecord r = modelo.record(i);
            auto *it = new QListWidgetItem(r.value("descricao").toString());
            it->setData(Qt::UserRole, r.value("id"));
            lista->addItem(it);
        }
        if (lista->count() > 0)
            lista->setCurrentRow(0);
    };
    connect(campo, &QLineEdit::textChanged, &busca, buscar);
    // leitor de código termina com Enter: já segue com o primeiro resultado
    connect(campo, &QLineEdit::returnPressed, &busca, [&]() {
        if (lista->currentItem())
            busca.accept();
    });
    campo->setFocus();
    if (busca.exec() != QDialog::Accepted || !lista->currentItem())
        return;

    SaidaUsoProprioDialog dlg(lista->currentItem()->data(Qt::UserRole).toLongLong(), this);
    if (dlg.exec() == QDialog::Accepted && dlg.registrou())
        carregar();
}

void HistoricoSaidasDialog::removerSelecionada()
{
    const int linha = tabela_->currentRow();
    if (linha < 0 || !tabela_->item(linha, 0)) {
        QMessageBox::information(this, "Saídas para uso próprio", "Clique na saída que deseja remover.");
        return;
    }
    const qlonglong id = tabela_->item(linha, 0)->data(Qt::UserRole).toLongLong();
    const QString produto = tabela_->item(linha, 1)->text();
    const QString qtd = tabela_->item(linha, 2)->text();

    QMessageBox pergunta(QMessageBox::Question, "Remover saída",
                         QStringLiteral("Remover a saída de %1 x %2?\n\nA quantidade volta para o estoque e a saída "
                                        "deixa de aparecer no histórico.").arg(qtd, produto),
                         QMessageBox::NoButton, this);
    QPushButton *sim = pergunta.addButton("Remover", QMessageBox::AcceptRole);
    pergunta.addButton("Cancelar", QMessageBox::RejectRole);
    pergunta.exec();
    if (pergunta.clickedButton() != sim)
        return;

    SaidaUsoProprio_service servico;
    const auto r = servico.remover(id);
    if (!r.ok)
        QMessageBox::warning(this, "Saídas para uso próprio", r.msg);
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
        tabela_->item(i, 0)->setData(Qt::UserRole, s.id);
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
