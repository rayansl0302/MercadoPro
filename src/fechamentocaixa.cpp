#include "fechamentocaixa.h"
#include "ui_fechamentocaixa.h"
#include "relatoriocaixajanela.h"
#include "services/relatoriocaixa_service.h"
#include <QMessageBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleValidator>
#include <cmath>

FechamentoCaixa::FechamentoCaixa(QWidget *parent, qlonglong idCaixaAlvo)
    : QDialog(parent)
    , ui(new Ui::FechamentoCaixa)
    , portugues(QLocale::Portuguese, QLocale::Brazil)
{
    ui->setupUi(this);
    setWindowModality(Qt::ApplicationModal);

    const CaixaDTO caixa = idCaixaAlvo > 0 ? caixaServ.getCaixa(idCaixaAlvo) : caixaServ.caixaAtual();
    idCaixa = caixa.id;
    if (!caixa.aberto()) {
        ui->Lbl_Cabecalho->setText(idCaixaAlvo > 0 ? "Este caixa já está fechado."
                                                   : "Nenhum caixa aberto neste terminal.");
        ui->Btn_Fechar->setEnabled(false);
        return;
    }

    resumoAtual = caixaServ.resumo(idCaixa);
    ui->Lbl_Cabecalho->setText(QString("Caixa #%1 · %2 · aberto em %3 · terminal %4")
                                   .arg(caixa.id)
                                   .arg(caixa.nomeOperador)
                                   .arg(RelatorioCaixa_service::formatarDataHora(caixa.abertoEm),
                                        caixa.terminal));
    QString aviso = "Tolerância: " + caixaServ.descricaoTolerancia() +
                    ". Divergência em cartão/PIX é registrada como ocorrência técnica.";
    aviso += "\nRegistre antes as sangrias (dinheiro retirado para depósito) e conte só o que ficou na gaveta; "
             "sangria esquecida vira diferença no fechamento.";
    if (!resumoAtual.outrasFormas.isEmpty()) {
        QStringList itens;
        for (auto it = resumoAtual.outrasFormas.constBegin(); it != resumoAtual.outrasFormas.constEnd(); ++it)
            itens << QString("%1: R$ %2").arg(it.key(), portugues.toString(it.value(), 'f', 2));
        aviso += "\nAtenção: há valores em formas que não entram na conferência (" + itens.join("; ") + ").";
    }
    {
        // a conta do dinheiro esperado, para o operador ver que as sangrias já foram descontadas
        double vendasDinheiro = 0;
        for (const VendasFormaDTO &v : resumoAtual.vendasPorForma)
            if (v.formaPagamento == "Dinheiro")
                vendasDinheiro += v.total;
        const double receb = resumoAtual.recebimentosPorForma.value("Dinheiro", 0.0);
        auto r = [&](double v) { return portugues.toString(v, 'f', 2); };
        aviso += "\nDinheiro esperado: troco inicial " + r(resumoAtual.caixa.trocoInicial) +
                 " + vendas " + r(vendasDinheiro) +
                 (receb > 0.004 ? " + recebimentos " + r(receb) : QString()) +
                 (resumoAtual.suprimentosPorForma.value("Dinheiro") > 0.004 ? " + suprimentos " + r(resumoAtual.suprimentosPorForma.value("Dinheiro")) : QString()) +
                 " − sangrias " + r(resumoAtual.sangriasPorForma.value("Dinheiro")) +
                 " = " + r(resumoAtual.esperadoPorForma.value("Dinheiro", 0.0)) + ".";
        for (auto it = resumoAtual.sangriasPorForma.constBegin(); it != resumoAtual.sangriasPorForma.constEnd(); ++it)
            if (it.key() != "Dinheiro" && it.value() > 0.004)
                aviso += "\nSangria em " + it.key() + ": − " + r(it.value()) + " (já descontada do esperado em " + it.key() + ").";
    }
    ui->Lbl_Tolerancia->setText(aviso);
    ui->Lbl_Tolerancia->setWordWrap(true);

    auto aplicarEsperado = [&](QLabel *lbl, QLineEdit *edit, const QString &forma) {
        const double esp = resumoAtual.esperadoPorForma.value(forma, 0.0);
        lbl->setText(portugues.toString(esp, 'f', 2));
        // campo vazio: o operador precisa contar e digitar (preencher com o esperado anularia a conferência)
        edit->setPlaceholderText("valor contado");
        QDoubleValidator *val = new QDoubleValidator(0.0, 99999999.99, 2, this);
        val->setLocale(portugues);
        edit->setValidator(val);
        connect(edit, &QLineEdit::textChanged, this, &FechamentoCaixa::recalcularDiferencas);
    };
    aplicarEsperado(ui->Lbl_EspDinheiro, ui->Ledit_Dinheiro, "Dinheiro");
    aplicarEsperado(ui->Lbl_EspCredito, ui->Ledit_Credito, "Crédito");
    aplicarEsperado(ui->Lbl_EspDebito, ui->Ledit_Debito, "Débito");
    aplicarEsperado(ui->Lbl_EspPix, ui->Ledit_Pix, "Pix");
    // quanto do dinheiro contado fica na gaveta para o próximo caixa; o resto é levado para depósito
    {
        auto *linha = new QHBoxLayout;
        auto *rotulo = new QLabel("Dinheiro que fica na gaveta (troco do próximo caixa):", this);
        ledFica = new QLineEdit(this);
        ledFica->setPlaceholderText("0,00");
        ledFica->setMaximumWidth(120);
        auto *val = new QDoubleValidator(0.0, 99999999.99, 2, this);
        val->setLocale(portugues);
        ledFica->setValidator(val);
        lblDeposito = new QLabel(this);
        lblDeposito->setStyleSheet("color: rgb(21, 128, 61); font-weight: 700;");
        linha->addWidget(rotulo);
        linha->addWidget(ledFica);
        linha->addWidget(lblDeposito, 1);
        const int pos = ui->verticalLayout->indexOf(ui->hlPin);
        ui->verticalLayout->insertLayout(pos >= 0 ? pos : ui->verticalLayout->count(), linha);
        connect(ledFica, &QLineEdit::textChanged, this, &FechamentoCaixa::atualizarDeposito);
        connect(ui->Ledit_Dinheiro, &QLineEdit::textChanged, this, &FechamentoCaixa::atualizarDeposito);
        atualizarDeposito();
    }
    recalcularDiferencas();
    ui->Ledit_Dinheiro->setFocus();
}

