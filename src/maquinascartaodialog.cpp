#include "maquinascartaodialog.h"
#include "services/maquinacartao_service.h"

#include <QCheckBox>
#include <QDate>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
const QLocale ptBR(QLocale::Portuguese, QLocale::Brazil);
QString percentual(double v) { return ptBR.toString(v, 'f', 2) + "%"; }
QString dinheiro(double v) { return ptBR.toCurrencyString(v, "R$ "); }

const char *kEstiloSecundario = "QPushButton { background: white; color: #142D45; border: 1px solid #C9D3DF; }"
                                "QPushButton:hover { background: #EAF6EC; }";
}

EditarMaquinaDialog::EditarMaquinaDialog(qlonglong idMaquina, QWidget *parent)
    : QDialog(parent)
    , id_(idMaquina)
{
    setWindowTitle(idMaquina > 0 ? "Alterar maquininha" : "Nova maquininha");
    setModal(true);
    setMinimumWidth(440);

    MaquinaCartaoDTO m;
    if (idMaquina > 0)
        m = MaquinaCartao_service().get(idMaquina);

    nome_ = new QLineEdit(m.nome);
    nome_->setPlaceholderText("Ex.: Stone, Cielo, PagBank...");
    nome_->setMaxLength(60);
    auto novaTaxa = [&](double valor) {
        auto *s = new QDoubleSpinBox;
        s->setRange(0.0, 100.0);
        s->setDecimals(2);
        s->setSingleStep(0.1);
        s->setSuffix(" %");
        s->setLocale(ptBR);
        s->setValue(valor);
        return s;
    };
    debito_ = novaTaxa(m.taxaDebito);
    credito_ = novaTaxa(m.taxaCredito);

    auto *aviso = new QLabel("A taxa é custo da loja: o cliente paga sempre o preço da gôndola, "
                             "e a taxa da maquininha escolhida na venda é descontada do lucro.");
    aviso->setWordWrap(true);
    aviso->setStyleSheet("color: #475569;");

    auto *form = new QFormLayout;
    form->addRow("Nome", nome_);
    form->addRow("Taxa no débito", debito_);
    form->addRow("Taxa no crédito (à vista)", credito_);

    auto *btnSalvar = new QPushButton("Salvar");
    btnSalvar->setDefault(true);
    auto *btnCancelar = new QPushButton("Cancelar");
    btnCancelar->setStyleSheet(kEstiloSecundario);
    connect(btnSalvar, &QPushButton::clicked, this, &EditarMaquinaDialog::salvar);
    connect(btnCancelar, &QPushButton::clicked, this, &QDialog::reject);

    auto *botoes = new QHBoxLayout;
    botoes->addStretch(1);
    botoes->addWidget(btnSalvar);
    botoes->addWidget(btnCancelar);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(10);
    layout->addLayout(form);
    layout->addWidget(aviso);
    layout->addLayout(botoes);
}

void EditarMaquinaDialog::salvar()
{
    MaquinaCartao_service servico;
    MaquinaCartaoDTO m;
    if (id_ > 0)
        m = servico.get(id_);
    m.id = id_;
    m.nome = nome_->text();
    m.taxaDebito = debito_->value();
    m.taxaCredito = credito_->value();
    const auto r = servico.salvar(m);
    if (!r.ok) {
        QMessageBox::warning(this, "Maquininha", r.msg);
        return;
    }
    accept();
}

MaquinasCartaoDialog::MaquinasCartaoDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Maquininhas de cartão");
    setModal(true);
    resize(760, 520);

    auto *titulo = new QLabel("Maquininhas de cartão");
    titulo->setStyleSheet("font-size: 16pt; font-weight: 700; color: #142D45;");
    auto *explica = new QLabel("Cadastre cada maquininha com as taxas dela. Na venda no cartão você escolhe a máquina, "
                               "o cliente paga o preço da gôndola e a taxa entra como custo nos relatórios de lucro.");
    explica->setWordWrap(true);
    explica->setStyleSheet("color: #475569;");

    tabela_ = new QTableWidget(0, 4);
    tabela_->setHorizontalHeaderLabels({"Nome", "Débito", "Crédito", "Situação"});
    tabela_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tabela_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tabela_->setSelectionMode(QAbstractItemView::SingleSelection);
    tabela_->setAlternatingRowColors(true);
    tabela_->verticalHeader()->setVisible(false);
    tabela_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c : {1, 2, 3})
        tabela_->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    connect(tabela_, &QTableWidget::itemSelectionChanged, this, &MaquinasCartaoDialog::atualizarBotoes);
    connect(tabela_, &QTableWidget::cellDoubleClicked, this, [this]() { editar(); });

    mostrarInativas_ = new QCheckBox("Mostrar desativadas");
    connect(mostrarInativas_, &QCheckBox::toggled, this, &MaquinasCartaoDialog::carregar);

    auto *btnNova = new QPushButton("Nova maquininha");
    btnEditar_ = new QPushButton("Alterar");
    btnEditar_->setStyleSheet(kEstiloSecundario);
    btnAtiva_ = new QPushButton("Desativar");
    btnAtiva_->setStyleSheet(kEstiloSecundario);
    auto *btnFechar = new QPushButton("Fechar");
    btnFechar->setStyleSheet(kEstiloSecundario);
    connect(btnNova, &QPushButton::clicked, this, &MaquinasCartaoDialog::nova);
    connect(btnEditar_, &QPushButton::clicked, this, &MaquinasCartaoDialog::editar);
    connect(btnAtiva_, &QPushButton::clicked, this, &MaquinasCartaoDialog::alternarAtiva);
    connect(btnFechar, &QPushButton::clicked, this, &QDialog::accept);

    auto *botoes = new QHBoxLayout;
    botoes->addWidget(btnNova);
    botoes->addWidget(btnEditar_);
    botoes->addWidget(btnAtiva_);
    botoes->addStretch(1);
    botoes->addWidget(mostrarInativas_);
    botoes->addWidget(btnFechar);

    lblCustos_ = new QLabel;
    lblCustos_->setWordWrap(true);
    lblCustos_->setTextFormat(Qt::RichText);
    lblCustos_->setStyleSheet("color: #142D45; background: #F3F6F9; border-radius: 8px; padding: 8px 10px;");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(10);
    layout->addWidget(titulo);
    layout->addWidget(explica);
    layout->addWidget(tabela_, 1);
    layout->addWidget(lblCustos_);
    layout->addLayout(botoes);

    carregar();
}

