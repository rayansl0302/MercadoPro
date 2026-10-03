#include "empresadialog.h"
#include "services/empresa_service.h"
#include "util/icones.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int kColunas = 2;

// Janela para cadastrar ou editar a empresa: nome curto, CNPJ e razão social (o resto é em Configurações)
class NovaEmpresaDialog : public QDialog
{
public:
    explicit NovaEmpresaDialog(QWidget *parent, const EmpresaDTO &atual = EmpresaDTO())
        : QDialog(parent)
    {
        const bool edicao = atual.valida();
        setWindowTitle(edicao ? "Editar empresa" : "Cadastrar empresa");
        setModal(true);
        auto *form = new QFormLayout;
        nome = new QLineEdit(atual.apelido);
        nome->setPlaceholderText("Ex.: Salão da Ana");
        cnpj = new QLineEdit;
        cnpj->setPlaceholderText("00.000.000/0000-00");
        cnpj->setInputMask(QStringLiteral("99.999.999/9999-99;_"));
        cnpj->setText(atual.cnpj);
        razao = new QLineEdit(atual.razaoSocial);
        razao->setPlaceholderText("Nome que sai na nota (opcional)");
        form->addRow("Nome da empresa:", nome);
        form->addRow("CNPJ:", cnpj);
        form->addRow("Razão social:", razao);

        auto *dica = new QLabel(edicao ? "Corrigir o CNPJ aqui não altera notas já emitidas."
                                       : "Depois de cadastrar, escolha a empresa e preencha os dados completos e o "
                                         "certificado em Configurações.");
        dica->setWordWrap(true);
        dica->setStyleSheet("color: #5B6B7F;");

        auto *botoes = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        botoes->button(QDialogButtonBox::Ok)->setText(edicao ? "Salvar" : "Cadastrar");
        botoes->button(QDialogButtonBox::Cancel)->setText("Cancelar");
        connect(botoes, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(botoes, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto *layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addWidget(dica);
        layout->addWidget(botoes);
        setMinimumWidth(460);
    }
    QString nomeDigitado() const { return nome->text(); }
    QString cnpjDigitado() const { return cnpj->text(); }
    QString razaoDigitada() const { return razao->text(); }

private:
    QLineEdit *nome;
    QLineEdit *cnpj;
    QLineEdit *razao;
};
}

EmpresaDialog::EmpresaDialog(std::function<bool(const QString &)> gate, const QString &titulo, QWidget *parent)
    : QDialog(parent)
    , exigirGerente(std::move(gate))
{
    setWindowTitle("Empresa");
    setModal(true);
    resize(620, 460);

    auto *lblTitulo = new QLabel(titulo.isEmpty() ? QStringLiteral("Escolher empresa") : titulo);
    lblTitulo->setStyleSheet("font-size: 17pt; font-weight: 700; color: #142D45;");
    auto *lblInfo = new QLabel("As vendas, contas a pagar e notas feitas a partir de agora ficam no CNPJ escolhido, "
                               "até você trocar de empresa.");
    lblInfo->setWordWrap(true);
    lblInfo->setStyleSheet("color: #5B6B7F;");

    auto *rolagem = new QScrollArea;
    rolagem->setWidgetResizable(true);
    rolagem->setFrameShape(QFrame::NoFrame);
    rolagem->setStyleSheet("QScrollArea { background: transparent; }");
    areaCartoes = new QWidget;
    areaCartoes->setStyleSheet("background: transparent;");
    grade = new QGridLayout(areaCartoes);
    grade->setContentsMargins(2, 2, 2, 2);
    grade->setSpacing(10);
    rolagem->setWidget(areaCartoes);

    lblAviso = new QLabel;
    lblAviso->setWordWrap(true);
    lblAviso->setStyleSheet("color: #B91C1C;");

    auto *btnNova = new QPushButton("Cadastrar empresa...");
    btnNova->setIcon(Icones::icone("plus", QColor(Qt::white), 18));
    btnEditar = new QPushButton("Editar...");
    btnEditar->setIcon(Icones::icone("pencil", QColor("#142D45"), 18));
    btnEditar->setStyleSheet("QPushButton { background: white; color: #142D45; border: 1px solid #C9D3DF; }"
                             "QPushButton:hover { background: #EAF6EC; border-color: #17963A; }"
                             "QPushButton:disabled { background: #F1F5F9; color: #94A3B8; border-color: #E2E8F0; }");
    btnDesativar = new QPushButton("Desativar");
    btnDesativar->setStyleSheet("QPushButton { background: white; color: #B91C1C; border: 1px solid #FCA5A5; }"
                                "QPushButton:hover { background: #FEF2F2; }"
                                "QPushButton:disabled { background: #F1F5F9; color: #94A3B8; border-color: #E2E8F0; }");
    auto *btnFechar = new QPushButton("Fechar");
    btnFechar->setStyleSheet("QPushButton { background: white; color: #142D45; border: 1px solid #C9D3DF; }"
                             "QPushButton:hover { background: #EAF6EC; }");
    btnUsar = new QPushButton("Usar esta empresa");
    btnUsar->setDefault(true);
    btnUsar->setIcon(Icones::icone("check", QColor(Qt::white), 18));

    connect(btnNova, &QPushButton::clicked, this, &EmpresaDialog::cadastrarNova);
    connect(btnEditar, &QPushButton::clicked, this, &EmpresaDialog::editarSelecionada);
    connect(btnDesativar, &QPushButton::clicked, this, &EmpresaDialog::desativarSelecionada);
    connect(btnFechar, &QPushButton::clicked, this, &QDialog::reject);
    connect(btnUsar, &QPushButton::clicked, this, &EmpresaDialog::usarSelecionada);

    auto *rodape = new QHBoxLayout;
    rodape->addWidget(btnNova);
    rodape->addWidget(btnEditar);
    rodape->addWidget(btnDesativar);
    rodape->addStretch(1);
    rodape->addWidget(btnFechar);
    rodape->addWidget(btnUsar);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 18, 22, 18);
    layout->addWidget(lblTitulo);
    layout->addWidget(lblInfo);
    layout->addWidget(rolagem, 1);
    chInativas = new QCheckBox("Mostrar empresas desativadas");
    connect(chInativas, &QCheckBox::toggled, this, [this]() { recarregar(); });
    layout->addWidget(chInativas);
    layout->addWidget(lblAviso);
    layout->addLayout(rodape);

