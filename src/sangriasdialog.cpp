#include "sangriasdialog.h"

#include <QDate>
#include <QDateEdit>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPageLayout>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QPushButton>
#include <QTableWidget>
#include <QTextDocument>
#include <QVBoxLayout>

namespace {
QString dinheiro(double v)
{
    return QLocale(QLocale::Portuguese, QLocale::Brazil).toCurrencyString(v, QStringLiteral("R$ "));
}
}

SangriasDialog::SangriasDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Sangrias do dia"));
    resize(760, 520);

    dtDe = new QDateEdit(QDate::currentDate(), this);
    dtAte = new QDateEdit(QDate::currentDate(), this);
    for (QDateEdit *d : {dtDe, dtAte}) {
        d->setCalendarPopup(true);
        d->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));
    }
    auto *btnHoje = new QPushButton(QStringLiteral("Hoje"), this);
    connect(btnHoje, &QPushButton::clicked, this, [this]() {
        dtDe->setDate(QDate::currentDate());
        dtAte->setDate(QDate::currentDate());
    });
    auto *topo = new QHBoxLayout;
    topo->addWidget(new QLabel(QStringLiteral("De:"), this));
    topo->addWidget(dtDe);
    topo->addWidget(new QLabel(QStringLiteral("Até:"), this));
    topo->addWidget(dtAte);
    topo->addWidget(btnHoje);
    topo->addStretch();

    tabela = new QTableWidget(0, 5, this);
    tabela->setHorizontalHeaderLabels({QStringLiteral("Data/hora"), QStringLiteral("Valor"), QStringLiteral("Motivo"),
                                       QStringLiteral("Caixa de"), QStringLiteral("Caixa nº")});
    tabela->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tabela->setSelectionBehavior(QAbstractItemView::SelectRows);
    tabela->verticalHeader()->setVisible(false);
    tabela->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    tabela->setAlternatingRowColors(true);

    lblTotal = new QLabel(this);
    lblTotal->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold;"));

    auto *btnImprimir = new QPushButton(QStringLiteral("Imprimir / PDF"), this);
    auto *btnFechar = new QPushButton(QStringLiteral("Fechar"), this);
    connect(btnImprimir, &QPushButton::clicked, this, &SangriasDialog::imprimir);
    connect(btnFechar, &QPushButton::clicked, this, &QDialog::accept);
    auto *base = new QHBoxLayout;
    base->addWidget(lblTotal);
    base->addStretch();
    base->addWidget(btnImprimir);
    base->addWidget(btnFechar);

    auto *lay = new QVBoxLayout(this);
    lay->addLayout(topo);
    lay->addWidget(tabela, 1);
    lay->addLayout(base);

    connect(dtDe, &QDateEdit::userDateChanged, this, &SangriasDialog::atualizar);
    connect(dtAte, &QDateEdit::userDateChanged, this, &SangriasDialog::atualizar);
    connect(btnHoje, &QPushButton::clicked, this, &SangriasDialog::atualizar);
    atualizar();
}

void SangriasDialog::atualizar()
{
    QDate de = dtDe->date();
    QDate ate = dtAte->date();
    if (ate < de)
        ate = de;
    atuais = caixaServ.sangrias(de, ate);
    tabela->setRowCount(atuais.size());
    double total = 0;
    for (int i = 0; i < atuais.size(); ++i) {
        const MovimentacaoCaixaDTO &m = atuais.at(i);
        total += m.valor;
        const QDateTime quando = QDateTime::fromString(m.dataHora, Qt::ISODate);
        auto *valor = new QTableWidgetItem(dinheiro(m.valor));
        valor->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        tabela->setItem(i, 0, new QTableWidgetItem(quando.toString(QStringLiteral("dd/MM/yyyy HH:mm"))));
        tabela->setItem(i, 1, valor);
        tabela->setItem(i, 2, new QTableWidgetItem(m.motivo));
        tabela->setItem(i, 3, new QTableWidgetItem(m.nomeOperador));
        tabela->setItem(i, 4, new QTableWidgetItem(QString::number(m.idCaixa)));
    }
    tabela->resizeColumnsToContents();
    tabela->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    lblTotal->setText(atuais.isEmpty()
                          ? QStringLiteral("Nenhuma sangria no período.")
                          : QStringLiteral("Total retirado: %1  (%2 sangria%3)")
                                .arg(dinheiro(total)).arg(atuais.size()).arg(atuais.size() == 1 ? "" : "s"));
}

void SangriasDialog::imprimir()
{
    double total = 0;
    QString h = QStringLiteral("<h2>Sangrias do caixa</h2><p>Período: %1 a %2</p>"
                               "<table width='100%' border='1' cellspacing='0' cellpadding='4'>"
                               "<tr><th>Data/hora</th><th align='right'>Valor</th><th>Motivo</th><th>Caixa de</th></tr>")
                    .arg(dtDe->date().toString(QStringLiteral("dd/MM/yyyy")), dtAte->date().toString(QStringLiteral("dd/MM/yyyy")));
    for (const MovimentacaoCaixaDTO &m : atuais) {
        total += m.valor;
        h += QStringLiteral("<tr><td>%1</td><td align='right'>%2</td><td>%3</td><td>%4</td></tr>")
                 .arg(QDateTime::fromString(m.dataHora, Qt::ISODate).toString(QStringLiteral("dd/MM/yyyy HH:mm")),
                      dinheiro(m.valor), m.motivo.toHtmlEscaped(), m.nomeOperador.toHtmlEscaped());
    }
    h += QStringLiteral("<tr><th>Total</th><th align='right'>%1</th><th colspan='2'></th></tr></table>").arg(dinheiro(total));

    QPrinter printer(QPrinter::HighResolution);
    printer.setPageSize(QPageSize(QPageSize::A4));
    QPrintPreviewDialog prev(&printer, this);
    connect(&prev, &QPrintPreviewDialog::paintRequested, this, [h](QPrinter *p) {
        QTextDocument doc;
        doc.setHtml(h);
        doc.print(p);
    });
    prev.exec();
}
