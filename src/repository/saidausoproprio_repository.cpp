#include "saidausoproprio_repository.h"
#include "../infra/databaseconnection_service.h"
#include "../util/datautil.h"
#include <QDebug>
#include <QMetaType>
#include <QSqlError>
#include <QSqlQuery>

namespace {
void setErro(QString *erro, const QString &t) { if (erro) *erro = t; }
QVariant nuloInt() { return QVariant(QMetaType(QMetaType::LongLong)); }
}

SaidaUsoProprio_repository::SaidaUsoProprio_repository()
    : db(DatabaseConnection_service::db())
{
}

qlonglong SaidaUsoProprio_repository::registrar(const SaidaUsoProprioDTO &s, bool baixarEstoque, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return -1;
    }
    if (!db.transaction()) {
        setErro(erro, "Não foi possível iniciar a gravação: " + db.lastError().text());
        return -1;
    }

    if (baixarEstoque) {
        QSqlQuery baixa(db);
        baixa.prepare("UPDATE produtos SET quantidade = quantidade - :q, atualizado_em = :agora "
                      "WHERE id = :id AND quantidade >= :q");
        baixa.bindValue(":q", s.quantidade);
        baixa.bindValue(":agora", DataUtil::getDataAgoraUS());
        baixa.bindValue(":id", s.idProduto);
        if (!baixa.exec()) {
            db.rollback();
            setErro(erro, baixa.lastError().text());
            return -1;
        }
        if (baixa.numRowsAffected() != 1) {
            db.rollback();
            setErro(erro, "Estoque insuficiente: outra tela pode ter dado baixa neste produto agora há pouco.");
            return -1;
        }
    }

    QSqlQuery ins(db);
    ins.prepare("INSERT INTO saidas_uso_proprio (id_produto, descricao_produto, quantidade, motivo, observacao, "
                "custo_unitario, preco_venda, id_operador_sessao, nome_operador, data_hora) "
                "VALUES (:prod, :desc, :q, :motivo, :obs, :custo, :preco, :op, :nome, :data)");
    ins.bindValue(":prod", s.idProduto);
    ins.bindValue(":desc", s.descricaoProduto);
    ins.bindValue(":q", s.quantidade);
    ins.bindValue(":motivo", s.motivo);
    ins.bindValue(":obs", s.observacao);
    ins.bindValue(":custo", s.custoUnitario);
    ins.bindValue(":preco", s.precoVenda);
    ins.bindValue(":op", s.idOperadorSessao >= 0 ? QVariant(s.idOperadorSessao) : nuloInt());
    ins.bindValue(":nome", s.nomeOperador);
    ins.bindValue(":data", DataUtil::getDataAgoraUS());
    if (!ins.exec()) {
        db.rollback();
        setErro(erro, ins.lastError().text());
        return -1;
    }
    QVariant id = ins.lastInsertId();
    if (!id.isValid()) {
        QSqlQuery q2(db);
        if (q2.exec("SELECT MAX(id) FROM saidas_uso_proprio") && q2.next())
            id = q2.value(0);
    }
    if (!db.commit()) {
        db.rollback();
        setErro(erro, "Não foi possível confirmar a gravação: " + db.lastError().text());
        return -1;
    }
    return id.toLongLong();
}

SaidaUsoProprioDTO SaidaUsoProprio_repository::obter(qlonglong id)
{
    SaidaUsoProprioDTO s;
    if (!DatabaseConnection_service::open())
        return s;
    QSqlQuery q(db);
    q.prepare("SELECT id, id_produto, descricao_produto, quantidade, motivo, observacao, custo_unitario, "
              "preco_venda, id_operador_sessao, nome_operador, data_hora FROM saidas_uso_proprio WHERE id = :id");
    q.bindValue(":id", id);
    if (q.exec() && q.next()) {
        s.id = q.value(0).toLongLong();
        s.idProduto = q.value(1).toLongLong();
        s.descricaoProduto = q.value(2).toString();
        s.quantidade = q.value(3).toDouble();
        s.motivo = q.value(4).toString();
        s.observacao = q.value(5).toString();
        s.custoUnitario = q.value(6).toDouble();
        s.precoVenda = q.value(7).toDouble();
        s.idOperadorSessao = q.value(8).isNull() ? -1 : q.value(8).toLongLong();
        s.nomeOperador = q.value(9).toString();
        s.dataHora = q.value(10).toString();
    }
    return s;
}

