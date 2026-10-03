#include "nuvemdialog.h"
#include "nuvem/nuvem_service.h"
#include "nuvem/nuvemconfig.h"
#include "nuvem/nuvemcontrole_repository.h"
#include "util/icones.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

NuvemDialog::NuvemDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Nuvem (painel online)");
    setModal(true);
    resize(760, 700);

    const NuvemConfig cfg = NuvemConfig::carregar();

    auto *titulo = new QLabel("Nuvem (painel online)");
    titulo->setStyleSheet("font-size: 18pt; font-weight: 700; color: #16324F;");
    auto *explica = new QLabel("Envia os dados da loja para o painel de gestão e executa os pedidos feitos por lá "
                               "(como baixar uma conta a pagar). Fica desligada até você ativar. Só um computador "
                               "da loja deve sincronizar: o que fica ligado durante o expediente.");
    explica->setWordWrap(true);
    explica->setStyleSheet("color: #5B6B7F;");

    chAtiva = new QCheckBox("Ativar a sincronização com a nuvem");
    chAtiva->setChecked(cfg.ativa);
    chSincroniza = new QCheckBox("Este computador é o que sincroniza");
    chSincroniza->setChecked(cfg.sincronizaNesteComputador);
    edUrl = new QLineEdit(cfg.url);
    edUrl->setPlaceholderText("https://seu-painel.up.railway.app");
    edToken = new QLineEdit(cfg.token);
    edToken->setEchoMode(QLineEdit::Password);
    edToken->setPlaceholderText("Chave da loja (gerada no painel)");
    chMostrarToken = new QCheckBox("mostrar");
    connect(chMostrarToken, &QCheckBox::toggled, this,
            [this](bool on) { edToken->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password); });
    spIntervalo = new QSpinBox;
    spIntervalo->setRange(15, 1440);
    spIntervalo->setSuffix(" minutos");
    spIntervalo->setValue(cfg.intervaloMinutos);

    auto *linhaToken = new QHBoxLayout;
    linhaToken->addWidget(edToken, 1);
    linhaToken->addWidget(chMostrarToken);

    auto *form = new QFormLayout;
    form->addRow(chAtiva);
    form->addRow(chSincroniza);
    form->addRow("Endereço da API", edUrl);
    form->addRow("Chave da loja", linhaToken);
    form->addRow("Enviar os dados a cada", spIntervalo);

    btnTestar = new QPushButton("Testar conexão");
    btnTestar->setIcon(Icones::icone("circle-check", QColor(Qt::white), 18));
    btnSincronizar = new QPushButton("Sincronizar agora");
    btnSincronizar->setIcon(Icones::icone("repeat", QColor(Qt::white), 18));
    auto *btnSalvar = new QPushButton("Salvar");
    auto *btnFechar = new QPushButton("Fechar");
    btnFechar->setStyleSheet("QPushButton { background: white; color: #1E3A5F; border: 1px solid #C9D3DF; }"
                             "QPushButton:hover { background: #EAF3FB; }");

    lblStatus = new QLabel;
    lblStatus->setWordWrap(true);
    lblStatus->setTextFormat(Qt::RichText);
    lblStatus->setStyleSheet("background: #EAF3FB; color: #174F75; border-radius: 8px; padding: 8px 12px;");
    lblResultado = new QLabel;
    lblResultado->setWordWrap(true);

    tabelaLog = new QTableWidget(0, 3);
    tabelaLog->setHorizontalHeaderLabels({"Quando", "Nível", "Mensagem"});
    tabelaLog->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tabelaLog->setAlternatingRowColors(true);
    tabelaLog->verticalHeader()->setVisible(false);
    tabelaLog->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    tabelaLog->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tabelaLog->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    connect(btnSalvar, &QPushButton::clicked, this, [this]() { if (salvar()) { lblResultado->setStyleSheet("color: #15803D;"); lblResultado->setText("Configuração salva."); atualizarStatus(); } });
    connect(btnTestar, &QPushButton::clicked, this, &NuvemDialog::testar);
    connect(btnSincronizar, &QPushButton::clicked, this, &NuvemDialog::sincronizarAgora);
    connect(btnFechar, &QPushButton::clicked, this, &QDialog::accept);
    connect(Nuvem_service::instancia(), &Nuvem_service::statusMudou, this, [this]() { atualizarStatus(); atualizarLog(); });

    auto *botoes = new QHBoxLayout;
    botoes->addWidget(btnSalvar);
    botoes->addWidget(btnTestar);
    botoes->addWidget(btnSincronizar);
    botoes->addStretch(1);
    botoes->addWidget(btnFechar);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(10);
    layout->addWidget(titulo);
    layout->addWidget(explica);
    layout->addLayout(form);
    layout->addLayout(botoes);
    layout->addWidget(lblResultado);
    layout->addWidget(lblStatus);
    layout->addWidget(new QLabel("Histórico"));
    layout->addWidget(tabelaLog, 1);

    atualizarStatus();
    atualizarLog();
}

