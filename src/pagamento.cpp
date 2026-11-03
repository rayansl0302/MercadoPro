#include "pagamento.h"
#include "ui_pagamento.h"
#include "seletormaquinacartao.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QPainter>
#include <QPrinter>
#include <QPrintDialog>
#include <QSqlQuery>
#include <QDoubleValidator>

pagamento::pagamento(QString total, QString cliente, QString data, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::pagamento)
{
    ui->setupUi(this);

    configDTO = confServ.carregarTudo();
    totalGlobal = total;
    ui->Lbl_ResumoTotal->setText(total);
    clienteGlobal = cliente;
    ui->Lbl_ResumoCliente->setText(cliente);
    dataGlobal = data;
    ui->Lbl_ResumoData->setText(data);

    ui->Ledit_Recebido->setFocus();

    // esconder os campos nao relativos a forma dinheiro (taxa)
    ui->label_8->hide();
    ui->Ledit_Taxa->hide();

    ui->FrameNF->setVisible(false);
    ui->Ledit_NNF->setVisible(false);
    ui->Lbl_NNF->setVisible(false);


    // valores padrao
    ui->Ledit_Recebido->setText(totalGlobal);
    ui->Lbl_Troco->setText("0");
    ui->Ledit_Desconto->setText("0");
    ui->Ledit_Taxa->setText("0");
    ui->Lbl_TotalTaxa->setText(totalGlobal);

    // validador
    QDoubleValidator *validador = new QDoubleValidator(0.0, 9999.99, 2);
    ui->Ledit_Taxa->setValidator(validador);

    // combo da maquininha na mesma linha da taxa (a linha é posicionada à mão: só é preciso alargá-la)
    auto *cbMaquina = new QComboBox(this);
    if (QWidget *linha = ui->label_8->parentWidget()) {
        if (auto *h = qobject_cast<QHBoxLayout *>(linha->layout())) {
            h->addWidget(cbMaquina);
            QRect g = linha->geometry();
            g.setWidth(qMax(g.width(), 440));
            linha->setGeometry(g);
        }
    }
    ui->label_8->setText(QStringLiteral("Taxa da máquina (%):"));
    ui->label_8->setToolTip(QStringLiteral("Custo da loja: não é somada ao valor, o cliente paga o preço da gôndola."));
    seletorMaquina = new SeletorMaquinaCartao(cbMaquina, nullptr, this);
    ui->Ledit_Taxa->setReadOnly(seletorMaquina->temMaquinas());
    connect(seletorMaquina, &SeletorMaquinaCartao::escolhaMudou, this, [this]() {
        const int forma = ui->CBox_FormaPagamento->currentIndex();
        if (forma == 2)
            mostrarTaxaMaquina("Crédito", configDTO.taxaCreditoFinanceiro);
        else if (forma == 3)
            mostrarTaxaMaquina("Débito", configDTO.taxaDebitoFinanceiro);
    });
    ui->Ledit_Recebido->setValidator(validador);
    ui->Ledit_Desconto->setValidator(validador);
    QIntValidator *validadorInt = new QIntValidator();
    ui->Ledit_NNF->setValidator(validadorInt);
}

pagamento::~pagamento()
{
    delete ui;
}

void pagamento::on_buttonBox_accepted()
{
    terminarPagamento();
}


void pagamento::on_Ledit_Recebido_textChanged(const QString &arg1)
{
    QString dinRecebido = ui->Ledit_Recebido->text();
    QString valorFinal = ui->Lbl_TotalTaxa->text();
    float troco = portugues.toFloat(dinRecebido) - portugues.toFloat(valorFinal);

    ui->Lbl_Troco->setText(portugues.toString(troco, 'f', 2));

}


