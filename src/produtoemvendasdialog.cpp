#include "produtoemvendasdialog.h"
#include "services/empresa_service.h"
#include "util/icones.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
QString dinheiro(double v) { return QLocale(QLocale::Portuguese, QLocale::Brazil).toCurrencyString(v, "R$ "); }
}

ProdutoEmVendasDialog::ProdutoEmVendasDialog(const QString &descricaoProduto,
                                             const QList<ProdutoVendaRefDTO> &vendas,
                                             std::function<void()> abrirVendas, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Produto já vendido");
    setModal(true);
    resize(860, 520);

    auto *titulo = new QLabel("Este produto não pode ser apagado");
    titulo->setStyleSheet("font-size: 16pt; font-weight: 700; color: #142D45;");

    auto *explica = new QLabel(QStringLiteral(
        "<b>%1</b> aparece em <b>%2</b> venda(s) abaixo, e o histórico dessas vendas depende dele.<br>"
        "Para apagar o produto, primeiro cancele/exclua essas vendas (em <b>Vendas</b>). "
        "Se só quer parar de vender, deixe o estoque em zero.")
                                .arg(descricaoProduto.toHtmlEscaped()).arg(vendas.size()));
    explica->setTextFormat(Qt::RichText);
    explica->setWordWrap(true);
    explica->setStyleSheet("color: #475569; background: #FFFBEB; border: 1px solid #FCD34D; border-radius: 8px; "
                           "padding: 8px 10px;");

    auto *tabela = new QTableWidget(vendas.size(), 8);
    tabela->setHorizontalHeaderLabels({"Venda", "Data", "Cliente", "Qtd.", "Preço unit.", "Total da venda",
                                       "Pagamento", "Empresa"});
    tabela->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tabela->setSelectionBehavior(QAbstractItemView::SelectRows);
    tabela->setAlternatingRowColors(true);
    tabela->verticalHeader()->setVisible(false);
    tabela->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    for (int c : {0, 1, 3, 4, 5, 6, 7})
        tabela->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);

    auto *es = Empresa_service::instancia();
    for (int i = 0; i < vendas.size(); ++i) {
        const ProdutoVendaRefDTO &v = vendas.at(i);
        auto celula = [&](int col, const QString &texto, Qt::Alignment a = Qt::AlignLeft | Qt::AlignVCenter) {
            auto *it = new QTableWidgetItem(texto);
            it->setTextAlignment(a);
            tabela->setItem(i, col, it);
        };
        const QDateTime d = QDateTime::fromString(v.dataHora, "yyyy-MM-dd HH:mm:ss");
        const EmpresaDTO e = es->getPorId(v.idEmpresa);
        celula(0, v.idVenda > 0 ? QStringLiteral("#%1").arg(v.idVenda) : QStringLiteral("sem venda"), Qt::AlignCenter);
        celula(1, d.isValid() ? d.toString("dd/MM/yyyy HH:mm") : v.dataHora);
        celula(2, v.cliente);
        celula(3, QLocale().toString(v.quantidade, 'g', 8), Qt::AlignCenter);
        celula(4, dinheiro(v.precoVendido), Qt::AlignRight | Qt::AlignVCenter);
        celula(5, v.idVenda > 0 ? dinheiro(v.valorFinalVenda) : QStringLiteral("—"), Qt::AlignRight | Qt::AlignVCenter);
        celula(6, v.formaPagamento + (!v.estaPago && v.idVenda > 0 ? QStringLiteral(" (em aberto)") : QString()));
        celula(7, e.valida() ? e.apelido : QStringLiteral("—"));
    }

    auto *btnVendas = new QPushButton("Abrir Vendas");
    btnVendas->setIcon(Icones::icone("receipt", QColor(Qt::white), 18));
    btnVendas->setVisible(bool(abrirVendas));
    connect(btnVendas, &QPushButton::clicked, this, [this, abrirVendas]() {
        accept();
        abrirVendas();
    });
    auto *btnFechar = new QPushButton("Fechar");
    btnFechar->setStyleSheet("QPushButton { background: white; color: #142D45; border: 1px solid #C9D3DF; }"
                             "QPushButton:hover { background: #EAF6EC; }");
    connect(btnFechar, &QPushButton::clicked, this, &QDialog::accept);

    auto *botoes = new QHBoxLayout;
    botoes->addStretch(1);
    botoes->addWidget(btnVendas);
    botoes->addWidget(btnFechar);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(10);
    layout->addWidget(titulo);
    layout->addWidget(explica);
    layout->addWidget(tabela, 1);
    layout->addLayout(botoes);
}
