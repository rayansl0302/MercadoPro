#include "movimentacaocaixa.h"
#include "ui_movimentacaocaixa.h"
#include <QMessageBox>
#include <QDoubleValidator>
#include <QComboBox>
#include <QVBoxLayout>
#include <QLabel>

MovimentacaoCaixa::MovimentacaoCaixa(Tipo tipo, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::MovimentacaoCaixa)
    , portugues(QLocale::Portuguese, QLocale::Brazil)
{
    ui->setupUi(this);
    setWindowModality(Qt::ApplicationModal);

    if (tipo == Tipo::Suprimento)
        ui->Rb_Suprimento->setChecked(true);
    else
        ui->Rb_Sangria->setChecked(true);

    const CaixaDTO caixa = caixaServ.caixaAtual();
    if (caixa.aberto()) {
        ui->Lbl_Caixa->setText(QString("Caixa #%1 · %2 · terminal %3")
                                   .arg(caixa.id).arg(caixa.nomeOperador, caixa.terminal));
    } else {
        ui->Lbl_Caixa->setText("Nenhum caixa aberto neste terminal.");
        ui->Btn_Confirmar->setEnabled(false);
    }

    QDoubleValidator *validador = new QDoubleValidator(0.01, 99999999.99, 2, this);
    validador->setLocale(portugues);
    ui->Ledit_Valor->setValidator(validador);

    // de onde saiu (sangria) ou onde entrou (suprimento): a diferença do caixa vem da forma errada
    cbForma = new QComboBox(this);
    for (const QString &f : {QStringLiteral("Dinheiro"), QStringLiteral("Crédito"), QStringLiteral("Débito"), QStringLiteral("Pix")})
        cbForma->addItem(f);
    if (auto *lay = qobject_cast<QVBoxLayout *>(layout())) {
        const int pos = lay->indexOf(ui->Ledit_Valor);
        lay->insertWidget(pos + 1, new QLabel("Forma (de onde foi retirado / onde entrou):", this));
        lay->insertWidget(pos + 2, cbForma);
    }
}

MovimentacaoCaixa::~MovimentacaoCaixa()
{
    delete ui;
}

void MovimentacaoCaixa::on_Btn_Confirmar_clicked()
{
    bool ok = false;
    const double valor = portugues.toDouble(ui->Ledit_Valor->text(), &ok);
    if (!ok || valor <= 0) {
        QMessageBox::warning(this, "Caixa", "Informe um valor maior que zero.");
        ui->Ledit_Valor->setFocus();
        return;
    }

    const QString motivo = ui->PTEdit_Motivo->toPlainText().trimmed();
    const auto r = ui->Rb_Suprimento->isChecked()
                       ? caixaServ.registrarSuprimento(valor, motivo, cbForma->currentText())
                       : caixaServ.registrarSangria(valor, motivo, cbForma->currentText());
    if (!r.ok) {
        QMessageBox::warning(this, "Caixa", r.msg);
        return;
    }
    QMessageBox::information(this, "Caixa", r.msg);
    accept();
}

void MovimentacaoCaixa::on_Btn_Cancelar_clicked()
{
    reject();
}
