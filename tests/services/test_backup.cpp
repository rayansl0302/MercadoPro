#include <QtTest>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include "test_backup.h"
#include "services/backup_service.h"
#include "infra/databaseconnection_service.h"

namespace {
void criar(const QDir &d, const QString &nome, const QByteArray &conteudo = "x")
{
    QFile f(d.filePath(nome));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(conteudo);
}
}

void TestBackup::nome_do_arquivo_tem_data_e_extensao_do_banco()
{
    const QDateTime q(QDate(2026, 10, 21), QTime(2, 5));
    QCOMPARE(Backup_service::nomeDoArquivo(q, true), QString("MercadoPro-backup-2026-10-21_0205.backup"));
    QCOMPARE(Backup_service::nomeDoArquivo(q, false), QString("MercadoPro-backup-2026-10-21_0205.db"));
}

void TestBackup::pasta_vazia_inexistente_e_sem_permissao_sao_recusadas()
{
    QVERIFY(!Backup_service::validarPasta("").isEmpty());
    QVERIFY(!Backup_service::validarPasta("   ").isEmpty());

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    // pasta que ainda não existe: é criada
    const QString nova = tmp.filePath("MEGA/Backups");
    QCOMPARE(Backup_service::validarPasta(nova), QString());
    QVERIFY(QDir(nova).exists());
    // não sobra o arquivo do teste de gravação
    QVERIFY(QDir(nova).entryList(QDir::Files | QDir::Hidden).isEmpty());
    // um arquivo no lugar da pasta: não dá para criar nem gravar
    criar(QDir(tmp.path()), "arquivo.txt");
    QVERIFY(!Backup_service::validarPasta(tmp.filePath("arquivo.txt/sub")).isEmpty());
}

void TestBackup::retencao_apaga_so_os_backups_mais_antigos_do_nosso_padrao()
{
    QTemporaryDir tmp;
    QDir d(tmp.path());
    for (int dia = 1; dia <= 5; ++dia)
        criar(d, QString("MercadoPro-backup-2026-10-%1_0200.backup").arg(dia, 2, 10, QLatin1Char('0')));
    criar(d, "foto-de-familia.jpg");                       // arquivo do usuário: nunca é tocado
    criar(d, "MercadoPro-backup-2026-09-01_0200.backup.parte"); // sobra antiga
    QFile parte(d.filePath("MercadoPro-backup-2026-09-01_0200.backup.parte"));
    QVERIFY(parte.open(QIODevice::ReadWrite));
    QVERIFY(parte.setFileTime(QDateTime::currentDateTime().addDays(-3), QFileDevice::FileModificationTime));
    parte.close();

    QCOMPARE(Backup_service::aplicarRetencao(tmp.path(), 3), 2);
    const QStringList restam = d.entryList({"MercadoPro-backup-*.backup"}, QDir::Files, QDir::Name);
    QCOMPARE(restam, (QStringList{"MercadoPro-backup-2026-10-03_0200.backup", "MercadoPro-backup-2026-10-04_0200.backup",
                                  "MercadoPro-backup-2026-10-05_0200.backup"}));
    QVERIFY(d.exists("foto-de-familia.jpg"));
    QVERIFY(!d.exists("MercadoPro-backup-2026-09-01_0200.backup.parte"));
    // nunca apaga tudo: manter 0 vira 1
    QCOMPARE(Backup_service::aplicarRetencao(tmp.path(), 0), 2);
    QCOMPARE(d.entryList({"MercadoPro-backup-*.backup"}, QDir::Files).size(), 1);
}

void TestBackup::lista_vem_do_mais_novo_para_o_mais_antigo()
{
    QTemporaryDir tmp;
    QDir d(tmp.path());
    criar(d, "MercadoPro-backup-2026-10-02_0200.db", "aaa");
    criar(d, "MercadoPro-backup-2026-10-09_0200.db", "bbbbbb");
    criar(d, "MercadoPro-backup-2026-10-05_1530.backup", "c");
    const auto l = Backup_service::listar(tmp.path());
    QCOMPARE(l.size(), 3);
    QCOMPARE(l.at(0).nome, QString("MercadoPro-backup-2026-10-09_0200.db"));
    QCOMPARE(l.at(0).tamanho, qint64(6));
    QCOMPARE(l.at(1).quando, QDateTime(QDate(2026, 10, 5), QTime(15, 30)));
    QCOMPARE(l.at(2).nome, QString("MercadoPro-backup-2026-10-02_0200.db"));
}

void TestBackup::pg_dump_nao_leva_a_senha_na_linha_de_comando()
{
    const QStringList a = Backup_service::argumentosPgDump("192.168.1.23", 5432, "postgres", "estoque", "C:/x/b.backup.parte");
    QVERIFY(a.contains("-w"));                      // nunca pergunta senha no terminal
    QVERIFY(a.contains("-Fc") || (a.contains("-F") && a.at(a.indexOf("-F") + 1) == "c"));
    QCOMPARE(a.last(), QString("estoque"));
    for (const QString &arg : a)
        QVERIFY2(!arg.contains("senha", Qt::CaseInsensitive) && !arg.contains("PASSWORD", Qt::CaseInsensitive), qPrintable(arg));
}

