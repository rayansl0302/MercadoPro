#include "extratoclientedialog.h"
#include "services/empresa_service.h"
#include "util/icones.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
QString dinheiro(double v) { return QLocale(QLocale::Portuguese, QLocale::Brazil).toCurrencyString(v, "R$ "); }

enum Coluna { Data, Tipo, Descricao, Valor, Saldo, Operador, Empresa, kColunas };

QPushButton *botaoSecundario(const QString &texto, const QString &icone, const QString &cor)
{
    auto *b = new QPushButton(texto);
    b->setIcon(Icones::icone(icone, QColor(cor), 18));
    b->setStyleSheet(QStringLiteral(
        "QPushButton { background: white; color: %1; border: 1px solid #C9D3DF; }"
        "QPushButton:hover { background: #EAF6EC; border-color: #17963A; }"
        "QPushButton:disabled { background: #F1F5F9; color: #94A3B8; border-color: #E2E8F0; }").arg(cor));
    return b;
}
}

ExtratoClienteDialog::ExtratoClienteDialog(qlonglong id, GateGerente g, QWidget *parent)
    : QDialog(parent)
    , idCliente(id)
    , gate(std::move(g))
{
    setWindowTitle("Extrato do cliente");
    setModal(true);
    resize(1120, 640);

    lblCliente = new QLabel;
    lblCliente->setStyleSheet("font-size: 18pt; font-weight: 700; color: #142D45;");
    lblResumo = new QLabel;
    lblResumo->setTextFormat(Qt::RichText);
    lblResumo->setWordWrap(true);
    lblResumo->setStyleSheet("background: #EAF6EC; color: #107A2E; border-radius: 8px; padding: 8px 12px;");

    tabela = new QTableWidget(0, kColunas);
    tabela->setHorizontalHeaderLabels({"Data", "Tipo", "Descrição", "Valor", "Saldo depois", "Operador", "Empresa"});
    tabela->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tabela->setSelectionBehavior(QAbstractItemView::SelectRows);
    tabela->setSelectionMode(QAbstractItemView::SingleSelection);
    tabela->setAlternatingRowColors(true);
    tabela->verticalHeader()->setVisible(false);
    tabela->horizontalHeader()->setSectionResizeMode(Descricao, QHeaderView::Stretch);
    for (int c : {Data, Tipo, Valor, Saldo, Operador, Empresa})
        tabela->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    connect(tabela, &QTableWidget::itemSelectionChanged, this, &ExtratoClienteDialog::atualizarBotoes);

    auto *btnLancar = new QPushButton("Lançar dívida");
    btnLancar->setIcon(Icones::icone("plus", QColor(Qt::white), 18));
    btnReceber = new QPushButton("Receber pagamento");
    btnReceber->setIcon(Icones::icone("circle-check", QColor(Qt::white), 18));
    btnReceberDivida = botaoSecundario("Receber desta dívida", "wallet", "#142D45");
    btnEstornar = botaoSecundario("Estornar pagamento", "undo-2", "#B45309");
    btnCancelar = botaoSecundario("Cancelar lançamento", "ban", "#B91C1C");
    auto *btnCredito = botaoSecundario("Dados de crédito", "settings", "#142D45");
    auto *btnCobrar = botaoSecundario("Cobrar no WhatsApp", "mail", "#15803D");
    auto *btnFechar = botaoSecundario("Fechar", "x", "#142D45");

    connect(btnLancar, &QPushButton::clicked, this, [this]() {
        if (lancarDividaComTela(this, gate, idCliente))
            recarregar();
    });
    connect(btnReceber, &QPushButton::clicked, this, [this]() {
        if (receberComTela(this, idCliente))
            recarregar();
    });
    connect(btnReceberDivida, &QPushButton::clicked, this, [this]() {
        const LinhaExtratoDTO l = selecionada();
        if (l.idDivida > 0 && receberComTela(this, idCliente, l.idDivida))
            recarregar();
    });
    connect(btnEstornar, &QPushButton::clicked, this, &ExtratoClienteDialog::estornar);
    connect(btnCancelar, &QPushButton::clicked, this, &ExtratoClienteDialog::cancelarLancamento);
    connect(btnCredito, &QPushButton::clicked, this, [this]() {
        editarCreditoComTela(this, gate, idCliente);
        recarregar();
    });
    connect(btnCobrar, &QPushButton::clicked, this, [this]() { cobrarPorWhatsApp(this, idCliente); });
    connect(btnFechar, &QPushButton::clicked, this, &QDialog::accept);

    auto *acoes = new QHBoxLayout;
    acoes->setSpacing(8);
    acoes->addWidget(btnLancar);
    acoes->addWidget(btnReceber);
    acoes->addWidget(btnReceberDivida);
    acoes->addSpacing(10);
    acoes->addWidget(btnEstornar);
    acoes->addWidget(btnCancelar);

    auto *rodape = new QHBoxLayout;
    rodape->setSpacing(8);
    rodape->addWidget(btnCredito);
    rodape->addWidget(btnCobrar);
    rodape->addStretch(1);
    rodape->addWidget(btnFechar);

    auto *dica = new QLabel("Compras a prazo feitas no PDV aparecem aqui, mas são recebidas e estornadas em "
                            "Vendas › à prazo. Lançamentos de caderneta são tratados nesta janela.");
    dica->setWordWrap(true);
    dica->setStyleSheet("color: #5B6B7F;");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(10);
    layout->addWidget(lblCliente);
    layout->addWidget(lblResumo);
    layout->addLayout(acoes);
    layout->addWidget(tabela, 1);
    layout->addWidget(dica);
    layout->addLayout(rodape);

    recarregar();
}

