#include "caixa_repository.h"
#include "../infra/databaseconnection_service.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDebug>
#include <QMetaType>

namespace {
void setErro(QString *erro, const QString &texto)
{
    if (erro)
        *erro = texto;
}

const char *SELECT_CAIXA =
    "SELECT c.*, o.nome AS nome_operador FROM caixas c "
    "LEFT JOIN operadores o ON o.id = c.id_operador ";
}

Caixa_repository::Caixa_repository(QObject *parent)
    : QObject{parent}
{
    db = DatabaseConnection_service::db();
}

CaixaDTO Caixa_repository::lerCaixa(QSqlQuery &query)
{
    CaixaDTO c;
    c.id = query.value("id").toLongLong();
    c.idOperador = query.value("id_operador").toLongLong();
    c.nomeOperador = query.value("nome_operador").toString();
    c.terminal = query.value("terminal").toString();
    c.status = query.value("status").toString();
    c.abertoEm = query.value("aberto_em").toDateTime().toString(Qt::ISODate);
    c.fechadoEm = query.value("fechado_em").isNull()
                      ? QString() : query.value("fechado_em").toDateTime().toString(Qt::ISODate);
    c.trocoInicial = query.value("troco_inicial").toDouble();
    c.trocoSugerido = query.value("troco_sugerido").toDouble();
    c.observacaoFechamento = query.value("observacao_fechamento").toString();
    c.reabertoPor = query.value("reaberto_por").toLongLong();
    c.motivoReabertura = query.value("motivo_reabertura").toString();
    c.reabertoEm = query.value("reaberto_em").isNull()
                       ? QString() : query.value("reaberto_em").toDateTime().toString(Qt::ISODate);
    return c;
}

MovimentacaoCaixaDTO Caixa_repository::lerMovimentacao(QSqlQuery &query)
{
    MovimentacaoCaixaDTO m;
    m.id = query.value("id").toLongLong();
    m.idCaixa = query.value("id_caixa").toLongLong();
    m.tipo = query.value("tipo").toString();
    m.valor = query.value("valor").toDouble();
    m.formaPagamento = query.value("forma_pagamento").toString();
    m.motivo = query.value("motivo").toString();
    m.idVenda = query.value("id_venda").toLongLong();
    m.idEntradaVenda = query.value("id_entrada_venda").toLongLong();
    m.idOperador = query.value("id_operador").toLongLong();
    m.nomeOperador = query.value("nome_operador").toString();
    m.estornado = query.value("estornado").toBool();
    m.dataHora = query.value("data_hora").toDateTime().toString(Qt::ISODate);
    return m;
}

qlonglong Caixa_repository::abrir(const CaixaDTO &caixa, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return -1;
    }
    QSqlQuery query(db);
    query.prepare("INSERT INTO caixas (id_operador, terminal, status, aberto_em, troco_inicial, troco_sugerido) "
                  "VALUES (:op, :terminal, 'ABERTO', :agora, :troco, :sugerido)");
    query.bindValue(":op", caixa.idOperador);
    query.bindValue(":terminal", caixa.terminal);
    query.bindValue(":agora", QDateTime::currentDateTime());
    query.bindValue(":troco", caixa.trocoInicial);
    query.bindValue(":sugerido", caixa.trocoSugerido);
    if (!query.exec()) {
        setErro(erro, query.lastError().text());
        qDebug() << "abrir caixa falhou:" << query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toLongLong();
}

CaixaDTO Caixa_repository::getPorId(qlonglong id)
{
    CaixaDTO c;
    if (!DatabaseConnection_service::open())
        return c;
    QSqlQuery query(db);
    query.prepare(QString(SELECT_CAIXA) + "WHERE c.id = :id");
    query.bindValue(":id", id);
    if (!query.exec() || !query.next())
        return c;
    return lerCaixa(query);
}

CaixaDTO Caixa_repository::getAbertoNoTerminal(const QString &terminal)
{
    CaixaDTO c;
    if (!DatabaseConnection_service::open())
        return c;
    QSqlQuery query(db);
    query.prepare(QString(SELECT_CAIXA) + "WHERE c.status = 'ABERTO' AND c.terminal = :t "
                                          "ORDER BY c.id DESC LIMIT 1");
    query.bindValue(":t", terminal);
    if (!query.exec() || !query.next())
        return c;
    return lerCaixa(query);
}

