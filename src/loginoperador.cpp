#include <QLineEdit>
#include <QGridLayout>
#include <QScrollArea>
#include <QToolButton>
#include "util/icones.h"
#include "loginoperador.h"
#include "ui_loginoperador.h"
#include "operadores.h"

SessaoDTO LoginOperador::executar(bool *cancelou, QWidget *parent)
{
    if (cancelou)
        *cancelou = false;

    Operador_service serv;
    const QList<OperadorDTO> ativos = serv.listar(true);
    const bool cadastroVazio = serv.listar(false).isEmpty();

    // primeira instalação: sem nenhum operador no cadastro o PIN do gerente é o que
    // destrava o cadastro. Se existe cadastro e o PIN geral não foi definido, isso é
    // inconsistência de instalação: não se cria o PIN aqui, senão qualquer pessoa
    // assumiria a gerência só porque todos os operadores estão desativados.
    if (cadastroVazio && !serv.gerentePinDefinido()) {
        if (!Operadores::autenticarGerente(parent)) {
            if (cancelou)
                *cancelou = true;
            return SessaoDTO();
        }
    }

    LoginOperador dialog(parent);
    dialog.carregarOperadores(ativos, ativos.isEmpty());
    if (ativos.isEmpty()) {
        dialog.setAviso(cadastroVazio
                           ? QStringLiteral("Nenhum operador cadastrado. Entre com o PIN do gerente "
                                            "e cadastre o primeiro operador.")
                           : QStringLiteral("Nenhum operador ativo. Fale com o gerente para reativar "
                                            "o cadastro ou entre com o PIN do gerente."));
    }
    if (dialog.exec() != QDialog::Accepted) {
        if (cancelou)
            *cancelou = true;
        return SessaoDTO();
    }
    return dialog.sessao();
}

LoginOperador::LoginOperador(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginOperador)
{
    ui->setupUi(this);
    connect(ui->Ledit_Pin, &QLineEdit::returnPressed, this, &LoginOperador::on_Btn_Entrar_clicked);

    // o combo continua guardando os dados, mas quem escolhe é a lista de cartões (mais fácil de tocar)
    ui->Lbl_Operador->hide();
    ui->Cmb_Operador->hide();
    ui->Ledit_Pin->setAlignment(Qt::AlignCenter);
    ui->Ledit_Pin->setPlaceholderText(QStringLiteral("PIN (4 a 6 dígitos)"));
    ui->Ledit_Pin->setStyleSheet(QStringLiteral("font-size: 16pt; letter-spacing: 6px;"));
    ui->Lbl_Pin->setText(QStringLiteral("PIN:"));
    ui->Ledit_Pin->setFocus();
}

LoginOperador::~LoginOperador()
{
    delete ui;
}