LinhaExtratoDTO ExtratoClienteDialog::selecionada() const
{
    const int linha = tabela->currentRow();
    if (linha < 0 || linha >= linhas.size())
        return LinhaExtratoDTO();
    return linhas.at(linha);
}

void ExtratoClienteDialog::recarregar()
{
    const ClienteCreditoDTO c = servico.getCredito(idCliente);
    lblCliente->setText(c.nome + (c.ativo ? QString() : QStringLiteral("  (inativo)")));

    const auto lim = servico.situacaoLimite(idCliente);
    QString resumo = QStringLiteral("Total devido: <b>%1</b>").arg(dinheiro(lim.devido));
    if (lim.temLimite)
        resumo += QStringLiteral("  ·  Limite: <b>%1</b>  ·  Disponível: <b>%2</b>").arg(dinheiro(lim.limite), dinheiro(lim.disponivel));
    else
        resumo += QStringLiteral("  ·  Sem limite de crédito");
    if (!c.telefone.isEmpty() || !c.whatsapp.isEmpty())
        resumo += QStringLiteral("  ·  Contato: %1").arg((c.whatsapp.isEmpty() ? c.telefone : c.whatsapp).toHtmlEscaped());
    if (!c.observacao.isEmpty())
        resumo += QStringLiteral("<br>Obs.: %1").arg(c.observacao.toHtmlEscaped());
    lblResumo->setText(resumo);
    lblResumo->setStyleSheet(lim.excede || (lim.temLimite && lim.devido > lim.limite + 0.004)
        ? "background: #FEF2F2; color: #B91C1C; border-radius: 8px; padding: 8px 12px;"
        : "background: #EAF6EC; color: #107A2E; border-radius: 8px; padding: 8px 12px;");

    const qlonglong selecionadaId = selecionada().idPagamento;
    linhas = servico.extrato(idCliente);
    tabela->setRowCount(linhas.size());
    auto *es = Empresa_service::instancia();
    int selecionar = -1;

    for (int i = 0; i < linhas.size(); ++i) {
        const LinhaExtratoDTO &l = linhas.at(i);
        if (selecionadaId > 0 && l.idPagamento == selecionadaId)
            selecionar = i;
        QString tipo;
        QColor cor("#1F2937");
        bool riscado = false;
        switch (l.tipo) {
        case LinhaExtratoDTO::Compra: tipo = "Dívida lançada"; cor = QColor("#B91C1C"); break;
        case LinhaExtratoDTO::VendaPrazo: tipo = "Venda a prazo"; cor = QColor("#B91C1C"); break;
        case LinhaExtratoDTO::Pagamento: tipo = "Pagamento"; cor = QColor("#15803D"); break;
        case LinhaExtratoDTO::PagamentoVenda: tipo = "Pagamento (venda)"; cor = QColor("#15803D"); break;
        case LinhaExtratoDTO::PagamentoEstornado: tipo = "Pagamento estornado"; cor = QColor("#94A3B8"); riscado = true; break;
        case LinhaExtratoDTO::DividaCancelada: tipo = "Lançamento cancelado"; cor = QColor("#94A3B8"); riscado = true; break;
        }
        auto celula = [&](int col, const QString &texto, Qt::Alignment a = Qt::AlignLeft | Qt::AlignVCenter, bool negrito = false) {
            auto *it = new QTableWidgetItem(texto);
            it->setForeground(cor);
            it->setTextAlignment(a);
            QFont f = it->font();
            f.setStrikeOut(riscado);
            f.setBold(negrito);
            it->setFont(f);
            tabela->setItem(i, col, it);
        };
        const QDateTime d = QDateTime::fromString(l.data, "yyyy-MM-dd HH:mm:ss");
        const bool soData = l.data.endsWith("00:00:00");
        celula(Data, d.isValid() ? d.toString(soData ? "dd/MM/yyyy" : "dd/MM/yyyy HH:mm") : l.data, Qt::AlignCenter);
        celula(Tipo, tipo, Qt::AlignLeft | Qt::AlignVCenter, true);
        celula(Descricao, l.descricao + (l.observacao.isEmpty() ? QString() : " · " + l.observacao));
        celula(Valor, (l.efeito < 0 ? "− " : "") + dinheiro(l.valor), Qt::AlignRight | Qt::AlignVCenter);
        celula(Saldo, dinheiro(l.saldoApos), Qt::AlignRight | Qt::AlignVCenter, true);
        celula(Operador, l.operador);
        const EmpresaDTO e = es->getPorId(l.idEmpresa);
        celula(Empresa, e.valida() ? e.apelido : QStringLiteral("—"));
    }
    if (selecionar >= 0)
        tabela->selectRow(selecionar);
    else if (!linhas.isEmpty())
        tabela->scrollToBottom();
    atualizarBotoes();
}