bool NuvemDialog::salvar()
{
    NuvemConfig cfg;
    cfg.ativa = chAtiva->isChecked();
    cfg.sincronizaNesteComputador = chSincroniza->isChecked();
    cfg.url = edUrl->text().trimmed();
    cfg.token = edToken->text().trimmed();
    cfg.intervaloMinutos = spIntervalo->value();
    // desligada, não exige nada; ligada, só salva se estiver tudo certo
    if (cfg.ativa) {
        const QString erro = cfg.validar();
        if (!erro.isEmpty()) {
            lblResultado->setStyleSheet("color: #B91C1C;");
            lblResultado->setText(erro);
            return false;
        }
    }
    if (!NuvemConfig::salvar(cfg)) {
        lblResultado->setStyleSheet("color: #B91C1C;");
        lblResultado->setText("Não foi possível gravar a configuração.");
        return false;
    }
    Nuvem_service::instancia()->iniciar();
    return true;
}

void NuvemDialog::ocupado(bool sim)
{
    btnTestar->setEnabled(!sim);
    btnSincronizar->setEnabled(!sim);
    setCursor(sim ? Qt::BusyCursor : Qt::ArrowCursor);
}

void NuvemDialog::testar()
{
    if (!salvar())
        return;
    ocupado(true);
    const auto r = Nuvem_service::instancia()->testarConexao();
    ocupado(false);
    lblResultado->setStyleSheet(r.ok ? "color: #15803D;" : "color: #B91C1C;");
    lblResultado->setText(r.mensagem);
}

void NuvemDialog::sincronizarAgora()
{
    if (!salvar())
        return;
    ocupado(true);
    const auto r = Nuvem_service::instancia()->executar(true);
    ocupado(false);
    if (!r.executou) {
        lblResultado->setStyleSheet("color: #B45309;");
        lblResultado->setText(r.mensagem.isEmpty()
            ? QStringLiteral("Nada a fazer: ative a sincronização e marque este computador como o que sincroniza.")
            : r.mensagem);
    } else {
        lblResultado->setStyleSheet(r.ok ? "color: #15803D;" : "color: #B91C1C;");
        lblResultado->setText(r.mensagem);
    }
    atualizarStatus();
    atualizarLog();
}

void NuvemDialog::atualizarStatus()
{
    const auto s = Nuvem_service::instancia()->status();
    QString t;
    t += QStringLiteral("<b>Situação:</b> %1<br>").arg(s.ativa ? (s.emExecucao ? "sincronizando agora..." : "ativa") : "desligada neste computador");
    t += QStringLiteral("<b>Último ciclo bem-sucedido:</b> %1<br>").arg(s.ultimoCicloOk.isEmpty() ? "—" : s.ultimoCicloOk);
    t += QStringLiteral("<b>Último envio de dados:</b> %1<br>").arg(s.ultimoEnvioOk.isEmpty() ? "—" : s.ultimoEnvioOk);
    if (!s.ultimoResumo.isEmpty())
        t += QStringLiteral("<b>Resumo:</b> %1<br>").arg(s.ultimoResumo.toHtmlEscaped());
    if (!s.ultimoErro.isEmpty())
        t += QStringLiteral("<span style='color:#B91C1C'><b>Último erro:</b> %1</span><br>").arg(s.ultimoErro.toHtmlEscaped());
    if (!s.donoDaTrava.isEmpty())
        t += QStringLiteral("<b>Computador que sincroniza:</b> %1").arg(s.donoDaTrava.toHtmlEscaped());
    lblStatus->setText(t);
}

void NuvemDialog::atualizarLog()
{
    NuvemControle_repository repo;
    const auto linhas = repo.ultimosLogs(100);
    tabelaLog->setRowCount(linhas.size());
    for (int i = 0; i < linhas.size(); ++i) {
        tabelaLog->setItem(i, 0, new QTableWidgetItem(linhas.at(i).quando));
        auto *nivel = new QTableWidgetItem(linhas.at(i).nivel);
        if (linhas.at(i).nivel == "ERRO")
            nivel->setForeground(QColor("#B91C1C"));
        tabelaLog->setItem(i, 1, nivel);
        tabelaLog->setItem(i, 2, new QTableWidgetItem(linhas.at(i).mensagem));
    }
}