FechamentoCaixa::~FechamentoCaixa()
{
    delete ui;
}

QMap<QString, double> FechamentoCaixa::valoresInformados() const
{
    QMap<QString, double> m;
    m["Dinheiro"] = portugues.toDouble(ui->Ledit_Dinheiro->text());
    m["Crédito"] = portugues.toDouble(ui->Ledit_Credito->text());
    m["Débito"] = portugues.toDouble(ui->Ledit_Debito->text());
    m["Pix"] = portugues.toDouble(ui->Ledit_Pix->text());
    return m;
}

void FechamentoCaixa::recalcularDiferencas()
{
    auto pintar = [&](QLabel *lbl, const QString &forma, const QString &texto) {
        bool ok = false;
        const double informado = portugues.toDouble(texto, &ok);
        if (texto.trimmed().isEmpty() || !ok) {
            lbl->setText("—");
            lbl->setStyleSheet("color: rgb(100, 116, 139);");
            return;
        }
        const double esperado = resumoAtual.esperadoPorForma.value(forma, 0.0);
        const double dif = ok ? informado - esperado : 0;
        lbl->setText(portugues.toString(dif, 'f', 2));
        const bool fora = ok && !caixaServ.dentroDaTolerancia(dif, esperado);
        lbl->setStyleSheet(std::abs(dif) < 0.005
                               ? "color: rgb(22, 101, 52);"
                               : (fora ? "color: rgb(185, 28, 28); font-weight: 700;"
                                       : "color: rgb(161, 98, 7);"));
    };
    pintar(ui->Lbl_DifDinheiro, "Dinheiro", ui->Ledit_Dinheiro->text());
    pintar(ui->Lbl_DifCredito, "Crédito", ui->Ledit_Credito->text());
    pintar(ui->Lbl_DifDebito, "Débito", ui->Ledit_Debito->text());
    pintar(ui->Lbl_DifPix, "Pix", ui->Ledit_Pix->text());
}

double FechamentoCaixa::dinheiroQueFica() const
{
    bool ok = false;
    const double v = portugues.toDouble(ledFica ? ledFica->text() : QString(), &ok);
    return ok ? v : 0.0;
}

void FechamentoCaixa::atualizarDeposito()
{
    if (!lblDeposito)
        return;
    bool ok = false;
    const double contado = portugues.toDouble(ui->Ledit_Dinheiro->text(), &ok);
    if (!ok) {
        lblDeposito->clear();
        return;
    }
    const double levar = contado - dinheiroQueFica();
    lblDeposito->setText(levar < -0.005 ? QStringLiteral("Maior que o dinheiro contado.")
                                        : QStringLiteral("Levar para depósito: R$ ") + portugues.toString(levar, 'f', 2));
}

void FechamentoCaixa::on_Btn_Fechar_clicked()
{
    if (idCaixa <= 0)
        return;

    bool okD = false, okC = false, okB = false, okP = false;
    portugues.toDouble(ui->Ledit_Dinheiro->text(), &okD);
    portugues.toDouble(ui->Ledit_Credito->text(), &okC);
    portugues.toDouble(ui->Ledit_Debito->text(), &okB);
    portugues.toDouble(ui->Ledit_Pix->text(), &okP);
    if (!okD || !okC || !okB || !okP) {
        QMessageBox::warning(this, "Fechar caixa", "Informe o valor contado em todas as formas de pagamento.");
        return;
    }

    const auto confirma = QMessageBox::question(
        this, "Fechar caixa",
        "Confirmar o fechamento do caixa com os valores contados?\nDepois de fechado ele não pode ser reaberto.",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (confirma != QMessageBox::Yes)
        return;

    const auto r = caixaServ.fecharCaixa(idCaixa, ui->Ledit_Pin->text(), valoresInformados(),
                                         ui->PTEdit_Obs->toPlainText(), dinheiroQueFica());
    if (!r.ok) {
        QMessageBox::warning(this, "Fechar caixa", r.msg);
        ui->Ledit_Pin->clear();
        ui->Ledit_Pin->setFocus();
        return;
    }

    fechouOk = true;
    RelatorioCaixaJanela rel(idCaixa, this);
    rel.exec();
    accept();
}

void FechamentoCaixa::on_Btn_Cancelar_clicked()
{
    reject();
}