CaixaDTO Caixa_repository::getAbertoDoOperador(qlonglong idOperador)
{
    CaixaDTO c;
    if (!DatabaseConnection_service::open())
        return c;
    QSqlQuery query(db);
    query.prepare(QString(SELECT_CAIXA) + "WHERE c.status = 'ABERTO' AND c.id_operador = :op "
                                          "ORDER BY c.id DESC LIMIT 1");
    query.bindValue(":op", idOperador);
    if (!query.exec() || !query.next())
        return c;
    return lerCaixa(query);
}

bool Caixa_repository::getTrocoProximo(const QString &terminal, double *valor)
{
    if (!DatabaseConnection_service::open())
        return false;
    QSqlQuery query(db);
    query.prepare("SELECT valor FROM config_caixa WHERE chave = :c");
    query.bindValue(":c", "troco_proximo_" + terminal);
    if (!query.exec() || !query.next())
        return false;
    bool ok = false;
    const double v = query.value(0).toString().toDouble(&ok);
    if (!ok)
        return false;
    if (valor)
        *valor = v;
    return true;
}

bool Caixa_repository::setTrocoProximo(const QString &terminal, double valor)
{
    if (!DatabaseConnection_service::open())
        return false;
    const QString chave = "troco_proximo_" + terminal;
    QSqlQuery up(db);
    up.prepare("UPDATE config_caixa SET valor = :v WHERE chave = :c");
    up.bindValue(":v", QString::number(valor, 'f', 2));
    up.bindValue(":c", chave);
    if (!up.exec())
        return false;
    if (up.numRowsAffected() > 0)
        return true;
    QSqlQuery ins(db);
    ins.prepare("INSERT INTO config_caixa (chave, valor) VALUES (:c, :v)");
    ins.bindValue(":c", chave);
    ins.bindValue(":v", QString::number(valor, 'f', 2));
    return ins.exec();
}

double Caixa_repository::getDinheiroInformadoUltimoFechamento(const QString &terminal)
{
    if (!DatabaseConnection_service::open())
        return 0;
    QSqlQuery query(db);
    query.prepare("SELECT f.valor_informado FROM fechamentos_caixa f "
                  "JOIN caixas c ON c.id = f.id_caixa "
                  "WHERE c.status = 'FECHADO' AND c.terminal = :t AND f.forma_pagamento = 'Dinheiro' "
                  "ORDER BY c.fechado_em DESC, c.id DESC LIMIT 1");
    query.bindValue(":t", terminal);
    if (!query.exec() || !query.next())
        return 0;
    return query.value(0).toDouble();
}

