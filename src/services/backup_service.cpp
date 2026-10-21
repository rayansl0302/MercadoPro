#include "backup_service.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTimer>
#include <algorithm>

#include "../infra/apppath_service.h"
#include "../infra/databaseconnection_service.h"

namespace {
constexpr auto kPrefixo = "MercadoPro-backup-";
constexpr int kLimiteMinutosPgDump = 60;

QString arquivoIniDeTeste;

QSettings ini()
{
    return QSettings(arquivoIniDeTeste.isEmpty() ? AppPath_service::configPath() : arquivoIniDeTeste, QSettings::IniFormat);
}

QDateTime quandoDoNome(const QString &nome)
{
    static const QRegularExpression re(QStringLiteral("(\\d{4}-\\d{2}-\\d{2}_\\d{4})"));
    const auto m = re.match(nome);
    return m.hasMatch() ? QDateTime::fromString(m.captured(1), "yyyy-MM-dd_HHmm") : QDateTime();
}
}

Backup_service *Backup_service::instancia()
{
    static Backup_service s;
    return &s;
}

Backup_service::Backup_service(QObject *parent) : QObject(parent)
{
    qRegisterMetaType<Backup_service::Resultado>("Backup_service::Resultado");
}

// ------------------------------------------------------------------ configuração
void Backup_service::usarArquivoDeConfiguracaoParaTeste(const QString &caminhoIni)
{
    arquivoIniDeTeste = caminhoIni;
}

Backup_service::Config Backup_service::carregar()
{
    QSettings s = ini();
    Config c;
    c.pasta = s.value("backup/pasta").toString().trimmed();
    c.manter = qBound(1, s.value("backup/manter", 14).toInt(), 365);
    c.automatico = s.value("backup/automatico", false).toBool();
    const QTime h = QTime::fromString(s.value("backup/hora", "02:00").toString(), "HH:mm");
    c.hora = h.isValid() ? h : QTime(2, 0);
    c.pgDump = s.value("backup/pg_dump").toString().trimmed();
    return c;
}

bool Backup_service::salvar(const Config &c)
{
    QSettings s = ini();
    s.setValue("backup/pasta", c.pasta.trimmed());
    s.setValue("backup/manter", qBound(1, c.manter, 365));
    s.setValue("backup/automatico", c.automatico);
    s.setValue("backup/hora", c.hora.toString("HH:mm"));
    s.setValue("backup/pg_dump", c.pgDump.trimmed());
    s.sync();
    return s.status() == QSettings::NoError;
}

QString Backup_service::ultimoOk()
{
    return ini().value("backup/ultimo_ok").toString();
}

QString Backup_service::ultimoErro()
{
    return ini().value("backup/ultimo_erro").toString();
}

// ------------------------------------------------------------------ partes puras
QString Backup_service::nomeDoArquivo(const QDateTime &quando, bool postgres)
{
    return QStringLiteral("%1%2.%3").arg(kPrefixo, quando.toString("yyyy-MM-dd_HHmm"), postgres ? "backup" : "db");
}

QString Backup_service::validarPasta(const QString &pastaBruta)
{
    const QString pasta = pastaBruta.trimmed();
    if (pasta.isEmpty())
        return "Escolha a pasta onde os backups serão salvos.";
    QDir dir(pasta);
    if (!dir.exists() && !QDir().mkpath(pasta))
        return "Não foi possível criar a pasta: " + pasta;
    // teste de gravação: pasta de rede/MEGA desconectada ou sem permissão aparece aqui, não só no dia do backup
    const QString teste = dir.filePath(QStringLiteral(".teste-gravacao-%1").arg(QRandomGenerator::global()->generate()));
    QFile f(teste);
    if (!f.open(QIODevice::WriteOnly))
        return "Não é possível gravar nessa pasta (sem permissão ou desconectada): " + pasta;
    f.close();
    f.remove();
    return QString();
}

QList<Backup_service::Arquivo> Backup_service::listar(const QString &pasta)
{
    QList<Arquivo> lista;
    const QDir dir(pasta);
    const auto infos = dir.entryInfoList({QString(kPrefixo) + "*.backup", QString(kPrefixo) + "*.db"}, QDir::Files);
    for (const QFileInfo &i : infos) {
        Arquivo a;
        a.nome = i.fileName();
        a.quando = quandoDoNome(a.nome);
        if (!a.quando.isValid())
            a.quando = i.lastModified();
        a.tamanho = i.size();
        lista << a;
    }
    std::sort(lista.begin(), lista.end(), [](const Arquivo &x, const Arquivo &y) { return x.quando > y.quando; });
    return lista;
}

