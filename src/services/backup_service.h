#ifndef BACKUP_SERVICE_H
#define BACKUP_SERVICE_H

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTime>

class QProcess;
class QTimer;

// Backup do banco de dados para uma pasta escolhida (por exemplo, uma pasta sincronizada pelo MEGA).
//  - SQLite: cópia consistente do arquivo (VACUUM INTO).
//  - PostgreSQL: pg_dump em formato compacto (.backup), restaurável com pg_restore.
// O arquivo só ganha o nome final quando está completo (antes é ".parte"), então um serviço de
// sincronização como o MEGA nunca envia um backup pela metade. A senha do banco vai ao pg_dump
// por variável de ambiente, nunca na linha de comando nem em log.
class Backup_service : public QObject
{
    Q_OBJECT
public:
    struct Config {
        QString pasta;            // onde salvar
        int manter = 14;          // quantos backups guardar (os mais antigos são apagados)
        bool automatico = false;  // fazer todo dia, neste computador
        QTime hora = QTime(2, 0); // a partir de que hora do dia
        QString pgDump;           // caminho do pg_dump.exe (vazio = procurar sozinho)
    };
    struct Resultado {
        bool ok = false;
        QString msg;
        QString arquivo;
        qint64 tamanho = 0;
        bool automatico = false; // veio do agendamento diário (e não do botão)
    };
    struct Arquivo {
        QString nome;
        QDateTime quando;
        qint64 tamanho = 0;
    };

    static Backup_service *instancia();

    // configuração (seção [backup] do config.ini)
    // Em testes: usa outro arquivo .ini (o config.ini do usuário nunca é tocado).
    static void usarArquivoDeConfiguracaoParaTeste(const QString &caminhoIni);

    static Config carregar();
    static bool salvar(const Config &cfg);
    static QString ultimoOk();         // "yyyy-MM-dd HH:mm:ss" do último backup concluído; vazio se nunca
    static QString ultimoErro();       // texto do último erro; vazio se o último deu certo

    // ---- partes sem efeito colateral (testáveis) ----
    static QString nomeDoArquivo(const QDateTime &quando, bool postgres);
    // vazio se a pasta serve (existe ou pode ser criada, e dá para gravar); senão, o motivo
    static QString validarPasta(const QString &pasta);
    // apaga os backups mais antigos além de "manter"; só mexe em arquivos com o nosso padrão de nome
    static int aplicarRetencao(const QString &pasta, int manter);
    static QList<Arquivo> listar(const QString &pasta);           // mais novos primeiro
    static QStringList argumentosPgDump(const QString &host, int porta, const QString &usuario,
                                        const QString &banco, const QString &arquivo);
    static QString localizarPgDump(const QString &configurado);   // vazio se não achou

    // Faz o backup agora. O resultado chega no sinal terminou(). Se já houver um em andamento,
    // devolve false.
    bool iniciarBackup();
    bool emAndamento() const { return processo != nullptr || ocupado; }

    // Agenda o backup automático (só age se "automatico" estiver ligado neste computador).
    void iniciar();
    void parar();

signals:
    void terminou(const Backup_service::Resultado &resultado);

private:
    explicit Backup_service(QObject *parent = nullptr);
    void concluir(const Resultado &r, bool manterAntigos);
    void backupSqlite(const Config &cfg);
    void backupPostgres(const Config &cfg);
    void verificarAgendamento();

    QTimer *relogio = nullptr;
    QProcess *processo = nullptr;
    bool ocupado = false;
    bool origemAutomatica = false;
    QString destinoParcial;
    QString destinoFinal;
    QString erroProcesso;
};

Q_DECLARE_METATYPE(Backup_service::Resultado)

#endif // BACKUP_SERVICE_H
