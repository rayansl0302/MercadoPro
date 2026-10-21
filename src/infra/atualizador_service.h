#ifndef ATUALIZADOR_SERVICE_H
#define ATUALIZADOR_SERVICE_H

#include <QObject>
#include <QUrl>
#include <functional>
#include <optional>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

// Auto-update silencioso: ao abrir (e a cada algumas horas) consulta, em segundo plano, a última
// versão publicada no GitHub Releases. Se houver uma mais nova, baixa o instalador, confere o hash
// SHA-256 e o executa em modo silencioso num momento ocioso (sem venda em andamento).
//
// Regras de segurança/robustez:
//  - só HTTPS e só hosts do GitHub (também nos redirecionamentos);
//  - o instalador só roda se o SHA-256 bater com o publicado junto na mesma release;
//  - tudo com tempo limite; qualquer falha (offline, DNS, 403...) só vira log, nunca trava nem
//    mostra erro ao operador, e o PDV segue funcionando na versão atual;
//  - no máximo 2 tentativas por versão (um instalador que falha não entra em laço);
//  - desligável em config.ini: [atualizacao] ativa=0.
class Atualizador_service : public QObject
{
    Q_OBJECT
public:
    struct Release {
        QString versao;        // "1.0.12" (sem o "v" do tag)
        QUrl urlInstalador;    // MercadoPro-Setup-1.0.12.exe
        QUrl urlHash;          // MercadoPro-Setup-1.0.12.exe.sha256
        qint64 tamanho = 0;
    };

    static Atualizador_service *instancia();

    // ---- partes puras (testáveis sem rede) ----
    static QString versaoAtual();                                   // versão compilada (MERCADOPRO_VERSAO)
    static int compararVersoes(const QString &a, const QString &b); // <0 se a<b, 0 se iguais, >0 se a>b ("1.0.10" > "1.0.9")
    static std::optional<Release> interpretarRelease(const QByteArray &json);
    static QString hashDoTexto(const QString &conteudoDoArquivoSha256); // extrai os 64 hex ("<hash>  arquivo")
    static QString sha256DoArquivo(const QString &caminho);          // hex minúsculo; vazio se não leu
    static bool urlPermitida(const QUrl &url);                       // https + host do GitHub

    // Configuração. Em testes, aponta a API para um servidor local.
    static void definirUrlApiParaTeste(const QUrl &url);

    // Inicia o ciclo: 1ª verificação ~20 s depois de abrir, depois a cada 6 h. podeAplicar diz se é
    // seguro instalar agora (sem venda aberta nem diálogo modal).
    void iniciar(std::function<bool()> podeAplicar);
    void parar();
    // Uma verificação imediata. manual=true (menu Ajuda > Verificar atualizações) ignora o "ativa=0" do
    // config.ini e, se a atualização já estiver baixada, avisa de novo em vez de ficar calado.
    void verificarAgora(bool manual = false);
    // Executa o instalador agora (o operador confirmou). Retorna false se não houver nada pronto.
    bool aplicarAgora();
    bool atualizacaoBaixada() const { return !instaladorPronto.isEmpty(); }
    void resetarEstadoParaTeste();

signals:
    void semAtualizacao();
    void atualizacaoPronta(const QString &versao, const QString &caminhoInstalador);
    void falhou(const QString &motivo);   // informativo (log); não é mostrado ao operador

private:
    explicit Atualizador_service(QObject *parent = nullptr);
    bool ativo() const;
    bool manualPermitido() const;
    bool executarInstalador();
    void baixar(const Release &release);
    void tentarAplicar();
    void falha(const QString &motivo);

    QNetworkAccessManager *rede = nullptr;
    QTimer *relogio = nullptr;
    QTimer *aplicador = nullptr;
    std::function<bool()> podeAplicar;
    bool emAndamento = false;
    QString versaoPronta;
    QString instaladorPronto;
};

#endif // ATUALIZADOR_SERVICE_H
