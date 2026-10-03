#ifndef NUVEM_SERVICE_H
#define NUVEM_SERVICE_H

#include <QDateTime>
#include <QObject>
#include <QString>
#include <memory>
#include "nuvemconfig.h"
#include "nuvemcontrole_repository.h"

class QTimer;
class NuvemClient;

// Módulo Nuvem: mantém o painel online em dia com a loja e executa os pedidos que o painel faz.
//
// Um ciclo (sempre nesta ordem, para uma cópia antiga nunca desfazer uma ação recém-aplicada):
//   1. avisa que a loja está online (heartbeat);
//   2. reenvia resultados de pedidos que ainda não chegaram à nuvem;
//   3. busca pedidos pendentes e aplica cada um pelos serviços do sistema;
//   4. se chegou a hora (ou se aplicou algum pedido), envia a foto completa dos dados: abre o ciclo, manda os
//      lotes e conclui; a API só troca o espelho no fim, de uma vez.
//
// Só um computador sincroniza por vez (trava no banco, renovada a cada ciclo).
class Nuvem_service : public QObject
{
    Q_OBJECT
public:
    struct ResultadoCiclo {
        bool executou = false;        // false: nada a fazer (desligada, outro computador sincroniza, já em execução)
        bool ok = false;
        QString mensagem;
        int pedidosAplicados = 0;
        qint64 linhasEnviadas = 0;
    };

    struct Status {
        bool ativa = false;
        bool emExecucao = false;
        QString ultimoCicloOk;        // yyyy-MM-dd HH:mm:ss
        QString ultimoEnvioOk;
        QString ultimoErro;
        QString ultimoResumo;
        QString donoDaTrava;
    };

    static Nuvem_service *instancia();
    explicit Nuvem_service(QObject *parent = nullptr);
    ~Nuvem_service() override;

    // liga o relógio (um tique por minuto). Sem efeito se a Nuvem estiver desligada neste computador.
    void iniciar();
    void parar();
    // abandona a chamada em andamento (o programa vai fechar)
    void abortar();

    ResultadoCiclo executar(bool comDados = true);
    ResultadoCiclo testarConexao();
    Status status();

    // para testes
    void definirConfigParaTeste(const NuvemConfig &cfg);
    void definirTerminalParaTeste(const QString &terminal);
    void definirTamanhoLoteParaTeste(int linhas) { linhasPorLote = linhas; }

signals:
    void statusMudou();

private:
    NuvemControle_repository controle;
    QTimer *relogio = nullptr;
    bool emExecucao = false;
    bool abortado = false;
    int linhasPorLote = 400;
    std::unique_ptr<NuvemConfig> configTeste;
    QString terminalTeste;
    NuvemClient *clienteAtual = nullptr;

    NuvemConfig configAtual() const;
    QString terminal() const;
    void tique();
    void falhar(ResultadoCiclo &r, const QString &mensagem);
    int processarPedidos(NuvemClient &cliente, QString *erro);
    bool enviarDados(NuvemClient &cliente, qint64 *linhas, QString *erro);
};

#endif // NUVEM_SERVICE_H