int Backup_service::aplicarRetencao(const QString &pasta, int manter)
{
    manter = qMax(1, manter);
    const QDir dir(pasta);
    int apagados = 0;
    const auto lista = listar(pasta);
    for (int i = manter; i < lista.size(); ++i)
        if (QFile::remove(dir.filePath(lista.at(i).nome)))
            ++apagados;
    // sobras de backup interrompido (".parte" com mais de um dia)
    const auto sobras = dir.entryInfoList({QString(kPrefixo) + "*.parte"}, QDir::Files);
    for (const QFileInfo &i : sobras)
        if (i.lastModified().secsTo(QDateTime::currentDateTime()) > 24 * 3600)
            QFile::remove(i.absoluteFilePath());
    return apagados;
}

QStringList Backup_service::argumentosPgDump(const QString &host, int porta, const QString &usuario,
                                             const QString &banco, const QString &arquivo)
{
    // sem senha aqui: ela vai por PGPASSWORD (linha de comando aparece na lista de processos)
    return {"-h", host, "-p", QString::number(porta), "-U", usuario, "-w", "-F", "c", "-Z", "6", "-f", arquivo, banco};
}

QString Backup_service::localizarPgDump(const QString &configurado)
{
    if (!configurado.trimmed().isEmpty() && QFileInfo::exists(configurado.trimmed()))
        return configurado.trimmed();
    const QString noPath = QStandardPaths::findExecutable("pg_dump");
    if (!noPath.isEmpty())
        return noPath;
    // instalações padrão do PostgreSQL: usa a versão mais nova encontrada
    QStringList bases;
    for (const char *v : {"ProgramFiles", "ProgramFiles(x86)", "ProgramW6432"}) {
        const QString p = qEnvironmentVariable(v);
        if (!p.isEmpty())
            bases << p + "/PostgreSQL";
    }
    bases << "/usr/lib/postgresql";
    QString melhor;
    int melhorVersao = -1;
    for (const QString &base : bases) {
        const QDir d(base);
        for (const QString &sub : d.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            for (const char *exe : {"pg_dump.exe", "pg_dump"}) {
                const QString candidato = d.filePath(sub + "/bin/" + exe);
                if (QFileInfo::exists(candidato) && sub.toInt() > melhorVersao) {
                    melhorVersao = sub.toInt();
                    melhor = candidato;
                }
            }
        }
    }
    return melhor;
}

// ------------------------------------------------------------------ execução
bool Backup_service::iniciarBackup()
{
    if (emAndamento())
        return false;
    ocupado = true;
    erroProcesso.clear();
    const Config cfg = carregar();

    const QString motivo = validarPasta(cfg.pasta);
    if (!motivo.isEmpty()) {
        Resultado r;
        r.msg = motivo;
        concluir(r, false);
        return true;
    }
    const bool pg = DatabaseConnection_service::isPostgres();
    destinoFinal = QDir(cfg.pasta).filePath(nomeDoArquivo(QDateTime::currentDateTime(), pg));
    destinoParcial = destinoFinal + ".parte";
    QFile::remove(destinoParcial);

    if (pg)
        backupPostgres(cfg);
    else
        backupSqlite(cfg);
    return true;
}

