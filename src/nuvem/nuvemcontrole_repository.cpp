#include "nuvemcontrole_repository.h"
#include "../infra/databaseconnection_service.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>

namespace {
QString agora() { return QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"); }
}

NuvemControle_repository::NuvemControle_repository()
    : db(DatabaseConnection_service::db())
{
}

bool NuvemControle_repository::adquirirTrava(const QString &terminal, int validadeSegundos)
{
    if (!DatabaseConnection_service::open())
        return false;
    const QString now = agora();
    const QString expira = QDateTime::currentDateTime().addSecs(validadeSegundos).toString("yyyy-MM-dd HH:mm:ss");
    QSqlQuery q(db);
    // pega a trava se ela é minha ou se já venceu (o dono parou de renovar)
    q.prepare("UPDATE sync_trava SET terminal = :t, expira_em = :exp, atualizado_em = :now "
              "WHERE id = 1 AND (terminal = :t2 OR expira_em < :now2)");
    q.bindValue(":t", terminal);
    q.bindValue(":exp", expira);
    q.bindValue(":now", now);
    q.bindValue(":t2", terminal);
    q.bindValue(":now2", now);
    if (!q.exec()) {
        qDebug() << "adquirirTrava:" << q.lastError().text();
        return false;
    }
    return q.numRowsAffected() == 1;
}

void NuvemControle_repository::liberarTrava(const QString &terminal)
{
    if (!DatabaseConnection_service::open())
        return;
    QSqlQuery q(db);
    q.prepare("UPDATE sync_trava SET terminal = NULL, expira_em = '2000-01-01 00:00:00' WHERE id = 1 AND terminal = :t");
    q.bindValue(":t", terminal);
    q.exec();
}

QString NuvemControle_repository::donoDaTrava()
{
    if (!DatabaseConnection_service::open())
        return QString();
    QSqlQuery q(db);
    q.prepare("SELECT terminal FROM sync_trava WHERE id = 1 AND expira_em >= :now");
    q.bindValue(":now", agora());
    return (q.exec() && q.next()) ? q.value(0).toString() : QString();
}

QString NuvemControle_repository::estado(const QString &chave, const QString &padrao)
{
    if (!DatabaseConnection_service::open())
        return padrao;
    QSqlQuery q(db);
    q.prepare("SELECT valor FROM sync_estado WHERE chave = :c");
    q.bindValue(":c", chave);
    return (q.exec() && q.next()) ? q.value(0).toString() : padrao;
}

void NuvemControle_repository::definirEstado(const QString &chave, const QString &valor)
{
    if (!DatabaseConnection_service::open())
        return;
    QSqlQuery up(db);
    up.prepare("UPDATE sync_estado SET valor = :v WHERE chave = :c");
    up.bindValue(":v", valor);
    up.bindValue(":c", chave);
    if (up.exec() && up.numRowsAffected() > 0)
        return;
    QSqlQuery ins(db);
    ins.prepare("INSERT INTO sync_estado (chave, valor) VALUES (:c, :v)");
    ins.bindValue(":c", chave);
    ins.bindValue(":v", valor);
    ins.exec();
}

void NuvemControle_repository::registrarLog(const QString &nivel, const QString &mensagem)
{
    if (!DatabaseConnection_service::open())
        return;
    QSqlQuery q(db);
    q.prepare("INSERT INTO sync_log (quando, nivel, mensagem) VALUES (:q, :n, :m)");
    q.bindValue(":q", agora());
    q.bindValue(":n", nivel);
    q.bindValue(":m", mensagem.left(2000));
    q.exec();
}

QList<LinhaLogNuvem> NuvemControle_repository::ultimosLogs(int limite)
{
    QList<LinhaLogNuvem> lista;
    if (!DatabaseConnection_service::open())
        return lista;
    QSqlQuery q(db);
    q.prepare("SELECT quando, nivel, mensagem FROM sync_log ORDER BY id DESC LIMIT :l");
    q.bindValue(":l", limite);
    if (q.exec())
        while (q.next())
            lista.append({q.value(0).toString(), q.value(1).toString(), q.value(2).toString()});
    return lista;
}

void NuvemControle_repository::limparLogsAntigos(int manterDias)
{
    if (!DatabaseConnection_service::open())
        return;
    QSqlQuery q(db);
    q.prepare("DELETE FROM sync_log WHERE quando < :limite");
    q.bindValue(":limite", QDateTime::currentDateTime().addDays(-manterDias).toString("yyyy-MM-dd HH:mm:ss"));
    q.exec();
}

PedidoAplicadoDTO NuvemControle_repository::pedido(const QString &idPedido)
{
    PedidoAplicadoDTO p;
    if (!DatabaseConnection_service::open())
        return p;
    QSqlQuery q(db);
    q.prepare("SELECT id_pedido, tipo, status, mensagem, detalhe, aplicado_em, enviado "
              "FROM sync_pedidos_aplicados WHERE id_pedido = :id");
    q.bindValue(":id", idPedido);
    if (q.exec() && q.next()) {
        p.idPedido = q.value(0).toString();
        p.tipo = q.value(1).toString();
        p.status = q.value(2).toString();
        p.mensagem = q.value(3).toString();
        p.detalhe = q.value(4).toString();
        p.aplicadoEm = q.value(5).toString();
        p.enviado = q.value(6).toInt() != 0;
    }
    return p;
}

bool NuvemControle_repository::registrarPedidoAplicado(const PedidoAplicadoDTO &p)
{
    if (!DatabaseConnection_service::open())
        return false;
    QSqlQuery q(db);
    q.prepare("INSERT INTO sync_pedidos_aplicados (id_pedido, tipo, status, mensagem, detalhe, aplicado_em, enviado) "
              "VALUES (:id, :tipo, :status, :msg, :det, :em, 0)");
    q.bindValue(":id", p.idPedido);
    q.bindValue(":tipo", p.tipo);
    q.bindValue(":status", p.status);
    q.bindValue(":msg", p.mensagem);
    q.bindValue(":det", p.detalhe);
    q.bindValue(":em", agora());
    if (!q.exec()) {
        qDebug() << "registrarPedidoAplicado:" << q.lastError().text();
        return false;
    }
    return true;
}

QList<PedidoAplicadoDTO> NuvemControle_repository::pedidosNaoEnviados()
{
    QList<PedidoAplicadoDTO> lista;
    if (!DatabaseConnection_service::open())
        return lista;
    QSqlQuery q(db);
    if (!q.exec("SELECT id_pedido FROM sync_pedidos_aplicados WHERE enviado = 0 ORDER BY aplicado_em"))
        return lista;
    QStringList ids;
    while (q.next())
        ids << q.value(0).toString();
    for (const QString &id : ids)
        lista.append(pedido(id));
    return lista;
}

void NuvemControle_repository::marcarPedidoEnviado(const QString &idPedido)
{
    if (!DatabaseConnection_service::open())
        return;
    QSqlQuery q(db);
    q.prepare("UPDATE sync_pedidos_aplicados SET enviado = 1 WHERE id_pedido = :id");
    q.bindValue(":id", idPedido);
    q.exec();
}