bool Caixa_repository::fechar(qlonglong idCaixa, const QString &observacao,
                              const QList<FechamentoFormaDTO> &formas, QString *erro,
                              int vendasConferidas, int movimentacoesConferidas)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    if (!db.transaction()) {
        setErro(erro, "Não foi possível iniciar a transação.");
        return false;
    }

    QSqlQuery query(db);
    query.prepare("UPDATE caixas SET status = 'FECHADO', fechado_em = :agora, observacao_fechamento = :obs "
                  "WHERE id = :id AND status = 'ABERTO'");
    query.bindValue(":agora", QDateTime::currentDateTime());
    query.bindValue(":obs", observacao);
    query.bindValue(":id", idCaixa);
    if (!query.exec() || query.numRowsAffected() != 1) {
        setErro(erro, query.lastError().isValid() ? query.lastError().text()
                                                  : "Este caixa já está fechado.");
        db.rollback();
        return false;
    }

    // o caixa já está FECHADO nesta transação: daqui em diante ninguém mais lança nele.
    // Conferir agora o que as contas usaram evita fechar com uma venda que entrou no meio.
    if (vendasConferidas >= 0 || movimentacoesConferidas >= 0) {
        QSqlQuery conta(db);
        conta.prepare("SELECT (SELECT COUNT(*) FROM vendas2 WHERE id_caixa = :id), "
                      "(SELECT COUNT(*) FROM movimentacoes_caixa WHERE id_caixa = :id2)");
        conta.bindValue(":id", idCaixa);
        conta.bindValue(":id2", idCaixa);
        if (!conta.exec() || !conta.next()) {
            setErro(erro, conta.lastError().text());
            db.rollback();
            return false;
        }
        const int vendasAgora = conta.value(0).toInt();
        const int movAgora = conta.value(1).toInt();
        if ((vendasConferidas >= 0 && vendasAgora != vendasConferidas) ||
            (movimentacoesConferidas >= 0 && movAgora != movimentacoesConferidas)) {
            setErro(erro, "Houve movimentação neste caixa durante o fechamento. "
                          "Confira os valores esperados e feche novamente.");
            db.rollback();
            return false;
        }
    }

    for (const FechamentoFormaDTO &f : formas) {
        QSqlQuery ins(db);
        ins.prepare("INSERT INTO fechamentos_caixa (id_caixa, forma_pagamento, valor_esperado, valor_informado, "
                    "diferenca, ocorrencia_tecnica) VALUES (:id, :forma, :esp, :inf, :dif, :tec)");
        ins.bindValue(":id", idCaixa);
        ins.bindValue(":forma", f.formaPagamento);
        ins.bindValue(":esp", f.valorEsperado);
        ins.bindValue(":inf", f.valorInformado);
        ins.bindValue(":dif", f.diferenca);
        ins.bindValue(":tec", f.ocorrenciaTecnica);
        if (!ins.exec()) {
            setErro(erro, ins.lastError().text());
            db.rollback();
            return false;
        }
    }

    if (!db.commit()) {
        setErro(erro, db.lastError().text());
        db.rollback();
        return false;
    }
    return true;
}

void Caixa_repository::listarHistorico(QSqlQueryModel *model, const QString &de, const QString &ate)
{
    if (!model || !DatabaseConnection_service::open())
        return;
    QString sql =
        "SELECT c.id AS id, o.nome AS operador, c.terminal AS terminal, c.status AS status, "
        "c.aberto_em AS aberto_em, c.fechado_em AS fechado_em, c.troco_inicial AS troco_inicial, "
        "(SELECT COALESCE(SUM(v.valor_final), 0) FROM vendas2 v WHERE v.id_caixa = c.id "
        "   AND v.forma_pagamento <> 'Prazo') AS total_vendas, "
        "(SELECT COALESCE(SUM(f.valor_informado), 0) FROM fechamentos_caixa f WHERE f.id_caixa = c.id) AS contado, "
        "(SELECT COALESCE(SUM(f.diferenca), 0) FROM fechamentos_caixa f WHERE f.id_caixa = c.id) AS diferenca "
        "FROM caixas c LEFT JOIN operadores o ON o.id = c.id_operador ";
    if (!de.isEmpty() && !ate.isEmpty())
        sql += "WHERE c.aberto_em >= :de AND c.aberto_em < :ate ";
    sql += "ORDER BY c.id DESC";

    QSqlQuery query(db);
    query.prepare(sql);
    if (!de.isEmpty() && !ate.isEmpty()) {
        query.bindValue(":de", de);
        query.bindValue(":ate", ate);
    }
    if (!query.exec()) {
        qDebug() << "listarHistorico caixas falhou:" << query.lastError().text();
        return;
    }
    model->setQuery(std::move(query));
}

