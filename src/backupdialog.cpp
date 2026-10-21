#include "backupdialog.h"
#include "infra/databaseconnection_service.h"
#include "util/icones.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTimeEdit>
#include <QUrl>
#include <QVBoxLayout>

namespace {
QString tamanhoLegivel(qint64 bytes)
{
    return QLocale(QLocale::Portuguese, QLocale::Brazil).formattedDataSize(bytes, 1);
}
}

BackupDialog::BackupDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Backup do banco de dados");
    setModal(true);
    resize(780, 720);

    const Backup_service::Config cfg = Backup_service::carregar();
    const bool postgres = DatabaseConnection_service::isPostgres();

    auto *titulo = new QLabel("Backup do banco de dados");
    titulo->setStyleSheet("font-size: 18pt; font-weight: 700; color: #142D45;");
    auto *explica = new QLabel(
        "Guarda uma cópia completa do banco (vendas, produtos, clientes, contas, notas) na pasta que você escolher. "
        "Dica: escolha uma pasta sincronizada pelo MEGA (ou outro serviço de nuvem) para a cópia sair do computador. "
        "O arquivo só recebe o nome final quando está completo, então a nuvem nunca envia um backup pela metade. "
        "Faça o backup automático <b>no computador que guarda o banco</b> (o servidor).");
    explica->setWordWrap(true);
    explica->setTextFormat(Qt::RichText);
    explica->setStyleSheet("color: #5B6B7F;");

    edPasta = new QLineEdit(cfg.pasta);
    edPasta->setPlaceholderText("Ex.: C:\\Users\\Loja\\MEGA\\Backups");
    auto *btnPasta = new QPushButton("Escolher...");
    btnPasta->setStyleSheet("QPushButton { background: white; color: #142D45; border: 1px solid #C9D3DF; }"
                            "QPushButton:hover { background: #EAF6EC; }");
    auto *btnAbrir = new QPushButton("Abrir pasta");
    btnAbrir->setStyleSheet(btnPasta->styleSheet());
    auto *linhaPasta = new QHBoxLayout;
    linhaPasta->addWidget(edPasta, 1);
    linhaPasta->addWidget(btnPasta);
    linhaPasta->addWidget(btnAbrir);

    spManter = new QSpinBox;
    spManter->setRange(1, 365);
    spManter->setSuffix(" backups");
    spManter->setValue(cfg.manter);

    chAutomatico = new QCheckBox("Fazer um backup automático todos os dias neste computador, a partir de");
    chAutomatico->setChecked(cfg.automatico);
    edHora = new QTimeEdit(cfg.hora);
    edHora->setDisplayFormat("HH:mm");
    auto *linhaAuto = new QHBoxLayout;
    linhaAuto->addWidget(chAutomatico);
    linhaAuto->addWidget(edHora);
    linhaAuto->addStretch(1);

    edPgDump = new QLineEdit(cfg.pgDump);
    const QString achado = Backup_service::localizarPgDump(cfg.pgDump);
    edPgDump->setPlaceholderText(achado.isEmpty() ? "pg_dump.exe não encontrado — escolha o arquivo" : "Encontrado automaticamente: " + achado);
    btnPgDump = new QPushButton("Escolher...");
    btnPgDump->setStyleSheet(btnPasta->styleSheet());
    auto *linhaPg = new QHBoxLayout;
    linhaPg->addWidget(edPgDump, 1);
    linhaPg->addWidget(btnPgDump);
    lblPgDump = new QLabel("Programa pg_dump");

    auto *form = new QFormLayout;
    form->addRow("Pasta dos backups", linhaPasta);
    form->addRow("Guardar os últimos", spManter);
    form->addRow(linhaAuto);
    if (postgres)
        form->addRow(lblPgDump, linhaPg);
    else {
        lblPgDump->hide();
        edPgDump->hide();
        btnPgDump->hide();
    }

    btnAgora = new QPushButton("Fazer backup agora");
    btnAgora->setIcon(Icones::icone("database-backup", QColor(Qt::white), 18));
    auto *btnSalvar = new QPushButton("Salvar");
    auto *btnFechar = new QPushButton("Fechar");
    btnFechar->setStyleSheet(btnPasta->styleSheet());

    lblStatus = new QLabel;
    lblStatus->setWordWrap(true);
    lblStatus->setTextFormat(Qt::RichText);
    lblStatus->setStyleSheet("background: #EAF6EC; color: #107A2E; border-radius: 8px; padding: 8px 12px;");
    lblResultado = new QLabel;
    lblResultado->setWordWrap(true);

    tabela = new QTableWidget(0, 3);
    tabela->setHorizontalHeaderLabels({"Arquivo", "Data", "Tamanho"});
    tabela->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tabela->setAlternatingRowColors(true);
    tabela->verticalHeader()->setVisible(false);
    tabela->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    tabela->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tabela->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    auto *restaurar = new QLabel(postgres
        ? "<b>Para restaurar</b> (feito por quem administra o servidor, com o programa fechado em todos os caixas): "
          "crie um banco vazio e rode <code>pg_restore -h localhost -U postgres -d NOME_DO_BANCO arquivo.backup</code>."
        : "<b>Para restaurar:</b> feche o programa e substitua o arquivo <code>estoque.db</code> pelo arquivo de backup.");
    restaurar->setWordWrap(true);
    restaurar->setTextFormat(Qt::RichText);
    restaurar->setStyleSheet("color: #5B6B7F;");

    connect(btnPasta, &QPushButton::clicked, this, &BackupDialog::escolherPasta);
    connect(btnPgDump, &QPushButton::clicked, this, &BackupDialog::escolherPgDump);
    connect(btnAbrir, &QPushButton::clicked, this, [this]() {
        const QString p = edPasta->text().trimmed();
        if (p.isEmpty() || !QDesktopServices::openUrl(QUrl::fromLocalFile(p)))
            QMessageBox::information(this, "Backup", "A pasta ainda não existe. Escolha uma e clique em Salvar.");
    });
    connect(btnSalvar, &QPushButton::clicked, this, [this]() {
        if (salvar()) {
            lblResultado->setStyleSheet("color: #15803D;");
            lblResultado->setText("Configuração salva.");
            atualizarStatus();
            atualizarLista();
        }
    });
    connect(btnAgora, &QPushButton::clicked, this, &BackupDialog::fazerAgora);
    connect(btnFechar, &QPushButton::clicked, this, &QDialog::accept);
    connect(Backup_service::instancia(), &Backup_service::terminou, this, [this](const Backup_service::Resultado &r) {
        btnAgora->setEnabled(true);
        mostrarResultado(r);
        atualizarStatus();
        atualizarLista();
    });

    auto *botoes = new QHBoxLayout;
    botoes->addWidget(btnSalvar);
    botoes->addWidget(btnAgora);
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
    layout->addWidget(new QLabel("Backups nesta pasta"));
    layout->addWidget(tabela, 1);
    layout->addWidget(restaurar);

    atualizarStatus();
    atualizarLista();
}