void ExtratoClienteDialog::atualizarBotoes()
{
    const LinhaExtratoDTO l = selecionada();
    const bool temDevido = servico.totalDevido(idCliente) > 0.004;
    btnReceber->setEnabled(temDevido);
    bool dividaAberta = false;
    if (l.idDivida > 0)
        dividaAberta = servico.getDivida(l.idDivida).aberta();
    btnReceberDivida->setEnabled(dividaAberta);
    btnEstornar->setEnabled(l.estornavel);
    btnCancelar->setEnabled(l.cancelavel);
}

void ExtratoClienteDialog::estornar()
{
    const LinhaExtratoDTO l = selecionada();
    if (!l.estornavel)
        return;
    if (gate && !gate(QStringLiteral("Estornar pagamento")))
        return;
    bool ok = false;
    const QString motivo = QInputDialog::getText(this, "Estornar pagamento",
        QStringLiteral("Motivo do estorno de %1:").arg(dinheiro(l.valor)), QLineEdit::Normal, QString(), &ok);
    if (!ok)
        return;
    const auto r = servico.estornarPagamento(l.idPagamento, motivo);
    if (!r.ok)
        QMessageBox::warning(this, "Estornar pagamento", r.msg);
    recarregar();
}

void ExtratoClienteDialog::cancelarLancamento()
{
    const LinhaExtratoDTO l = selecionada();
    if (!l.cancelavel)
        return;
    if (gate && !gate(QStringLiteral("Cancelar lançamento")))
        return;
    bool ok = false;
    const QString motivo = QInputDialog::getText(this, "Cancelar lançamento",
        QStringLiteral("Motivo do cancelamento de \"%1\" (%2):").arg(l.descricao, dinheiro(l.valor)),
        QLineEdit::Normal, QString(), &ok);
    if (!ok)
        return;
    const auto r = servico.cancelarDivida(l.idDivida, motivo);
    if (!r.ok)
        QMessageBox::warning(this, "Cancelar lançamento", r.msg);
    recarregar();
}
