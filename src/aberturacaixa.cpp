#include "aberturacaixa.h"
#include "fechamentocaixa.h"
#include "ui_aberturacaixa.h"
#include "operadores.h"
#include "services/sessao_service.h"
#include <QMessageBox>
#include <QDoubleValidator>

AberturaCaixa::AberturaCaixa(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AberturaCaixa)
    , portugues(QLocale::Portuguese, QLocale::Brazil)
{
    ui->setupUi(this);
    setWindowModality(Qt::ApplicationModal);

    ui->Lbl_Terminal->setText("Terminal: " + Caixa_service::terminalAtual());

    QDoubleValidator *validador = new QDoubleValidator(0.0, 99999999.99, 2, this);
    validador->setLocale(portugues);
    ui->Ledit_Troco->setValidator(validador);

    trocoSugerido = caixaServ.sugerirTrocoInicial();
    ui->Ledit_Troco->setText(portugues.toString(trocoSugerido, 'f', 2));
    ui->Lbl_Sugerido->setText("Sugerido pelo último fechamento deste terminal: R$ " +
                              portugues.toString(trocoSugerido, 'f', 2) + " (pode alterar).");

    carregarOperadores();
}

AberturaCaixa::~AberturaCaixa()
{
    delete ui;
}

void AberturaCaixa::carregarOperadores()
{
    const qlonglong selecionado = ui->CBox_Operador->currentData().toLongLong();
    ui->CBox_Operador->clear();
    for (const OperadorDTO &op : operadorServ.listar(true))
        ui->CBox_Operador->addItem(op.nome, op.id);

    // quem está logado abre o próprio caixa: só a troca fica na mão do operador
    qlonglong inicial = selecionado;
    const qlonglong idSessao = Sessao_service::instancia()->idOperador();
    if (ui->CBox_Operador->findData(idSessao) >= 0)
        inicial = idSessao;

    const int idx = ui->CBox_Operador->findData(inicial);
    if (idx >= 0)
        ui->CBox_Operador->setCurrentIndex(idx);

    // quem já entrou no sistema não digita o PIN de novo: o caixa abre no nome dele
    Sessao_service *sessao = Sessao_service::instancia();
    const bool logado = sessao->ativa() && !sessao->pinGeral() && ui->CBox_Operador->findData(idSessao) >= 0;
    ui->CBox_Operador->setEnabled(!logado);
    ui->Ledit_Pin->setVisible(!logado);
    ui->label_2->setVisible(!logado);
    if (logado)
        ui->Ledit_Troco->setFocus();

    const bool temOperador = ui->CBox_Operador->count() > 0;
    ui->Btn_Abrir->setEnabled(temOperador);
    if (!temOperador)
        ui->Lbl_Sugerido->setText("Nenhum operador ativo cadastrado. Clique em \"Operadores...\" para cadastrar.");
}

void AberturaCaixa::on_Btn_Operadores_clicked()
{
    if (!Operadores::autenticarGerente(this))
        return;
    Operadores dlg(this);
    dlg.exec();
    carregarOperadores();
}

void AberturaCaixa::on_Btn_Abrir_clicked()
{
    bool ok = false;
    const double troco = portugues.toDouble(ui->Ledit_Troco->text(), &ok);
    if (!ok) {
        QMessageBox::warning(this, "Abrir caixa", "Informe um troco inicial válido.");
        ui->Ledit_Troco->setFocus();
        return;
    }

    const auto r = caixaServ.abrirCaixa(ui->CBox_Operador->currentData().toLongLong(),
                                        ui->Ledit_Pin->text(), troco, trocoSugerido);
    if (!r.ok) {
        QMessageBox::warning(this, "Abrir caixa", r.msg);
        ui->Ledit_Pin->clear();
        ui->Ledit_Pin->setFocus();
        return;
    }
    idCaixa = r.id;
    accept();
}

void AberturaCaixa::on_Btn_Cancelar_clicked()
{
    reject();
}

bool AberturaCaixa::garantirCaixaAberto(QWidget *parent)
{
    Caixa_service caixaServ;
    {
        const CaixaDTO atual = caixaServ.caixaAtual();
        QString motivoVencido;
        if (atual.aberto() && caixaServ.caixaVencido(atual, &motivoVencido)) {
            const auto resp = QMessageBox::warning(parent, "Caixa vencido", motivoVencido + "\n\nDeseja fechar o caixa agora?",
                                                   QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
            if (resp != QMessageBox::Yes)
                return false;
            FechamentoCaixa fechamento(parent);
            fechamento.exec();
            if (caixaServ.caixaAtual().aberto())
                return false;       // não fechou: segue sem caixa
        }
    }
    const auto r = caixaServ.exigirCaixaAberto();
    if (r.ok)
        return true;

    const auto resp = QMessageBox::question(parent, "Caixa", r.msg + "\n\nDeseja abrir o caixa agora?",
                                            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (resp != QMessageBox::Yes)
        return false;

    AberturaCaixa dlg(parent);
    if (dlg.exec() != QDialog::Accepted)
        return false;
    return caixaServ.exigirCaixaAberto().ok;
}