qlonglong MaquinasCartaoDialog::idSelecionado() const
{
    const int linha = tabela_->currentRow();
    if (linha < 0 || !tabela_->item(linha, 0))
        return 0;
    return tabela_->item(linha, 0)->data(Qt::UserRole).toLongLong();
}

void MaquinasCartaoDialog::carregar()
{
    MaquinaCartao_service servico;
    const QList<MaquinaCartaoDTO> maquinas = servico.listar(mostrarInativas_->isChecked());
    tabela_->setRowCount(maquinas.size());
    for (int i = 0; i < maquinas.size(); ++i) {
        const MaquinaCartaoDTO &m = maquinas.at(i);
        auto celula = [&](int col, const QString &texto, Qt::Alignment a = Qt::AlignLeft | Qt::AlignVCenter) {
            auto *it = new QTableWidgetItem(texto);
            it->setTextAlignment(a);
            if (col == 0)
                it->setData(Qt::UserRole, m.id);
            if (!m.ativa)
                it->setForeground(QColor("#94A3B8"));
            tabela_->setItem(i, col, it);
        };
        celula(0, m.nome);
        celula(1, percentual(m.taxaDebito), Qt::AlignCenter);
        celula(2, percentual(m.taxaCredito), Qt::AlignCenter);
        celula(3, m.ativa ? "Ativa" : "Desativada", Qt::AlignCenter);
    }

    // quanto cada máquina custou de taxa neste mês (vendas no cartão e recebimentos de fiado no cartão)
    const QDate hoje = QDate::currentDate();
    const QList<CustoMaquinaDTO> custos = servico.custoPorMaquina(QDate(hoje.year(), hoje.month(), 1), hoje);
    if (custos.isEmpty()) {
        lblCustos_->setText("Taxas de cartão neste mês: nenhuma venda no cartão com maquininha ainda.");
    } else {
        double total = 0;
        QStringList linhas;
        for (const CustoMaquinaDTO &c : custos) {
            total += c.custoTaxas;
            linhas << QStringLiteral("%1: <b>%2</b> em taxas (%3 vendas, %4)")
                          .arg((c.nome.isEmpty() ? QStringLiteral("Taxa padrão (sem máquina)") : c.nome).toHtmlEscaped(),
                               dinheiro(c.custoTaxas)).arg(c.vendas).arg(dinheiro(c.valorVendido));
        }
        lblCustos_->setText(QStringLiteral("<b>Taxas de cartão neste mês: %1</b><br>%2").arg(dinheiro(total), linhas.join("<br>")));
    }
    atualizarBotoes();
}

void MaquinasCartaoDialog::atualizarBotoes()
{
    const bool tem = idSelecionado() > 0;
    btnEditar_->setEnabled(tem);
    btnAtiva_->setEnabled(tem);
    if (tem) {
        const bool ativa = tabela_->item(tabela_->currentRow(), 3)->text() == "Ativa";
        btnAtiva_->setText(ativa ? "Desativar" : "Reativar");
    }
}

void MaquinasCartaoDialog::nova()
{
    EditarMaquinaDialog dlg(0, this);
    if (dlg.exec() == QDialog::Accepted)
        carregar();
}

void MaquinasCartaoDialog::editar()
{
    const qlonglong id = idSelecionado();
    if (id <= 0)
        return;
    EditarMaquinaDialog dlg(id, this);
    if (dlg.exec() == QDialog::Accepted)
        carregar();
}

void MaquinasCartaoDialog::alternarAtiva()
{
    const qlonglong id = idSelecionado();
    if (id <= 0)
        return;
    MaquinaCartao_service servico;
    const bool ativa = tabela_->item(tabela_->currentRow(), 3)->text() == "Ativa";
    const auto r = servico.definirAtiva(id, !ativa);
    if (!r.ok) {
        QMessageBox::warning(this, "Maquininha", r.msg);
        return;
    }
    carregar();
}