void TestBackup::caminho_do_pg_dump_informado_vale_primeiro()
{
    QTemporaryDir tmp;
    criar(QDir(tmp.path()), "meu_pg_dump.exe");
    const QString caminho = tmp.filePath("meu_pg_dump.exe");
    QCOMPARE(Backup_service::localizarPgDump(caminho), caminho);
    // caminho informado que não existe não é aceito (cai na busca automática)
    QVERIFY(Backup_service::localizarPgDump(tmp.filePath("nao_existe.exe")) != tmp.filePath("nao_existe.exe"));
}

void TestBackup::pg_dump_embutido_na_pasta_pgtools_e_encontrado()
{
    const QString pasta = QCoreApplication::applicationDirPath() + "/pgtools";
    QVERIFY(QDir().mkpath(pasta));
    const QString falso = pasta + "/pg_dump.exe";
    {
        QFile f(falso);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("x");
    }
    QCOMPARE(Backup_service::localizarPgDump(QString()), falso);
    // o caminho informado pelo usuário continua tendo prioridade sobre o embutido
    QTemporaryDir tmp;
    criar(QDir(tmp.path()), "outro_pg_dump.exe");
    QCOMPARE(Backup_service::localizarPgDump(tmp.filePath("outro_pg_dump.exe")), tmp.filePath("outro_pg_dump.exe"));
    QFile::remove(falso);
    QDir().rmdir(pasta);
}

void TestBackup::backup_sqlite_gera_copia_consistente_sem_arquivo_parcial()
{
    if (DatabaseConnection_service::isPostgres())
        QSKIP("este teste cobre o banco SQLite");
    QTemporaryDir tmp, pasta;
    Backup_service::usarArquivoDeConfiguracaoParaTeste(tmp.filePath("config.ini"));
    Backup_service::Config c;
    c.pasta = pasta.path();
    c.manter = 5;
    QVERIFY(Backup_service::salvar(c));

    auto *b = Backup_service::instancia();
    QSignalSpy spy(b, &Backup_service::terminou);
    QVERIFY(b->iniciarBackup());
    QVERIFY(spy.count() == 1 || spy.wait(15000));
    const auto r = spy.first().first().value<Backup_service::Resultado>();
    QVERIFY2(r.ok, qPrintable(r.msg));
    QVERIFY(QFile::exists(r.arquivo));
    QVERIFY(r.arquivo.endsWith(".db"));
    QVERIFY(r.tamanho > 0);
    QVERIFY(QDir(pasta.path()).entryList({"*.parte"}, QDir::Files).isEmpty());   // nada incompleto na pasta (o MEGA não leva meio arquivo)
    QVERIFY(!Backup_service::ultimoOk().isEmpty());
    QVERIFY(Backup_service::ultimoErro().isEmpty());

    // a cópia é um banco de verdade, com as tabelas do sistema
    {
        QSqlDatabase copia = QSqlDatabase::addDatabase("QSQLITE", "conferir_backup");
        copia.setDatabaseName(r.arquivo);
        QVERIFY(copia.open());
        QSqlQuery q(copia);
        QVERIFY(q.exec("SELECT count(*) FROM sqlite_master WHERE type = 'table' AND name IN ('produtos', 'vendas2', 'operadores')"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 3);
        copia.close();
    }
    QSqlDatabase::removeDatabase("conferir_backup");
    Backup_service::usarArquivoDeConfiguracaoParaTeste(QString());
}

void TestBackup::pasta_invalida_falha_com_motivo_e_nao_apaga_nada()
{
    QTemporaryDir tmp;
    QDir d(tmp.path());
    criar(d, "arquivo.txt");
    Backup_service::usarArquivoDeConfiguracaoParaTeste(tmp.filePath("config2.ini"));
    Backup_service::Config c;
    c.pasta = tmp.filePath("arquivo.txt/sub"); // um arquivo no caminho: impossível gravar
    QVERIFY(Backup_service::salvar(c));

    auto *b = Backup_service::instancia();
    QSignalSpy spy(b, &Backup_service::terminou);
    QVERIFY(b->iniciarBackup());
    QVERIFY(spy.count() == 1 || spy.wait(5000));
    const auto r = spy.first().first().value<Backup_service::Resultado>();
    QVERIFY(!r.ok);
    QVERIFY(!r.msg.isEmpty());
    QVERIFY(!Backup_service::ultimoErro().isEmpty());     // a tela mostra a última falha
    QVERIFY(d.exists("arquivo.txt"));
    QVERIFY(!b->emAndamento());                           // e dá para tentar de novo
    Backup_service::usarArquivoDeConfiguracaoParaTeste(QString());
}
