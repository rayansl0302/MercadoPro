#include "contapagardialogs.h"
#include "services/empresa_service.h"
#include "util/icones.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
QLocale ptBR() { return QLocale(QLocale::Portuguese, QLocale::Brazil); }

QString dinheiro(double v) { return ptBR().toCurrencyString(v, "R$ "); }

QDoubleSpinBox *campoDinheiro()
{
    auto *sp = new QDoubleSpinBox;
    sp->setLocale(ptBR());
    sp->setDecimals(2);
    sp->setRange(0.0, 99999999.99);
    sp->setPrefix("R$ ");
    sp->setGroupSeparatorShown(true);
    sp->setButtonSymbols(QAbstractSpinBox::NoButtons);
    sp->setAlignment(Qt::AlignRight);
    return sp;
}

QDateEdit *campoData(const QDate &inicial)
{
    auto *d = new QDateEdit(inicial);
    d->setCalendarPopup(true);
    d->setDisplayFormat("dd/MM/yyyy");
    d->setLocale(ptBR());
    return d;
}
}

// ---------------- NovaContaDialog ----------------

NovaContaDialog::NovaContaDialog(const ContaPagarDTO &conta, QWidget *parent)
    : QDialog(parent)
    , idConta(conta.id)
{
    const bool edicao = conta.id > 0;
    setWindowTitle(edicao ? "Editar conta a pagar" : "Nova conta a pagar");
    setModal(true);
    setMinimumWidth(520);

    edDescricao = new QLineEdit(conta.descricao);
    edDescricao->setPlaceholderText("Ex.: Compra de mercadoria, Aluguel de outubro");
    edFornecedor = new QLineEdit(conta.fornecedor);
    edFornecedor->setPlaceholderText("Quem vai receber");
    cbCategoria = new QComboBox;
    cbCategoria->setEditable(true);
    cbCategoria->addItems(ContasPagar_service::categoriasSugeridas());
    cbCategoria->setCurrentText(conta.categoria.isEmpty() ? ContasPagar_service::categoriasSugeridas().first()
                                                          : conta.categoria);
    edDocumento = new QLineEdit(conta.documento);
    edDocumento->setPlaceholderText("Nº da nota ou do boleto (opcional)");
    spValor = campoDinheiro();
    spValor->setValue(conta.valor);
    const QDate venc = conta.dataVencimento().isValid() ? conta.dataVencimento() : QDate::currentDate();
    dtVencimento = campoData(venc);

    spParcelas = new QSpinBox;
    spParcelas->setRange(1, 120);
    spParcelas->setValue(1);
    spParcelas->setSuffix(" parcela(s)");
    cbPeriodicidade = new QComboBox;
    cbPeriodicidade->addItem("Mensal", int(ContasPagar_service::Periodicidade::Mensal));
    cbPeriodicidade->addItem("Quinzenal", int(ContasPagar_service::Periodicidade::Quinzenal));
    cbPeriodicidade->addItem("Semanal", int(ContasPagar_service::Periodicidade::Semanal));
    if (edicao) {
        spParcelas->setEnabled(false);
        cbPeriodicidade->setEnabled(false);
    }
    if (conta.status == kContaPaga) {
        // conta paga: valor e vencimento só mudam estornando o pagamento
        spValor->setEnabled(false);
        dtVencimento->setEnabled(false);
        spValor->setToolTip("Para mudar o valor ou o vencimento, estorne o pagamento antes.");
        dtVencimento->setToolTip("Para mudar o valor ou o vencimento, estorne o pagamento antes.");
    }

    edObservacao = new QPlainTextEdit(conta.observacao);
    edObservacao->setMaximumHeight(64);

    const EmpresaDTO empresa = Empresa_service::instancia()->ativa();
    auto *lblEmpresa = new QLabel(edicao ? QString() : QStringLiteral("Esta conta será lançada na empresa <b>%1</b>.")
                                                           .arg(empresa.apelido));
    lblEmpresa->setTextFormat(Qt::RichText);
    lblEmpresa->setStyleSheet("color: #142D45;");
    lblEmpresa->setVisible(!edicao);

    auto *form = new QFormLayout;
    form->addRow("Descrição *", edDescricao);
    form->addRow("Fornecedor", edFornecedor);
    form->addRow("Categoria", cbCategoria);
    form->addRow("Documento", edDocumento);
    form->addRow("Valor total *", spValor);
    form->addRow(edicao ? "Vencimento *" : "1º vencimento *", dtVencimento);
    form->addRow("Parcelas", spParcelas);
    form->addRow("Intervalo", cbPeriodicidade);
    form->addRow("Observação", edObservacao);

    lblPrevia = new QLabel;
    lblPrevia->setWordWrap(true);
    lblPrevia->setStyleSheet("background: #EAF6EC; color: #107A2E; border-radius: 8px; padding: 8px 10px;");
    lblAviso = new QLabel;
    lblAviso->setWordWrap(true);
    lblAviso->setStyleSheet("color: #B91C1C;");

    auto *botoes = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    botoes->button(QDialogButtonBox::Ok)->setText(edicao ? "Salvar" : "Lançar");
    botoes->button(QDialogButtonBox::Cancel)->setText("Cancelar");
    connect(botoes, &QDialogButtonBox::accepted, this, &NovaContaDialog::confirmar);
    connect(botoes, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->addWidget(lblEmpresa);
    layout->addLayout(form);
    layout->addWidget(lblPrevia);
    layout->addWidget(lblAviso);
    layout->addWidget(botoes);

    connect(spValor, &QDoubleSpinBox::valueChanged, this, &NovaContaDialog::atualizarPrevia);
    connect(dtVencimento, &QDateEdit::dateChanged, this, &NovaContaDialog::atualizarPrevia);
    connect(spParcelas, &QSpinBox::valueChanged, this, &NovaContaDialog::atualizarPrevia);
    connect(cbPeriodicidade, &QComboBox::currentIndexChanged, this, &NovaContaDialog::atualizarPrevia);
    atualizarPrevia();
    edDescricao->setFocus();
}

int NovaContaDialog::parcelas() const { return spParcelas->value(); }

ContasPagar_service::Periodicidade NovaContaDialog::periodicidade() const
{
    return ContasPagar_service::Periodicidade(cbPeriodicidade->currentData().toInt());
}

ContaPagarDTO NovaContaDialog::conta() const
{
    ContaPagarDTO c;
    c.id = idConta;
    c.descricao = edDescricao->text().trimmed();
    c.fornecedor = edFornecedor->text().trimmed();
    c.categoria = cbCategoria->currentText().trimmed();
    c.documento = edDocumento->text().trimmed();
    c.valor = spValor->value();
    c.vencimento = dtVencimento->date().toString(Qt::ISODate);
    c.observacao = edObservacao->toPlainText().trimmed();
    return c;
}

void NovaContaDialog::atualizarPrevia()
{
    const int n = spParcelas->value();
    if (idConta > 0 || n <= 1 || spValor->value() <= 0) {
        lblPrevia->setVisible(false);
        return;
    }
    const QList<double> valores = ContasPagar_service::dividirValor(spValor->value(), n);
    QStringList datas;
    const QDate primeiro = dtVencimento->date();
    for (int i = 0; i < qMin(n, 6); ++i) {
        QDate d = primeiro;
        switch (periodicidade()) {
        case ContasPagar_service::Periodicidade::Mensal: d = primeiro.addMonths(i); break;
        case ContasPagar_service::Periodicidade::Quinzenal: d = primeiro.addDays(15 * i); break;
        case ContasPagar_service::Periodicidade::Semanal: d = primeiro.addDays(7 * i); break;
        }
        datas << d.toString("dd/MM/yy");
    }
    QString texto = QStringLiteral("%1 parcelas de %2").arg(n).arg(dinheiro(valores.first()));
    if (!qFuzzyCompare(valores.first() + 1, valores.last() + 1))
        texto += QStringLiteral(" (a última de %1)").arg(dinheiro(valores.last()));
    texto += QStringLiteral(" — vencimentos: %1%2").arg(datas.join(", "), n > 6 ? QStringLiteral(", ...") : QString());
    lblPrevia->setText(texto);
    lblPrevia->setVisible(true);
}

void NovaContaDialog::confirmar()
{
    if (edDescricao->text().trimmed().size() < 2) {
        lblAviso->setText("Informe a descrição da conta.");
        edDescricao->setFocus();
        return;
    }
    if (spValor->value() <= 0) {
        lblAviso->setText("Informe um valor maior que zero.");
        spValor->setFocus();
        return;
    }
    accept();
}

// ---------------- BaixaContaDialog ----------------

BaixaContaDialog::BaixaContaDialog(const ContaPagarDTO &conta, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Baixar conta");
    setModal(true);
    setMinimumWidth(460);

    auto *resumo = new QLabel(QStringLiteral("<b>%1</b><br>%2 · vence em %3 · %4")
                                  .arg(conta.descricao.toHtmlEscaped(),
                                       conta.fornecedor.isEmpty() ? QStringLiteral("sem fornecedor")
                                                                  : conta.fornecedor.toHtmlEscaped(),
                                       conta.dataVencimento().toString("dd/MM/yyyy"), dinheiro(conta.valor)));
    resumo->setTextFormat(Qt::RichText);
    resumo->setWordWrap(true);
    resumo->setStyleSheet("background: #EAF6EC; color: #107A2E; border-radius: 8px; padding: 8px 10px;");

    spValor = campoDinheiro();
    spValor->setValue(conta.valor);
    cbForma = new QComboBox;
    cbForma->addItems(ContasPagar_service::formasDePagamento());
    dtPagamento = campoData(QDate::currentDate());
    dtPagamento->setMaximumDate(QDate::currentDate());

    auto *dica = new QLabel("Se pagou com juros ou desconto, informe o valor realmente pago.");
    dica->setStyleSheet("color: #5B6B7F;");

    auto *form = new QFormLayout;
    form->addRow("Valor pago", spValor);
    form->addRow("Forma de pagamento", cbForma);
    form->addRow("Data do pagamento", dtPagamento);

    auto *botoes = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    botoes->button(QDialogButtonBox::Ok)->setText("Confirmar pagamento");
    botoes->button(QDialogButtonBox::Ok)->setIcon(Icones::icone("check", QColor(Qt::white), 18));
    botoes->button(QDialogButtonBox::Cancel)->setText("Cancelar");
    connect(botoes, &QDialogButtonBox::accepted, this, [this]() {
        if (spValor->value() > 0)
            accept();
    });
    connect(botoes, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->addWidget(resumo);
    layout->addLayout(form);
    layout->addWidget(dica);
    layout->addWidget(botoes);
    spValor->setFocus();
    spValor->selectAll();
}

double BaixaContaDialog::valorPago() const { return spValor->value(); }
QString BaixaContaDialog::forma() const { return cbForma->currentText(); }

QDateTime BaixaContaDialog::quando() const
{
    // a data escolhida com a hora atual (a baixa de hoje fica com a hora real)
    return QDateTime(dtPagamento->date(), QTime::currentTime());
}