qlonglong Caixa_repository::inserirMovimentacao(const MovimentacaoCaixaDTO &mov, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return -1;
    }
    QSqlQuery query(db);
    query.prepare("INSERT INTO movimentacoes_caixa (id_caixa, tipo, valor, forma_pagamento, motivo, id_venda, "
                  "id_entrada_venda, id_pagamento_divida, id_operador, id_operador_sessao, estornado, data_hora) "
                  "VALUES (:caixa, :tipo, :valor, :forma, :motivo, :venda, :entrada, :pagdiv, :op, :opsessao, :est, :agora)");
    query.bindValue(":pagdiv", mov.idPagamentoDivida > 0 ? QVariant(mov.idPagamentoDivida)
                                                          : QVariant(QMetaType(QMetaType::LongLong)));
    query.bindValue(":caixa", mov.idCaixa);
    query.bindValue(":tipo", mov.tipo);
    query.bindValue(":valor", mov.valor);
    query.bindValue(":forma", mov.formaPagamento.isEmpty() ? QVariant(QMetaType(QMetaType::QString))
                                                            : QVariant(mov.formaPagamento));
    query.bindValue(":motivo", mov.motivo);
    query.bindValue(":venda", mov.idVenda > 0 ? QVariant(mov.idVenda) : QVariant(QMetaType(QMetaType::LongLong)));
    query.bindValue(":entrada", mov.idEntradaVenda > 0 ? QVariant(mov.idEntradaVenda)
                                                       : QVariant(QMetaType(QMetaType::LongLong)));
    query.bindValue(":op", mov.idOperador > 0 ? QVariant(mov.idOperador) : QVariant(QMetaType(QMetaType::LongLong)));
    // quem praticou o movimento e o operador logado; id_operador continua sendo o dono do caixa
    query.bindValue(":opsessao", mov.idOperadorSessao >= 0 ? QVariant(mov.idOperadorSessao)
                                                          : QVariant(QMetaType(QMetaType::LongLong)));
    query.bindValue(":est", mov.estornado);
    query.bindValue(":agora", QDateTime::currentDateTime());
    if (!query.exec()) {
        setErro(erro, query.lastError().text());
        qDebug() << "inserir movimentacao caixa falhou:" << query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toLongLong();
}

MovimentacaoCaixaDTO Caixa_repository::getMovimentacaoPorEntradaVenda(qlonglong idEntradaVenda)
{
    MovimentacaoCaixaDTO m;
    if (!DatabaseConnection_service::open())
        return m;
    QSqlQuery query(db);
    query.prepare("SELECT m.*, o.nome AS nome_operador FROM movimentacoes_caixa m "
                  "LEFT JOIN operadores o ON o.id = m.id_operador "
                  "WHERE m.id_entrada_venda = :id AND m.tipo = 'RECEBIMENTO' ORDER BY m.id DESC LIMIT 1");
    query.bindValue(":id", idEntradaVenda);
    if (!query.exec() || !query.next())
        return m;
    return lerMovimentacao(query);
}

MovimentacaoCaixaDTO Caixa_repository::getMovimentacaoPorPagamentoDivida(qlonglong idPagamentoDivida)
{
    MovimentacaoCaixaDTO m;
    if (!DatabaseConnection_service::open())
        return m;
    QSqlQuery query(db);
    query.prepare("SELECT m.*, o.nome AS nome_operador FROM movimentacoes_caixa m "
                  "LEFT JOIN operadores o ON o.id = m.id_operador "
                  "WHERE m.id_pagamento_divida = :id AND m.tipo = 'RECEBIMENTO' ORDER BY m.id DESC LIMIT 1");
    query.bindValue(":id", idPagamentoDivida);
    if (!query.exec() || !query.next())
        return m;
    return lerMovimentacao(query);
}

bool Caixa_repository::deletarMovimentacao(qlonglong id, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    QSqlQuery query(db);
    query.prepare("DELETE FROM movimentacoes_caixa WHERE id = :id");
    query.bindValue(":id", id);
    if (!query.exec()) {
        setErro(erro, query.lastError().text());
        return false;
    }
    return true;
}

MovimentacaoCaixaDTO Caixa_repository::getMovimentacao(qlonglong id)
{
    MovimentacaoCaixaDTO m;
    if (!DatabaseConnection_service::open())
        return m;
    QSqlQuery query(db);
    query.prepare("SELECT m.*, o.nome AS nome_operador FROM movimentacoes_caixa m "
                  "LEFT JOIN operadores o ON o.id = m.id_operador WHERE m.id = :id");
    query.bindValue(":id", id);
    if (!query.exec() || !query.next())
        return m;
    return lerMovimentacao(query);
}