bool SaidaUsoProprio_repository::remover(qlonglong id, bool devolverEstoque, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    if (!db.transaction()) {
        setErro(erro, "Não foi possível iniciar a remoção: " + db.lastError().text());
        return false;
    }
    QSqlQuery sel(db);
    sel.prepare("SELECT id_produto, quantidade FROM saidas_uso_proprio WHERE id = :id");
    sel.bindValue(":id", id);
    if (!sel.exec() || !sel.next()) {
        db.rollback();
        setErro(erro, "Esta saída não existe mais (talvez já tenha sido removida em outra tela).");
        return false;
    }
    const qlonglong idProduto = sel.value(0).toLongLong();
    const double quantidade = sel.value(1).toDouble();
    sel.finish();

    QSqlQuery del(db);
    del.prepare("DELETE FROM saidas_uso_proprio WHERE id = :id");
    del.bindValue(":id", id);
    if (!del.exec() || del.numRowsAffected() != 1) {
        db.rollback();
        setErro(erro, del.lastError().isValid() ? del.lastError().text()
                                                : QStringLiteral("Esta saída já foi removida em outra tela."));
        return false;
    }

    if (devolverEstoque) {
        QSqlQuery volta(db);
        volta.prepare("UPDATE produtos SET quantidade = quantidade + :q, atualizado_em = :agora WHERE id = :id");
        volta.bindValue(":q", quantidade);
        volta.bindValue(":agora", DataUtil::getDataAgoraUS());
        volta.bindValue(":id", idProduto);
        if (!volta.exec()) {
            db.rollback();
            setErro(erro, volta.lastError().text());
            return false;
        }
    }
    if (!db.commit()) {
        db.rollback();
        setErro(erro, "Não foi possível confirmar a remoção: " + db.lastError().text());
        return false;
    }
    return true;
}

QList<SaidaUsoProprioDTO> SaidaUsoProprio_repository::listar(const QDate &de, const QDate &ate, const QString &motivo)
{
    QList<SaidaUsoProprioDTO> lista;
    if (!DatabaseConnection_service::open())
        return lista;
    QString sql = "SELECT id, id_produto, descricao_produto, quantidade, motivo, observacao, custo_unitario, "
                  "preco_venda, id_operador_sessao, nome_operador, data_hora FROM saidas_uso_proprio "
                  "WHERE data_hora >= :de AND data_hora < :ate";
    if (!motivo.isEmpty())
        sql += " AND motivo = :motivo";
    sql += " ORDER BY data_hora DESC, id DESC";
    QSqlQuery q(db);
    q.prepare(sql);
    q.bindValue(":de", de.toString("yyyy-MM-dd") + " 00:00:00");
    q.bindValue(":ate", ate.addDays(1).toString("yyyy-MM-dd") + " 00:00:00");
    if (!motivo.isEmpty())
        q.bindValue(":motivo", motivo);
    if (!q.exec()) {
        qDebug() << "Erro ao listar saidas_uso_proprio:" << q.lastError().text();
        return lista;
    }
    while (q.next()) {
        SaidaUsoProprioDTO s;
        s.id = q.value(0).toLongLong();
        s.idProduto = q.value(1).toLongLong();
        s.descricaoProduto = q.value(2).toString();
        s.quantidade = q.value(3).toDouble();
        s.motivo = q.value(4).toString();
        s.observacao = q.value(5).toString();
        s.custoUnitario = q.value(6).toDouble();
        s.precoVenda = q.value(7).toDouble();
        s.idOperadorSessao = q.value(8).isNull() ? -1 : q.value(8).toLongLong();
        s.nomeOperador = q.value(9).toString();
        s.dataHora = q.value(10).toString();
        lista << s;
    }
    return lista;
}