    idSelecionada = Empresa_service::instancia()->idAtiva();
    recarregar();
}

void EmpresaDialog::recarregar()
{
    for (QToolButton *c : std::as_const(cartoes))
        c->deleteLater();
    cartoes.clear();

    auto *es = Empresa_service::instancia();
    empresas = es->listar(!(chInativas && chInativas->isChecked()));
    const qlonglong emUso = es->idAtiva();

    int i = 0;
    for (const EmpresaDTO &e : std::as_const(empresas)) {
        auto *botao = new QToolButton;
        botao->setCheckable(true);
        botao->setAutoExclusive(true);
        botao->setCursor(Qt::PointingHandCursor);
        botao->setMinimumSize(250, 96);
        botao->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        botao->setIconSize(QSize(34, 34));
        botao->setIcon(Icones::icone("building-2", QColor("#17963A"), 34));
        const QString cnpj = e.cnpj.isEmpty() ? QStringLiteral("CNPJ não informado") : e.cnpjFormatado();
        botao->setText(QStringLiteral("%1\n%2%3").arg(e.apelido, cnpj,
                                                       !e.ativa ? QStringLiteral("\nDesativada")
                                                       : e.id == emUso ? QStringLiteral("\nEm uso agora") : QString()));
        if (!e.ativa)
            botao->setIcon(Icones::icone("building-2", QColor("#94A3B8"), 34));
        botao->setStyleSheet(
            "QToolButton { background: white; color: #142D45; border: 1px solid #D5DEE9; border-radius: 12px;"
            " font-weight: 700; padding: 8px 12px; text-align: left; }"
            "QToolButton:hover { background: #EAF6EC; border: 2px solid #17963A; }"
            "QToolButton:checked { background: #D4F0DA; border: 2px solid #17963A; color: #142D45; }");
        const qlonglong id = e.id;
        connect(botao, &QToolButton::clicked, this, [this, id]() { selecionar(id); });
        cartoes.append(botao);
        grade->addWidget(botao, i / kColunas, i % kColunas);
        ++i;
    }
    grade->setRowStretch(grade->rowCount(), 1);

    if (idSelecionada <= 0 || !es->getPorId(idSelecionada).valida())
        idSelecionada = emUso;
    selecionar(idSelecionada);
}

