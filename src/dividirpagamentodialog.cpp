#include "dividirpagamentodialog.h"

#include "seletormaquinacartao.h"
#include "services/pagamentodividido_service.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleValidator>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QVBoxLayout>

namespace {
const QStringList kFormas = {QStringLiteral("Dinheiro"), QStringLiteral("Pix"),
                             QStringLiteral("Crédito"), QStringLiteral("Débito")};
constexpr int kMaxLinhas = 4;

QString dinheiro(double v)
{
    return QLocale(QLocale::Portuguese, QLocale::Brazil).toCurrencyString(v, QStringLiteral("R$ "));
}
double arred(double v) { return qRound64(v * 100.0) / 100.0; }
}

double DividirPagamentoDialog::lerValor(const QString &texto)
{
    QString t = texto.trimmed();
    t.remove(QLatin1Char('.'));          // separador de milhar
    t.replace(QLatin1Char(','), QLatin1Char('.'));
    return t.toDouble();
}

QString DividirPagamentoDialog::montar(double valorFinal, const QList<Entrega> &entregas, QList<PagamentoVendaDTO> *saida)
{
    return PagamentoDividido::montar(valorFinal, entregas, saida);
}

DividirPagamentoDialog::DividirPagamentoDialog(double valorFinal, double taxaCreditoPadrao, double taxaDebitoPadrao,
                                               QWidget *parent)
    : QDialog(parent), valorFinal(valorFinal), taxaCredito(taxaCreditoPadrao), taxaDebito(taxaDebitoPadrao)
{
    setWindowTitle(QStringLiteral("Dividir pagamento"));
    setModal(true);
    setMinimumWidth(430);

    auto *lay = new QVBoxLayout(this);
    auto *lblTotal = new QLabel(QStringLiteral("Valor a pagar: <b>%1</b>").arg(dinheiro(valorFinal)), this);
    lblTotal->setStyleSheet(QStringLiteral("font-size: 16px;"));
    lay->addWidget(lblTotal);
    lay->addWidget(new QLabel(QStringLiteral("Digite quanto o cliente entregou em cada forma. O troco sai do dinheiro."), this));

    grade = new QGridLayout;
    grade->setColumnStretch(1, 1);
    lay->addLayout(grade);
    adicionarLinha(QStringLiteral("Dinheiro"));
    adicionarLinha(QStringLiteral("Pix"));

    auto *btnMais = new QPushButton(QStringLiteral("+ Adicionar outra forma"), this);
    connect(btnMais, &QPushButton::clicked, this, [this, btnMais]() {
        // escolhe uma forma ainda não usada
        QSet<QString> usadas;
        for (const Linha &l : linhas)
            usadas.insert(l.forma->currentText());
        QString livre = kFormas.first();
        for (const QString &f : kFormas)
            if (!usadas.contains(f)) { livre = f; break; }
        adicionarLinha(livre);
        btnMais->setVisible(linhas.size() < kMaxLinhas);
    });
    lay->addWidget(btnMais, 0, Qt::AlignLeft);

    cbMaquina = new QComboBox(this);
    lblMaquina = new QLabel(QStringLiteral("Maquininha do cartão:"), this);
    auto *linhaMaquina = new QHBoxLayout;
    linhaMaquina->addWidget(lblMaquina);
    linhaMaquina->addWidget(cbMaquina, 1);
    lay->addLayout(linhaMaquina);
    seletor = new SeletorMaquinaCartao(cbMaquina, lblMaquina, this);

    lblSituacao = new QLabel(this);
    lblSituacao->setWordWrap(true);
    lblSituacao->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: bold;"));
    lay->addWidget(lblSituacao);

    auto *botoes = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    btnOk = botoes->button(QDialogButtonBox::Ok);
    btnOk->setText(QStringLiteral("Confirmar divisão"));
    botoes->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Cancelar"));
    connect(botoes, &QDialogButtonBox::accepted, this, &DividirPagamentoDialog::confirmar);
    connect(botoes, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(botoes);

    // sugestão: metade em cada uma, o operador ajusta
    const QLocale br(QLocale::Portuguese, QLocale::Brazil);
    const double metade = arred(valorFinal / 2.0);
    linhas.first().valor->setText(br.toString(metade, 'f', 2));
    linhas.last().valor->setText(br.toString(arred(valorFinal - metade), 'f', 2));
    atualizar();
    linhas.first().valor->setFocus();
    linhas.first().valor->selectAll();
}

void DividirPagamentoDialog::adicionarLinha(const QString &formaInicial)
{
    Linha l;
    l.forma = new QComboBox(this);
    l.forma->addItems(kFormas);
    l.forma->setCurrentText(formaInicial);
    l.valor = new QLineEdit(this);
    l.valor->setPlaceholderText(QStringLiteral("0,00"));
    auto *val = new QDoubleValidator(0.0, 9999999.99, 2, l.valor);
    val->setNotation(QDoubleValidator::StandardNotation);
    val->setLocale(QLocale(QLocale::Portuguese, QLocale::Brazil));
    l.valor->setValidator(val);
    const int linha = linhas.size();
    grade->addWidget(l.forma, linha, 0);
    grade->addWidget(new QLabel(QStringLiteral("R$"), this), linha, 1, Qt::AlignRight);
    grade->addWidget(l.valor, linha, 2);
    linhas.append(l);
    connect(l.forma, &QComboBox::currentTextChanged, this, [this]() { atualizar(); });
    QLineEdit *quem = l.valor;
    connect(l.valor, &QLineEdit::textEdited, this, [this, quem]() { aoEditar(quem); });
    // a última linha recebe o que falta, até o operador mexer nela
    ultimaManual = false;
    if (linhas.size() > 2)
        quem->clear();
    atualizar();
}

void DividirPagamentoDialog::aoEditar(QLineEdit *quem)
{
    // última linha: deixa de ser automática; as outras: a última completa o que falta
    if (quem == linhas.last().valor) {
        ultimaManual = true;
    } else if (!ultimaManual) {
        double outras = 0;
        for (int i = 0; i < linhas.size() - 1; ++i)
            outras += lerValor(linhas.at(i).valor->text());
        const double falta = arred(valorFinal - outras);
        linhas.last().valor->setText(falta > 0
                                         ? QLocale(QLocale::Portuguese, QLocale::Brazil).toString(falta, 'f', 2)
                                         : QString());
    }
    atualizar();
}

QList<DividirPagamentoDialog::Entrega> DividirPagamentoDialog::entregas() const
{
    QList<Entrega> lista;
    for (const Linha &l : linhas)
        lista.append({l.forma->currentText(), lerValor(l.valor->text())});
    return lista;
}

void DividirPagamentoDialog::atualizar()
{
    if (!seletor || !btnOk || !lblSituacao)   // ainda montando a tela
        return;
    bool cartao = false;
    double soma = 0;
    for (const Linha &l : linhas) {
        const double v = lerValor(l.valor->text());
        if (v > 0 && (l.forma->currentText() == "Crédito" || l.forma->currentText() == "Débito"))
            cartao = true;
        soma += v;
    }
    seletor->mostrar(cartao);

    QList<PagamentoVendaDTO> prontas;
    const QString erro = montar(valorFinal, entregas(), &prontas);
    btnOk->setEnabled(erro.isEmpty());
    if (!erro.isEmpty()) {
        const double falta = arred(valorFinal - soma);
        lblSituacao->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: bold; color: #B45309;"));
        lblSituacao->setText(falta > 0.009 ? QStringLiteral("Falta: %1").arg(dinheiro(falta)) : erro);
        return;
    }
    double troco = 0;
    for (const PagamentoVendaDTO &p : prontas)
        troco += p.troco;
    lblSituacao->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: bold; color: #107A2E;"));
    lblSituacao->setText(troco > 0.009 ? QStringLiteral("Confere. Troco: %1").arg(dinheiro(troco))
                                       : QStringLiteral("Confere."));
}

void DividirPagamentoDialog::confirmar()
{
    QList<PagamentoVendaDTO> prontas;
    const QString erro = montar(valorFinal, entregas(), &prontas);
    if (!erro.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Dividir pagamento"), erro);
        return;
    }
    for (PagamentoVendaDTO &p : prontas) {
        if (p.forma == "Crédito" || p.forma == "Débito") {
            p.idMaquina = seletor->idMaquina();
            p.taxaMaquina = seletor->taxaDaForma(p.forma, p.forma == "Crédito" ? taxaCredito : taxaDebito);
        }
    }
    partes = prontas;
    accept();
}
