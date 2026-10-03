#ifndef NUVEMCONTROLE_REPOSITORY_H
#define NUVEMCONTROLE_REPOSITORY_H

#include <QDateTime>
#include <QList>
#include <QSqlDatabase>
#include <QString>

struct PedidoAplicadoDTO {
    QString idPedido;
    QString tipo;
    QString status;           // APLICADO | RECUSADO | CONFLITO | EXPIRADO
    QString mensagem;
    QString detalhe;          // JSON
    QString aplicadoEm;
    bool enviado = false;
    bool existe() const { return !idPedido.isEmpty(); }
};

struct LinhaLogNuvem {
    QString quando;
    QString nivel;
    QString mensagem;
};

// Tabelas de controle do módulo Nuvem (migração 23): estado, trava de "quem sincroniza", histórico e pedidos
// do painel já aplicados.
class NuvemControle_repository
{
public:
    NuvemControle_repository();

    // ---- trava: só um computador sincroniza por vez. Uma única UPDATE condicional (atômica).
    bool adquirirTrava(const QString &terminal, int validadeSegundos);
    void liberarTrava(const QString &terminal);
    QString donoDaTrava();

    // ---- estado (chave/valor)
    QString estado(const QString &chave, const QString &padrao = QString());
    void definirEstado(const QString &chave, const QString &valor);

    // ---- histórico
    void registrarLog(const QString &nivel, const QString &mensagem);
    QList<LinhaLogNuvem> ultimosLogs(int limite = 100);
    void limparLogsAntigos(int manterDias = 30);

    // ---- pedidos do painel
    PedidoAplicadoDTO pedido(const QString &idPedido);
    bool registrarPedidoAplicado(const PedidoAplicadoDTO &pedido);
    QList<PedidoAplicadoDTO> pedidosNaoEnviados();
    void marcarPedidoEnviado(const QString &idPedido);

private:
    QSqlDatabase db;
};

#endif // NUVEMCONTROLE_REPOSITORY_H