void LoginOperador::carregarOperadores(const QList<OperadorDTO> &operadores, bool somenteGerente)
{
    this->somenteGerente = somenteGerente;
    ui->Cmb_Operador->clear();

    // operadores do cadastro primeiro
    for (const OperadorDTO &op : operadores) {
        const QString rotulo = op.gerente ? QStringLiteral("%1  (gerente)").arg(op.nome) : op.nome;
        ui->Cmb_Operador->addItem(rotulo, op.id);
    }

    // a linha do PIN geral do gerente é sempre a última: sem identidade de operador,
    // serve tanto para o primeiro acesso quanto para quem não tem cadastro
    ui->Cmb_Operador->addItem(QStringLiteral("Entrar como gerente"), kOperadorGerenteId);
    ui->Cmb_Operador->setItemData(ui->Cmb_Operador->count() - 1,
                                  QStringLiteral("Usa o PIN geral do gerente"),
                                  Qt::ToolTipRole);

    // sem nenhum operador ativo não há como escolher outra coisa
    if (somenteGerente)
        ui->Cmb_Operador->setCurrentIndex(ui->Cmb_Operador->count() - 1);

    // monta a lista de cartões a partir do combo
    if (cartoes) {
        ui->vl_login->removeWidget(cartoes);
        cartoes->deleteLater();
        cartoes = nullptr;
    }
    botoesOperador.clear();

    auto *rolagem = new QScrollArea(this);
    rolagem->setWidgetResizable(true);
    rolagem->setFrameShape(QFrame::NoFrame);
    rolagem->setMinimumHeight(190);
    rolagem->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
    auto *interno = new QWidget;
    interno->setStyleSheet(QStringLiteral("background: transparent;"));
    auto *grade = new QGridLayout(interno);
    grade->setContentsMargins(2, 2, 2, 2);
    grade->setSpacing(10);
    constexpr int kColunas = 3;

    for (int i = 0; i < ui->Cmb_Operador->count(); ++i) {
        const bool ehGerenteGeral = ui->Cmb_Operador->itemData(i).toLongLong() == kOperadorGerenteId;
        const bool ehGerente = ehGerenteGeral || ui->Cmb_Operador->itemText(i).contains(QStringLiteral("(gerente)"));
        QString nome = ui->Cmb_Operador->itemText(i);
        nome.remove(QStringLiteral("  (gerente)"));

        auto *botao = new QToolButton;
        botao->setCheckable(true);
        botao->setAutoExclusive(true);
        botao->setCursor(Qt::PointingHandCursor);
        botao->setFixedSize(168, 88);
        botao->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        botao->setIconSize(QSize(30, 30));
        botao->setIcon(Icones::icone(ehGerenteGeral ? "shield-check" : "user", QColor("#17963A"), 30));
        botao->setText(ehGerente && !ehGerenteGeral ? nome + QStringLiteral("\nGerente") : nome);
        botao->setToolTip(ehGerenteGeral ? QStringLiteral("Usa o PIN geral do gerente") : nome);
        botao->setStyleSheet(QStringLiteral(
            "QToolButton { background: white; color: #142D45; border: 1px solid #D5DEE9; border-radius: 12px;"
            " font-weight: 700; padding: 6px; }"
            "QToolButton:hover { background: #EAF6EC; border: 2px solid #17963A; }"
            "QToolButton:checked { background: #D4F0DA; border: 2px solid #17963A; color: #142D45; }"
            "QToolButton:focus { border: 2px solid #17963A; }"));
        connect(botao, &QToolButton::clicked, this, [this, i]() { selecionarCartao(i); });
        botoesOperador.append(botao);
        grade->addWidget(botao, i / kColunas, i % kColunas);
    }
    grade->setRowStretch(grade->rowCount(), 1);
    grade->setColumnStretch(kColunas, 1);
    rolagem->setWidget(interno);
    cartoes = rolagem;
    // logo abaixo do texto de instrução
    ui->vl_login->insertWidget(2, rolagem);

    selecionarCartao(somenteGerente ? ui->Cmb_Operador->count() - 1 : 0);
    ui->Ledit_Pin->clear();
    ui->Lbl_Aviso->clear();
}

void LoginOperador::selecionarCartao(int indice)
{
    if (indice < 0 || indice >= ui->Cmb_Operador->count())
        return;
    ui->Cmb_Operador->setCurrentIndex(indice);
    if (indice < botoesOperador.size())
        botoesOperador.at(indice)->setChecked(true);
    ui->Ledit_Pin->setFocus();
}

void LoginOperador::setAviso(const QString &texto)
{
    ui->Lbl_Aviso->setText(texto);
}

SessaoDTO LoginOperador::sessao() const
{
    return sessaoAtual;
}

void LoginOperador::limparPin()
{
    ui->Ledit_Pin->clear();
    ui->Ledit_Pin->setFocus();
}

void LoginOperador::tentarEntrar()
{
    const QVariant dados = ui->Cmb_Operador->currentData();
    if (!dados.isValid()) {
        setAviso("Selecione o operador.");
        return;
    }

    const QString pin = ui->Ledit_Pin->text();
    if (!Operador_service::pinValido(pin)) {
        setAviso("O PIN deve ter de 4 a 6 dígitos numéricos.");
        limparPin();
        return;
    }

    const qlonglong id = dados.toLongLong();
    QString erro;
    const SessaoDTO sessao = operadorServ.validarLogin(id, pin, &erro);
    if (sessao.nomeOperador.isEmpty()) {
        setAviso(erro.isEmpty() ? QStringLiteral("PIN incorreto.") : erro);
        limparPin();
        return;
    }

    sessaoAtual = sessao;
    ui->Lbl_Aviso->clear();
    accept();
}

void LoginOperador::on_Btn_Entrar_clicked()
{
    tentarEntrar();
}

void LoginOperador::on_Btn_Sair_clicked()
{
    reject();
}