void pagamento::on_CBox_FormaPagamento_activated(int index)
{
    // mostrar ou esconder campos relacionados ao troco
    // a depender da forma dinheiro ser selecionada

    seletorMaquina->mostrar(index == 2 || index == 3);

    switch (index) {
    case 0:
        // dinheiro
        ui->Lbl_Troco->show();
        ui->label_2->show();
        ui->label_3->show();
        ui->Ledit_Recebido->show();
        ui->label_8->hide();
        ui->Ledit_Taxa->hide();

        // valores padrao
        ui->Ledit_Recebido->setText(totalGlobal);
        ui->Lbl_Troco->setText("0");
        ui->Ledit_Desconto->setText("0");
        ui->Ledit_Taxa->setText("0");
        ui->Lbl_TotalTaxa->setText(totalGlobal);
        break;
    case 2:
        // credito
        ui->Lbl_Troco->hide();
        ui->label_2->hide();
        ui->label_3->hide();
        ui->Ledit_Recebido->hide();
        ui->label_8->show();
        ui->Ledit_Taxa->show();

        // valores padrao
        ui->Ledit_Recebido->setText(totalGlobal);
        ui->Lbl_Troco->setText("0");
        ui->Ledit_Desconto->setText("0");
        mostrarTaxaMaquina("Crédito", configDTO.taxaCreditoFinanceiro);
        ui->Lbl_TotalTaxa->setText(portugues.toString(obterValorFinal("0", "0"), 'f', 2));
        break;
    case 3:
        // debito
        ui->Lbl_Troco->hide();
        ui->label_2->hide();
        ui->label_3->hide();
        ui->Ledit_Recebido->hide();
        ui->label_8->show();
        ui->Ledit_Taxa->show();

        // valores padrao
        ui->Ledit_Recebido->setText(totalGlobal);
        ui->Lbl_Troco->setText("0");
        ui->Ledit_Desconto->setText("0");
        mostrarTaxaMaquina("Débito", configDTO.taxaDebitoFinanceiro);
        ui->Lbl_TotalTaxa->setText(portugues.toString(obterValorFinal("0", "0"), 'f', 2));
        break;
    default:
        ui->Lbl_Troco->hide();
        ui->label_2->hide();
        ui->label_3->hide();
        ui->Ledit_Recebido->hide();
        ui->label_8->hide();
        ui->Ledit_Taxa->hide();

        // valores padrao
        ui->Ledit_Recebido->setText(totalGlobal);
        ui->Lbl_Troco->setText("0");
        ui->Ledit_Desconto->setText("0");
        ui->Ledit_Taxa->setText("0");
        ui->Lbl_TotalTaxa->setText(totalGlobal);
        break;
    }
}


void pagamento::on_Ledit_Taxa_textChanged(const QString &arg1)
{
    descontoTaxa();
}

// A taxa do cartão NÃO entra no valor: o cliente paga o preço de gôndola (total - desconto).
// O parâmetro fica só por compatibilidade; a taxa é gravada como custo da loja.
float pagamento::obterValorFinal(QString /*taxa*/, QString desconto){
    return portugues.toFloat(totalGlobal) - portugues.toFloat(desconto);
}

void pagamento::mostrarTaxaMaquina(const QString &forma, double taxaPadrao)
{
    seletorMaquina->mostrar(true);
    ui->Ledit_Taxa->setText(portugues.toString(seletorMaquina->taxaDaForma(forma, taxaPadrao), 'f', 2));
}

void pagamento::on_Ledit_Desconto_textChanged(const QString &arg1)
{
    descontoTaxa();
}

void pagamento::descontoTaxa(){
    QString novaTaxa = ui->Ledit_Taxa->text();
    QString descontoInicial = ui->Ledit_Desconto->text();
    QString desconto;
    if (ui->CheckPorcentagem->isChecked()){
        desconto = portugues.toString((portugues.toFloat(descontoInicial)/100)*portugues.toFloat(totalGlobal));
    }
    else{
        desconto = descontoInicial;
    }
    QString valorFinal = portugues.toString(obterValorFinal(novaTaxa, desconto), 'f', 2);
    ui->Lbl_TotalTaxa->setText(valorFinal);
    // o valor final influencia os campos recebido, portanto modificacoes nele devem afetar
    // os campos influenciados por ele

    // o valor recebido deve ser o mesmo que o valor final, pois ao alterar o valor final
    // o dinheiro recebido não será mais o mesmo do valor final anterior
    ui->Ledit_Recebido->setText(valorFinal);

    // o troco será zero portanto
    ui->Lbl_Troco->setText("0");
}

void pagamento::terminarPagamento()
{

}

void pagamento::on_CheckPorcentagem_stateChanged(int arg1)
{
    descontoTaxa();
}


void pagamento::on_CBox_ModeloEmit_currentIndexChanged(int index)
{
    if(index == 2){
        ui->RadioBtn_EmitNfApenas->setVisible(false);
        ui->RadioBtn_EmitNfTodos->setVisible(false);
    }else{
        ui->RadioBtn_EmitNfApenas->setVisible(true);
        ui->RadioBtn_EmitNfTodos->setVisible(true);
    }
}