void Backup_service::concluir(const Resultado &resultado, bool manterAntigos)
{
    Resultado r = resultado;
    r.automatico = origemAutomatica;
    origemAutomatica = false;
    QSettings s = ini();
    if (r.ok) {
        s.setValue("backup/ultimo_ok", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
        s.setValue("backup/ultimo_arquivo", r.arquivo);
        s.remove("backup/ultimo_erro");
        if (manterAntigos)
            aplicarRetencao(QFileInfo(r.arquivo).absolutePath(), carregar().manter);
    } else {
        s.setValue("backup/ultimo_erro", r.msg);
        s.setValue("backup/ultimo_erro_em", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    }
    s.sync();
    if (processo) {
        processo->deleteLater();
        processo = nullptr;
    }
    ocupado = false;
    emit terminou(r);
}

void Backup_service::backupSqlite(const Config &)
{
    Resultado r;
    QSqlDatabase db = DatabaseConnection_service::db();
    bool copiou = false;
    if (db.isOpen() || db.open()) {
        QString caminho = destinoParcial;
        caminho.replace('\'', "''");
        QSqlQuery q(db);
        copiou = q.exec(QStringLiteral("VACUUM INTO '%1'").arg(caminho)); // cópia consistente, mesmo com o programa em uso
    }
    if (!copiou) {
        QFile::remove(destinoParcial);
        copiou = QFile::copy(AppPath_service::databasePath(), destinoParcial);
    }
    if (!copiou || QFileInfo(destinoParcial).size() <= 0) {
        QFile::remove(destinoParcial);
        r.msg = "Não foi possível copiar o banco de dados para a pasta de backup.";
        concluir(r, false);
        return;
    }
    QFile::remove(destinoFinal);
    if (!QFile::rename(destinoParcial, destinoFinal)) {
        r.msg = "Não foi possível finalizar o arquivo de backup na pasta.";
        concluir(r, false);
        return;
    }
    r.ok = true;
    r.arquivo = destinoFinal;
    r.tamanho = QFileInfo(destinoFinal).size();
    r.msg = "Backup concluído.";
    concluir(r, true);
}

void Backup_service::backupPostgres(const Config &cfg)
{
    Resultado r;
    const QString pgDump = localizarPgDump(cfg.pgDump);
    if (pgDump.isEmpty()) {
        r.msg = "O programa pg_dump não foi encontrado neste computador. Ele vem com o PostgreSQL "
                "(pasta bin, ex.: C:\\Program Files\\PostgreSQL\\16\\bin\\pg_dump.exe). "
                "Informe o caminho dele na tela de Backup.";
        concluir(r, false);
        return;
    }
    const QSqlDatabase db = DatabaseConnection_service::db();
    processo = new QProcess(this);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PGPASSWORD", db.password()); // só no ambiente deste processo; nunca em log
    env.insert("PGCONNECT_TIMEOUT", "15");
    processo->setProcessEnvironment(env);
    processo->setProgram(pgDump);
    processo->setArguments(argumentosPgDump(db.hostName(), db.port() > 0 ? db.port() : 5432, db.userName(),
                                            db.databaseName(), destinoParcial));

    connect(processo, &QProcess::readyReadStandardError, this, [this]() {
        if (erroProcesso.size() < 4000)
            erroProcesso += QString::fromLocal8Bit(processo->readAllStandardError());
    });
    auto *limite = new QTimer(processo);
    limite->setSingleShot(true);
    connect(limite, &QTimer::timeout, processo, &QProcess::kill);
    limite->start(kLimiteMinutosPgDump * 60 * 1000);

    connect(processo, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart)
            return;
        QFile::remove(destinoParcial);
        Resultado f;
        f.msg = "Não foi possível executar o pg_dump: " + processo->errorString();
        concluir(f, false);
    });
    connect(processo, &QProcess::finished, this, [this](int codigo, QProcess::ExitStatus status) {
        Resultado f;
        const bool arquivoOk = QFileInfo(destinoParcial).size() > 0;
        if (status != QProcess::NormalExit || codigo != 0 || !arquivoOk) {
            QFile::remove(destinoParcial);
            QString detalhe = erroProcesso.trimmed();
            if (detalhe.contains("version mismatch", Qt::CaseInsensitive))
                detalhe = "o pg_dump é de uma versão mais antiga que a do servidor PostgreSQL. Use o pg_dump da mesma versão do servidor.";
            else if (detalhe.contains("password authentication failed", Qt::CaseInsensitive))
                detalhe = "senha do banco recusada.";
            f.msg = "O pg_dump falhou" + (detalhe.isEmpty() ? QString() : ": " + detalhe.left(600));
            concluir(f, false);
            return;
        }
        QFile::remove(destinoFinal);
        if (!QFile::rename(destinoParcial, destinoFinal)) {
            f.msg = "Não foi possível finalizar o arquivo de backup na pasta.";
            concluir(f, false);
            return;
        }
        f.ok = true;
        f.arquivo = destinoFinal;
        f.tamanho = QFileInfo(destinoFinal).size();
        f.msg = "Backup concluído.";
        concluir(f, true);
    });
    processo->start();
}

// ------------------------------------------------------------------ agendamento
void Backup_service::iniciar()
{
    if (!relogio) {
        relogio = new QTimer(this);
        connect(relogio, &QTimer::timeout, this, &Backup_service::verificarAgendamento);
    }
    relogio->start(60 * 1000);
    QTimer::singleShot(30 * 1000, this, &Backup_service::verificarAgendamento); // logo depois de abrir
}

void Backup_service::parar()
{
    if (relogio)
        relogio->stop();
}

void Backup_service::verificarAgendamento()
{
    const Config cfg = carregar();
    if (!cfg.automatico || emAndamento())
        return;
    const QDateTime agora = QDateTime::currentDateTime();
    if (agora.time() < cfg.hora)
        return;
    const QDateTime ultimo = QDateTime::fromString(ultimoOk(), "yyyy-MM-dd HH:mm:ss");
    if (ultimo.isValid() && ultimo.date() == agora.date())
        return; // já fez hoje
    // depois de uma falha, espera 30 minutos para tentar de novo (não fica martelando)
    const QDateTime ultimaFalha = QDateTime::fromString(ini().value("backup/ultimo_erro_em").toString(), "yyyy-MM-dd HH:mm:ss");
    if (ultimaFalha.isValid() && ultimaFalha.secsTo(agora) < 30 * 60 && ultimaFalha.date() == agora.date())
        return;
    origemAutomatica = true;
    if (!iniciarBackup())
        origemAutomatica = false;
}