bool Caixa_repository::atualizarMovimentacao(qlonglong id, double valor, const QString &motivo, const QString &forma, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    QSqlQuery query(db);
    query.prepare("UPDATE movimentacoes_caixa SET valor = :valor, motivo = :motivo, forma_pagamento = :forma WHERE id = :id");
    query.bindValue(":valor", valor);
    query.bindValue(":motivo", motivo);
    query.bindValue(":forma", forma);
    query.bindValue(":id", id);
    if (!query.exec()) {
        setErro(erro, query.lastError().text());
        return false;
    }
    return true;
}

bool Caixa_repository::deletarRecebimentosPorVenda(qlonglong idVenda, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    QSqlQuery query(db);
    query.prepare("DELETE FROM movimentacoes_caixa WHERE id_venda = :id AND tipo = 'RECEBIMENTO'");
    query.bindValue(":id", idVenda);
    if (!query.exec()) {
        setErro(erro, query.lastError().text());
        return false;
    }
    return true;
}

QList<MovimentacaoCaixaDTO> Caixa_repository::listarMovimentacoes(qlonglong idCaixa)
{
    QList<MovimentacaoCaixaDTO> lista;
    if (!DatabaseConnection_service::open())
        return lista;
    QSqlQuery query(db);
    query.prepare("SELECT m.*, o.nome AS nome_operador FROM movimentacoes_caixa m "
                  "LEFT JOIN operadores o ON o.id = m.id_operador "
                  "WHERE m.id_caixa = :id ORDER BY m.data_hora, m.id");
    query.bindValue(":id", idCaixa);
    if (!query.exec()) {
        qDebug() << "listarMovimentacoes falhou:" << query.lastError().text();
        return lista;
    }
    while (query.next())
        lista << lerMovimentacao(query);
    return lista;
}

QList<MovimentacaoCaixaDTO> Caixa_repository::listarSangrias(const QDate &de, const QDate &ate)
{
    QList<MovimentacaoCaixaDTO> lista;
    if (!DatabaseConnection_service::open())
        return lista;
    QSqlQuery query(db);
    query.prepare("SELECT m.*, o.nome AS nome_operador FROM movimentacoes_caixa m "
                  "LEFT JOIN operadores o ON o.id = m.id_operador "
                  "WHERE m.tipo = 'SANGRIA' AND m.estornado = :est "
                  "AND m.data_hora >= :de AND m.data_hora < :ate ORDER BY m.data_hora, m.id");
    query.bindValue(":est", false);
    query.bindValue(":de", QDateTime(de, QTime(0, 0)));
    query.bindValue(":ate", QDateTime(ate.addDays(1), QTime(0, 0)));
    if (!query.exec()) {
        qDebug() << "listarSangrias falhou:" << query.lastError().text();
        return lista;
    }
    while (query.next())
        lista << lerMovimentacao(query);
    return lista;
}

QList<VendasFormaDTO> Caixa_repository::vendasPorForma(qlonglong idCaixa)
{
    QList<VendasFormaDTO> lista;
    if (!DatabaseConnection_service::open())
        return lista;
    // vendas de uma forma só entram pela forma; vendas "Misto" entram pelas partes (venda_pagamentos),
    // contadas à parte na linha "Misto" (total 0: o valor já está nas formas)
    QSqlQuery query(db);
    query.prepare("SELECT forma_pagamento, COUNT(*) AS qtd, COALESCE(SUM(valor_final), 0) AS total "
                  "FROM vendas2 WHERE id_caixa = :id AND forma_pagamento <> 'Misto' GROUP BY forma_pagamento "
                  "UNION ALL "
                  "SELECT p.forma_pagamento, 0 AS qtd, COALESCE(SUM(p.valor), 0) AS total "
                  "FROM venda_pagamentos p JOIN vendas2 v ON v.id = p.id_venda "
                  "WHERE v.id_caixa = :id2 AND v.forma_pagamento = 'Misto' GROUP BY p.forma_pagamento "
                  "UNION ALL "
                  "SELECT 'Misto' AS forma_pagamento, COUNT(*) AS qtd, 0 AS total "
                  "FROM vendas2 WHERE id_caixa = :id3 AND forma_pagamento = 'Misto' HAVING COUNT(*) > 0");
    query.bindValue(":id", idCaixa);
    query.bindValue(":id2", idCaixa);
    query.bindValue(":id3", idCaixa);
    if (!query.exec()) {
        qDebug() << "vendasPorForma falhou:" << query.lastError().text();
        return lista;
    }
    QMap<QString, VendasFormaDTO> porForma;   // junta a mesma forma vinda de vendas simples e de partes
    while (query.next()) {
        const QString forma = query.value("forma_pagamento").toString();
        VendasFormaDTO &v = porForma[forma];
        v.formaPagamento = forma;
        v.quantidade += query.value("qtd").toInt();
        v.total += query.value("total").toDouble();
    }
    for (const VendasFormaDTO &v : porForma)
        lista << v;
    return lista;
}