void EmpresaDialog::selecionar(qlonglong id)
{
    idSelecionada = id;
    for (int i = 0; i < empresas.size() && i < cartoes.size(); ++i)
        cartoes.at(i)->setChecked(empresas.at(i).id == id);
    const bool emUso = id == Empresa_service::instancia()->idAtiva();
    const EmpresaDTO sel = Empresa_service::instancia()->getPorId(id);
    btnUsar->setText(emUso ? QStringLiteral("Continuar nesta empresa") : QStringLiteral("Usar esta empresa"));
    btnUsar->setEnabled(sel.valida() && sel.ativa);
    btnEditar->setEnabled(sel.valida());
    // empresa desativada: o botão vira "Reativar"
    btnDesativar->setText(sel.valida() && !sel.ativa ? QStringLiteral("Reativar") : QStringLiteral("Desativar"));
    btnDesativar->setEnabled(sel.valida() && (!sel.ativa || (!emUso && empresas.size() > 1)));
    lblAviso->clear();
}

void EmpresaDialog::usarSelecionada()
{
    if (idSelecionada <= 0) {
        lblAviso->setText("Selecione uma empresa.");
        return;
    }
    const auto r = Empresa_service::instancia()->trocarPara(idSelecionada);
    if (!r.ok) {
        lblAviso->setText(r.msg);
        return;
    }
    idEscolhida = idSelecionada;
    accept();
}

void EmpresaDialog::cadastrarNova()
{
    if (exigirGerente && !exigirGerente(QStringLiteral("O cadastro de empresas")))
        return;
    NovaEmpresaDialog dlg(this);
    while (dlg.exec() == QDialog::Accepted) {
        const auto r = Empresa_service::instancia()->cadastrar(dlg.nomeDigitado(), dlg.cnpjDigitado(), dlg.razaoDigitada());
        if (r.ok) {
            idSelecionada = r.id;
            recarregar();
            QMessageBox::information(this, "Empresa cadastrada",
                "Empresa cadastrada.\n\nEscolha-a e use \"Usar esta empresa\"; depois preencha os dados completos "
                "e o certificado em Configurações.");
            return;
        }
        QMessageBox::warning(this, "Cadastrar empresa", r.msg);
    }
}

void EmpresaDialog::desativarSelecionada()
{
    const EmpresaDTO e = Empresa_service::instancia()->getPorId(idSelecionada);
    if (!e.valida())
        return;
    if (exigirGerente && !exigirGerente(e.ativa ? QStringLiteral("Desativar empresa") : QStringLiteral("Reativar empresa")))
        return;
    if (!e.ativa) {
        const auto r = Empresa_service::instancia()->definirAtivaNoCadastro(e.id, true);
        if (!r.ok)
            lblAviso->setText(r.msg);
        recarregar();
        return;
    }
    const auto resp = QMessageBox::question(this, "Desativar empresa",
        QStringLiteral("Desativar \"%1\"?\nAs vendas e contas antigas continuam no histórico; ela só deixa de "
                       "aparecer para escolha (dá para reativar depois).").arg(e.apelido),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (resp != QMessageBox::Yes)
        return;
    const auto r = Empresa_service::instancia()->definirAtivaNoCadastro(e.id, false);
    if (!r.ok) {
        lblAviso->setText(r.msg);
        return;
    }
    idSelecionada = Empresa_service::instancia()->idAtiva();
    recarregar();
}

void EmpresaDialog::editarSelecionada()
{
    auto *es = Empresa_service::instancia();
    const EmpresaDTO e = es->getPorId(idSelecionada);
    if (!e.valida())
        return;
    if (exigirGerente && !exigirGerente(QStringLiteral("Editar empresa")))
        return;
    NovaEmpresaDialog dlg(this, e);
    while (dlg.exec() == QDialog::Accepted) {
        // trocar o CNPJ de empresa que já vendeu: avisa antes (notas emitidas não mudam)
        const QString novoCnpj = Empresa_service::somenteDigitos(dlg.cnpjDigitado());
        if (!e.cnpj.isEmpty() && novoCnpj != e.cnpj && es->quantidadeDeVendas(e.id) > 0) {
            const auto resp = QMessageBox::question(this, "Alterar CNPJ",
                QStringLiteral("Esta empresa já tem %1 venda(s). Alterar o CNPJ não muda as notas já emitidas e as "
                               "próximas notas usarão o novo CNPJ (e o certificado dele).\n\nConfirmar a alteração?")
                    .arg(es->quantidadeDeVendas(e.id)),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (resp != QMessageBox::Yes)
                continue;
        }
        const auto r = es->atualizar(e.id, dlg.nomeDigitado(), dlg.cnpjDigitado(), dlg.razaoDigitada());
        if (r.ok) {
            recarregar();
            return;
        }
        QMessageBox::warning(this, "Editar empresa", r.msg);
    }
}