bool BackupDialog::salvar()
{
    Backup_service::Config c;
    c.pasta = edPasta->text().trimmed();
    c.manter = spManter->value();
    c.automatico = chAutomatico->isChecked();
    c.hora = edHora->time();
    c.pgDump = edPgDump->text().trimmed();
    const QString erro = Backup_service::validarPasta(c.pasta);
    if (!erro.isEmpty()) {
        lblResultado->setStyleSheet("color: #B91C1C;");
        lblResultado->setText(erro);
        return false;
    }
    if (!Backup_service::salvar(c)) {
        lblResultado->setStyleSheet("color: #B91C1C;");
        lblResultado->setText("Não foi possível gravar a configuração.");
        return false;
    }
    return true;
}

void BackupDialog::escolherPasta()
{
    const QString p = QFileDialog::getExistingDirectory(this, "Escolha a pasta dos backups", edPasta->text());
    if (!p.isEmpty())
        edPasta->setText(p);
}

void BackupDialog::escolherPgDump()
{
    const QString p = QFileDialog::getOpenFileName(this, "Escolha o pg_dump (pasta bin do PostgreSQL)", edPgDump->text(),
                                                   "pg_dump (pg_dump*);;Todos os arquivos (*)");
    if (!p.isEmpty())
        edPgDump->setText(p);
}

void BackupDialog::fazerAgora()
{
    if (!salvar()) // usa o que está na tela
        return;
    btnAgora->setEnabled(false);
    lblResultado->setStyleSheet("color: #5B6B7F;");
    lblResultado->setText("Fazendo o backup... (pode levar alguns minutos em bancos grandes)");
    if (!Backup_service::instancia()->iniciarBackup()) {
        btnAgora->setEnabled(true);
        lblResultado->setText("Já existe um backup em andamento.");
    }
}

void BackupDialog::mostrarResultado(const Backup_service::Resultado &r)
{
    if (r.ok) {
        lblResultado->setStyleSheet("color: #15803D; font-weight: 600;");
        lblResultado->setText(QStringLiteral("Backup concluído (%1): %2").arg(tamanhoLegivel(r.tamanho), QFileInfo(r.arquivo).fileName()));
    } else {
        lblResultado->setStyleSheet("color: #B91C1C; font-weight: 600;");
        lblResultado->setText("Falhou: " + r.msg);
    }
}

void BackupDialog::atualizarStatus()
{
    const QString ok = Backup_service::ultimoOk();
    const QString erro = Backup_service::ultimoErro();
    QString t = QStringLiteral("<b>Último backup:</b> %1").arg(ok.isEmpty() ? "nunca foi feito" : ok);
    if (!erro.isEmpty())
        t += QStringLiteral("<br><span style='color:#B91C1C'><b>Última falha:</b> %1</span>").arg(erro.toHtmlEscaped());
    lblStatus->setText(t);
}

void BackupDialog::atualizarLista()
{
    const QString pasta = edPasta->text().trimmed();
    const auto lista = pasta.isEmpty() ? QList<Backup_service::Arquivo>() : Backup_service::listar(pasta);
    tabela->setRowCount(lista.size());
    for (int i = 0; i < lista.size(); ++i) {
        tabela->setItem(i, 0, new QTableWidgetItem(lista.at(i).nome));
        tabela->setItem(i, 1, new QTableWidgetItem(lista.at(i).quando.toString("dd/MM/yyyy HH:mm")));
        auto *tam = new QTableWidgetItem(tamanhoLegivel(lista.at(i).tamanho));
        tam->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        tabela->setItem(i, 2, tam);
    }
}