QList<FechamentoFormaDTO> Caixa_repository::getFechamento(qlonglong idCaixa)
{
    QList<FechamentoFormaDTO> lista;
    if (!DatabaseConnection_service::open())
        return lista;
    QSqlQuery query(db);
    query.prepare("SELECT * FROM fechamentos_caixa WHERE id_caixa = :id ORDER BY id");
    query.bindValue(":id", idCaixa);
    if (!query.exec())
        return lista;
    while (query.next()) {
        FechamentoFormaDTO f;
        f.formaPagamento = query.value("forma_pagamento").toString();
        f.valorEsperado = query.value("valor_esperado").toDouble();
        f.valorInformado = query.value("valor_informado").toDouble();
        f.diferenca = query.value("diferenca").toDouble();
        f.ocorrenciaTecnica = query.value("ocorrencia_tecnica").toBool();
        lista << f;
    }
    return lista;
}

QList<OperadorSessaoCaixaDTO> Caixa_repository::operadoresDaSessao(qlonglong idCaixa)
{
    QList<OperadorSessaoCaixaDTO> lista;
    if (idCaixa <= 0 || !DatabaseConnection_service::open())
        return lista;

    // vendas e movimentacoes apontando para o mesmo id_operador_sessao, contando registros.
    // Estorno não conta: é a desfazer de uma operação, não uma operação nova de quem estornou.
    const QString sql =
        "SELECT s.id_operador_sessao AS id_op, "
        "       COALESCE(o.nome, 'Gerente (PIN geral)') AS nome, "
        "       SUM(s.quantidade) AS quantidade "
        "FROM ("
        "  SELECT id_operador_sessao, 1 AS quantidade FROM vendas2 "
        "   WHERE id_caixa = :id AND id_operador_sessao IS NOT NULL "
        "  UNION ALL "
        "  SELECT id_operador_sessao, 1 AS quantidade FROM movimentacoes_caixa "
        "   WHERE id_caixa = :id2 AND id_operador_sessao IS NOT NULL "
        "     AND (estornado = FALSE OR estornado IS NULL) "
        ") s "
        "LEFT JOIN operadores o ON o.id = s.id_operador_sessao "
        "GROUP BY s.id_operador_sessao, nome "
        "ORDER BY quantidade DESC, nome";

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":id", idCaixa);
    query.bindValue(":id2", idCaixa);
    if (!query.exec()) {
        qDebug() << "operadoresDaSessao falhou:" << query.lastError().text();
        return lista;
    }
    while (query.next()) {
        OperadorSessaoCaixaDTO op;
        op.id = query.value("id_op").toLongLong();
        op.nome = query.value("nome").toString();
        op.quantidade = query.value("quantidade").toLongLong();
        lista << op;
    }
    return lista;
}

int Caixa_repository::contarRecebimentosEmCaixaFechado(qlonglong idVenda){
    if (idVenda <= 0 || !DatabaseConnection_service::open())
        return 0;
    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM movimentacoes_caixa m JOIN caixas c ON c.id = m.id_caixa "
                  "WHERE m.id_venda = :v AND m.tipo = 'RECEBIMENTO' AND c.status <> 'ABERTO'");
    query.bindValue(":v", idVenda);
    if (!query.exec() || !query.next())
        return 0;
    return query.value(0).toInt();
}
