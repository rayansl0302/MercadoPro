#include "contareceberdialogs.h"
#include "services/contasreceber_service.h"
#include "services/empresa_service.h"
#include "util/icones.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDateEdit>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QUrl>
#include <QUrlQuery>
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
    d->setMaximumDate(QDate::currentDate());      // nunca no futuro
    return d;
}

QLabel *faixa(const QString &corTexto, const QString &corFundo)
{
    auto *l = new QLabel;
    l->setWordWrap(true);
    l->setStyleSheet(QStringLiteral("background: %2; color: %1; border-radius: 8px; padding: 8px 10px;")
                         .arg(corTexto, corFundo));
    return l;
}
}

// ============================================================ fluxos prontos

bool lancarDividaComTela(QWidget *parent, const GateGerente &gate, qlonglong idClienteFixo)
{
    LancarDividaDialog dlg(idClienteFixo, parent);
    ContasReceber_service servico;
    while (dlg.exec() == QDialog::Accepted) {
        const DividaDTO d = dlg.divida();
        auto r = servico.lancarDivida(d);
        if (!r.ok && r.limiteExcedido) {
            const auto resp = QMessageBox::question(parent, "Limite de crédito",
                r.msg + "\n\nLiberar esta compra acima do limite? (precisa do PIN do gerente)",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (resp != QMessageBox::Yes)
                continue;
            if (gate && !gate(QStringLiteral("Liberar compra acima do limite")))
                continue;
            r = servico.lancarDivida(d, true);
        }
        if (r.ok) {
            QMessageBox::information(parent, "Dívida lançada",
                                     QStringLiteral("Dívida de %1 lançada.").arg(dinheiro(r.valor)));
            return true;
        }
        QMessageBox::warning(parent, "Lançar dívida", r.msg);
    }
    return false;
}

bool receberComTela(QWidget *parent, qlonglong idCliente, qlonglong idDivida)
{
    ReceberPagamentoDialog dlg(idCliente, idDivida, parent);
    ContasReceber_service servico;
    while (dlg.exec() == QDialog::Accepted) {
        const auto r = idDivida > 0
            ? servico.receberPagamento(idDivida, dlg.valor(), dlg.forma(), dlg.data(), dlg.observacao())
            : servico.receberDoCliente(idCliente, dlg.valor(), dlg.forma(), dlg.data(), dlg.observacao());
        if (r.ok) {
            QMessageBox::information(parent, "Pagamento", r.msg);
            return true;
        }
        QMessageBox::warning(parent, "Receber pagamento", r.msg);
    }
    return false;
}

bool editarCreditoComTela(QWidget *parent, const GateGerente &gate, qlonglong idCliente)
{
    CreditoClienteDialog dlg(idCliente, gate, parent);
    ContasReceber_service servico;
    const ClienteCreditoDTO antes = servico.getCredito(idCliente);
    bool mexeu = false;
    while (dlg.exec() == QDialog::Accepted) {
        const bool limiteMudou = antes.temLimite != dlg.temLimite() ||
                                 (dlg.temLimite() && qAbs(antes.limite - dlg.limite()) > 0.004);
        if (limiteMudou && gate && !gate(QStringLiteral("Alterar o limite de crédito")))
            continue;
        const auto r = servico.definirCredito(idCliente, dlg.temLimite(), dlg.limite(), dlg.observacao(),
                                              dlg.whatsapp());
        if (r.ok)
            return true;
        QMessageBox::warning(parent, "Dados de crédito", r.msg);
    }
    return mexeu || dlg.mudouAtivo();
}

void cobrarPorWhatsApp(QWidget *parent, qlonglong idCliente)
{
    ContasReceber_service servico;
    const ClienteCreditoDTO c = servico.getCredito(idCliente);
    QString numero;
    for (const QChar ch : (c.whatsapp.isEmpty() ? c.telefone : c.whatsapp))
        if (ch.isDigit())
            numero.append(ch);
    if (numero.size() < 10) {
        QMessageBox::information(parent, "Cobrança",
                                 "Este cliente não tem WhatsApp/telefone com DDD cadastrado.\n"
                                 "Informe em \"Dados de crédito\".");
        return;
    }
    if (numero.size() <= 11)
        numero.prepend("55");
    const double devido = servico.totalDevido(idCliente);
    const QString empresa = Empresa_service::instancia()->ativa().apelido;
    const QString texto = QStringLiteral("Olá, %1! Passando para lembrar que você tem %2 em aberto na %3. "
                                         "Qualquer dúvida, estamos à disposição.")
                              .arg(c.nome, dinheiro(devido), empresa);
    QUrl url(QStringLiteral("https://wa.me/%1").arg(numero));
    QUrlQuery q;
    q.addQueryItem("text", texto);
    url.setQuery(q);
    QDesktopServices::openUrl(url);        // abre o WhatsApp com a mensagem pronta; quem envia é a pessoa
}

// ============================================================ LancarDividaDialog

LancarDividaDialog::LancarDividaDialog(qlonglong idClienteFixo, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Lançar dívida");
    setModal(true);
    setMinimumWidth(520);

    ContasReceber_service servico;
    cbCliente = new QComboBox;
    cbCliente->setEditable(true);
    cbCliente->setInsertPolicy(QComboBox::NoInsert);
    FiltroDevedoresDTO todos;
    todos.modo = ModoDevedores::Todos;
    // clientes ativos (com ou sem dívida)
    ContasReceber_repository repo;
    for (const ClienteCreditoDTO &c : repo.listarClientes(false))
        cbCliente->addItem(c.nome, c.id);
    if (cbCliente->completer())
        cbCliente->completer()->setCaseSensitivity(Qt::CaseInsensitive);
    if (idClienteFixo > 0) {
        const int idx = cbCliente->findData(idClienteFixo);
        if (idx >= 0)
            cbCliente->setCurrentIndex(idx);
        cbCliente->setEnabled(false);
    } else {
        cbCliente->setCurrentIndex(-1);
        cbCliente->lineEdit()->setPlaceholderText("Escolha ou digite o nome do cliente");
    }

    dtData = campoData(QDate::currentDate());
    edDescricao = new QLineEdit;
    edDescricao->setPlaceholderText("O que o cliente levou (ex.: compras do mês, 2 sacos de arroz)");
    spValor = campoDinheiro();
    edObs = new QPlainTextEdit;
    edObs->setMaximumHeight(60);

    lblSituacao = faixa("#107A2E", "#EAF6EC");
    lblAviso = new QLabel;
    lblAviso->setWordWrap(true);
    lblAviso->setStyleSheet("color: #B91C1C;");

    auto *form = new QFormLayout;
    form->addRow("Cliente *", cbCliente);
    form->addRow("Data da compra *", dtData);
    form->addRow("Descrição *", edDescricao);
    form->addRow("Valor *", spValor);
    form->addRow("Observação", edObs);

    auto *botoes = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    botoes->button(QDialogButtonBox::Ok)->setText("Lançar dívida");
    botoes->button(QDialogButtonBox::Cancel)->setText("Cancelar");
    connect(botoes, &QDialogButtonBox::accepted, this, &LancarDividaDialog::confirmar);
    connect(botoes, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->addLayout(form);
    layout->addWidget(lblSituacao);
    layout->addWidget(lblAviso);
    layout->addWidget(botoes);

    connect(cbCliente, &QComboBox::currentIndexChanged, this, &LancarDividaDialog::atualizarSituacao);
    connect(cbCliente, &QComboBox::editTextChanged, this, &LancarDividaDialog::atualizarSituacao);
    connect(spValor, &QDoubleSpinBox::valueChanged, this, &LancarDividaDialog::atualizarSituacao);
    atualizarSituacao();
    (idClienteFixo > 0 ? static_cast<QWidget *>(edDescricao) : static_cast<QWidget *>(cbCliente))->setFocus();
}

DividaDTO LancarDividaDialog::divida() const
{
    DividaDTO d;
    const int idx = cbCliente->findText(cbCliente->currentText(), Qt::MatchFixedString);
    d.idCliente = idx >= 0 ? cbCliente->itemData(idx).toLongLong() : 0;
    d.dataCompra = dtData->date().toString(Qt::ISODate);
    d.descricao = edDescricao->text().trimmed();
    d.valorTotal = spValor->value();
    d.observacao = edObs->toPlainText().trimmed();
    return d;
}

void LancarDividaDialog::atualizarSituacao()
{
    const qlonglong id = divida().idCliente;
    if (id <= 0) {
        lblSituacao->setText("Escolha o cliente para ver o que ele já deve.");
        return;
    }
    ContasReceber_service servico;
    const auto s = servico.situacaoLimite(id, spValor->value());
    QString texto = QStringLiteral("Já deve <b>%1</b>.").arg(dinheiro(s.devido));
    bool excede = false;
    if (s.temLimite) {
        texto += QStringLiteral(" Limite <b>%1</b> · disponível <b>%2</b>.").arg(dinheiro(s.limite), dinheiro(s.disponivel));
        if (s.excede) {
            texto += QStringLiteral(" <b>Esta compra passa do limite em %1.</b>").arg(dinheiro(s.excedente));
            excede = true;
        }
    } else {
        texto += " Sem limite de crédito.";
    }
    lblSituacao->setTextFormat(Qt::RichText);
    lblSituacao->setText(texto);
    lblSituacao->setStyleSheet(excede
        ? "background: #FEF2F2; color: #B91C1C; border-radius: 8px; padding: 8px 10px;"
        : "background: #EAF6EC; color: #107A2E; border-radius: 8px; padding: 8px 10px;");
}

void LancarDividaDialog::confirmar()
{
    const DividaDTO d = divida();
    if (d.idCliente <= 0) {
        lblAviso->setText("Escolha um cliente da lista.");
        return;
    }
    if (d.descricao.size() < 2) {
        lblAviso->setText("Descreva o que o cliente comprou.");
        edDescricao->setFocus();
        return;
    }
    if (d.valorTotal <= 0) {
        lblAviso->setText("Informe um valor maior que zero.");
        spValor->setFocus();
        return;
    }
    accept();
}

// ============================================================ ReceberPagamentoDialog

ReceberPagamentoDialog::ReceberPagamentoDialog(qlonglong idCliente, qlonglong idDivida, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Receber pagamento");
    setModal(true);
    setMinimumWidth(480);

    ContasReceber_service servico;
    const ClienteCreditoDTO cli = servico.getCredito(idCliente);
    double maximo = servico.totalDevido(idCliente);
    QString escopo = QStringLiteral("Recebe da dívida mais antiga para a mais nova.");
    if (idDivida > 0) {
        const DividaDTO d = servico.getDivida(idDivida);
        maximo = d.saldo;
        escopo = QStringLiteral("Abate só da dívida \"%1\".").arg(d.descricao.toHtmlEscaped());
    }

    auto *resumo = faixa("#107A2E", "#EAF6EC");
    resumo->setTextFormat(Qt::RichText);
    resumo->setText(QStringLiteral("<b>%1</b><br>Saldo a receber: <b>%2</b><br>%3")
                        .arg(cli.nome.toHtmlEscaped(), dinheiro(maximo), escopo));

    spValor = campoDinheiro();
    spValor->setMaximum(qMax(0.01, maximo));
    spValor->setValue(maximo);
    cbForma = new QComboBox;
    cbForma->addItems(ContasReceber_service::formasDePagamento());
    dtData = campoData(QDate::currentDate());
    edObs = new QLineEdit;
    edObs->setPlaceholderText("Opcional");

    auto *dica = new QLabel("Pagamento parcial é aceito: o saldo que sobrar continua em aberto. "
                            "O valor entra no caixa aberto de quem está logado.");
    dica->setWordWrap(true);
    dica->setStyleSheet("color: #5B6B7F;");

    auto *form = new QFormLayout;
    form->addRow("Valor pago", spValor);
    form->addRow("Forma de pagamento", cbForma);
    form->addRow("Data do pagamento", dtData);
    form->addRow("Observação", edObs);

    auto *botoes = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    botoes->button(QDialogButtonBox::Ok)->setText("Confirmar recebimento");
    botoes->button(QDialogButtonBox::Ok)->setIcon(Icones::icone("check", QColor(Qt::white), 18));
    botoes->button(QDialogButtonBox::Cancel)->setText("Cancelar");
    connect(botoes, &QDialogButtonBox::accepted, this, [this]() { if (spValor->value() > 0) accept(); });
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

double ReceberPagamentoDialog::valor() const { return spValor->value(); }
QString ReceberPagamentoDialog::forma() const { return cbForma->currentText(); }
QDate ReceberPagamentoDialog::data() const { return dtData->date(); }
QString ReceberPagamentoDialog::observacao() const { return edObs->text(); }

// ============================================================ CreditoClienteDialog

CreditoClienteDialog::CreditoClienteDialog(qlonglong id, const GateGerente &g, QWidget *parent)
    : QDialog(parent)
    , idCliente(id)
    , gate(g)
{
    setWindowTitle("Dados de crédito");
    setModal(true);
    setMinimumWidth(480);

    ContasReceber_service servico;
    const ClienteCreditoDTO c = servico.getCredito(id);

    auto *titulo = new QLabel(QStringLiteral("<b>%1</b>").arg(c.nome.toHtmlEscaped()));
    titulo->setTextFormat(Qt::RichText);
    titulo->setStyleSheet("font-size: 13pt; color: #142D45;");

    chLimite = new QCheckBox("Este cliente tem limite de crédito");
    chLimite->setChecked(c.temLimite);
    spLimite = campoDinheiro();
    spLimite->setValue(c.limite);
    spLimite->setEnabled(c.temLimite);
    connect(chLimite, &QCheckBox::toggled, spLimite, &QWidget::setEnabled);

    auto *dicaLimite = new QLabel("Sem limite marcado, o cliente pode dever qualquer valor. Alterar o limite pede o "
                                  "PIN do gerente; compra acima do limite só com liberação do gerente.");
    dicaLimite->setWordWrap(true);
    dicaLimite->setStyleSheet("color: #5B6B7F;");

    edWhats = new QLineEdit(c.whatsapp);
    edWhats->setPlaceholderText("DDD + número (usado na cobrança)");
    edObs = new QPlainTextEdit(c.observacao);
    edObs->setMaximumHeight(70);

    btnAtivo = new QPushButton;
    btnAtivo->setText(c.ativo ? "Inativar cliente" : "Reativar cliente");
    btnAtivo->setStyleSheet("QPushButton { background: white; color: #B91C1C; border: 1px solid #FCA5A5; }"
                            "QPushButton:hover { background: #FEF2F2; }");
    connect(btnAtivo, &QPushButton::clicked, this, &CreditoClienteDialog::alternarAtivo);
    auto *lblAtivo = new QLabel(c.ativo ? "Cliente ativo."
                                        : "Cliente <b>inativo</b>: não aparece nas vendas e não compra a prazo.");
    lblAtivo->setTextFormat(Qt::RichText);

    auto *form = new QFormLayout;
    form->addRow(chLimite);
    form->addRow("Limite", spLimite);
    form->addRow(dicaLimite);
    form->addRow("WhatsApp", edWhats);
    form->addRow("Observação", edObs);

    auto *botoes = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    botoes->button(QDialogButtonBox::Ok)->setText("Salvar");
    botoes->button(QDialogButtonBox::Cancel)->setText("Cancelar");
    connect(botoes, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(botoes, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *linhaAtivo = new QHBoxLayout;
    linhaAtivo->addWidget(lblAtivo, 1);
    linhaAtivo->addWidget(btnAtivo);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->addWidget(titulo);
    layout->addLayout(form);
    layout->addLayout(linhaAtivo);
    layout->addWidget(botoes);
}

bool CreditoClienteDialog::temLimite() const { return chLimite->isChecked(); }
double CreditoClienteDialog::limite() const { return spLimite->value(); }
QString CreditoClienteDialog::whatsapp() const { return edWhats->text(); }
QString CreditoClienteDialog::observacao() const { return edObs->toPlainText(); }

void CreditoClienteDialog::alternarAtivo()
{
    ContasReceber_service servico;
    const bool ativo = servico.getCredito(idCliente).ativo;
    if (gate && !gate(ativo ? QStringLiteral("Inativar cliente") : QStringLiteral("Reativar cliente")))
        return;
    const auto r = ativo ? servico.inativarCliente(idCliente) : servico.reativarCliente(idCliente);
    if (!r.ok) {
        QMessageBox::warning(this, "Cliente", r.msg);
        return;
    }
    QMessageBox::information(this, "Cliente", r.msg);
    ativoAlterado = true;
    reject();
}
