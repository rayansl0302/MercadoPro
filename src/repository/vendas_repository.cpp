#include "../infra/empresaativa.h"
#include "vendas_repository.h"
#include "../infra/databaseconnection_service.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDate>

Vendas_repository::Vendas_repository(QObject *parent)
    : QObject{parent}
{
    db = DatabaseConnection_service::db();
}


qlonglong Vendas_repository::getQuantidadeComprasCliente(qlonglong idcliente){
    if(!DatabaseConnection_service::open()){
        qDebug() << "erro ao abrir banco de dados. geQuantVendas";
        return 0;
    }

    qlonglong quantCompras = 0;
    QSqlQuery query(db);
    query.prepare("SELECT count(*) FROM vendas2 where id_cliente = :id");
    query.bindValue(":id", idcliente);

    if(!query.exec()){
        return 0;
    }
    if(!query.next()){
        return 0;
    }else{
        quantCompras = query.value(0).toLongLong();
        return quantCompras;
    }
}

double Vendas_repository::getValorTotalVendasPrazoCliente(qlonglong idcliente){
    if (!DatabaseConnection_service::open()) {
        qDebug() << "Erro ao abrir banco de dados getValorTotalVendasPrazoCliente";
        return 0.0;
    }

    QSqlQuery queryVendas(db);
    queryVendas.prepare(
        "SELECT SUM(valor_final) FROM vendas2 "
        "WHERE id_cliente = :id_cliente AND "
        "forma_pagamento = 'Prazo'"
        );
    queryVendas.bindValue(":id_cliente", idcliente);

    double totalVendas = 0.0;

    if (queryVendas.exec() && queryVendas.next()) {
        totalVendas = queryVendas.value(0).toDouble();
    } else {
        qDebug() << "Erro ao calcular total de vendas:" << queryVendas.lastError().text();
        return 0.0;
    }
    return totalVendas;
}
void Vendas_repository::listarVendas(QSqlQueryModel *model)
{

    if (!model) {
        qDebug() << "Model inválido em listarVendas";
        return;
    }

    if (!DatabaseConnection_service::open()) {
        qDebug() << "Erro ao abrir banco (listarVendas)";
        return;
    }

    model->setQuery(
        "SELECT id, valor_final, forma_pagamento, data_hora, "
        "cliente, esta_pago, total, desconto, taxa, "
        "valor_recebido, id_cliente, troco "
        "FROM vendas2 WHERE id_empresa = " + QString::number(EmpresaAtiva::id()) +
        " ORDER BY id DESC LIMIT 240",
        db
        );

    if (model->lastError().isValid()) {
        qDebug() << "Erro SQL:" << model->lastError().text();
    }

}
QPair<QDate, QDate> Vendas_repository::getMinMaxData(){
    QPair<QDate, QDate> dataRange;
    if (!DatabaseConnection_service::open()) {
        qDebug() << "Erro ao abrir banco (getDataMinima)";
        return dataRange;
    }

    QSqlQuery query(db);

    query.exec("SELECT MIN(data_hora) FROM vendas2");
    if (query.next()) {
        dataRange.first = query.value(0).toDate();
    }else{
        qDebug() << "Data Minima query nao rodou";
        return dataRange;
    }

    // data nova
    query.exec("SELECT MAX(data_hora) FROM vendas2");
    if (query.next()) {
        dataRange.second = query.value(0).toDate();
    }else{
        qDebug() << "Data Maxima query nao rodou";
        return dataRange;
    }
    query.finish();
    return dataRange;
}

VendasDTO Vendas_repository::getVenda(qlonglong id){
    VendasDTO result;
    if (!DatabaseConnection_service::open()) {
        qDebug() << "Erro ao abrir banco (listarProdutosVenda)";
        return result;
    }

    QSqlQuery query(db);
    query.prepare("SELECT cliente, data_hora, total, forma_pagamento, valor_recebido, troco, "
                  "taxa, valor_final, desconto, esta_pago, id_cliente, id_caixa, id_empresa FROM vendas2 "
                  "WHERE id = :id_venda");
    query.bindValue(":id_venda", id);
    if(query.exec()){
        while (query.next()) {
            result.id = id;
            result.idCaixa = query.value("id_caixa").toLongLong();
            result.idEmpresa = query.value("id_empresa").toLongLong();
            result.clienteNome = query.value("cliente").toString();
            result.dataHora = query.value("data_hora").toString();
            result.total = query.value("total").toDouble();
            result.formaPagamento = query.value("forma_pagamento").toString();
            result.valorRecebido = query.value("valor_recebido").toDouble();
            result.troco = query.value("troco").toDouble();
            result.taxa = query.value("taxa").toDouble();
            result.valorFinal = query.value("valor_final").toDouble();
            result.desconto = query.value("desconto").toDouble();
            result.estaPago = query.value("esta_pago").toBool();
            result.idCliente = query.value("id_cliente").toLongLong();

        }
    }else{
        qDebug() << "erro query venda2 imprimir";
    }
    if (result.misto())
        result.pagamentos = listarPagamentos(id);
    return result;

}

bool Vendas_repository::inserirPagamentos(qlonglong idVenda, const QList<PagamentoVendaDTO> &pagamentos)
{
    if (!DatabaseConnection_service::open())
        return false;
    for (const PagamentoVendaDTO &p : pagamentos) {
        QSqlQuery query(db);
        query.prepare("INSERT INTO venda_pagamentos (id_venda, forma_pagamento, valor, valor_recebido, troco, "
                      "id_maquina, taxa_maquina) VALUES (:venda, :forma, :valor, :recebido, :troco, :maquina, :taxa)");
        query.bindValue(":venda", idVenda);
        query.bindValue(":forma", p.forma);
        query.bindValue(":valor", p.valor);
        query.bindValue(":recebido", p.valorRecebido);
        query.bindValue(":troco", p.troco);
        query.bindValue(":maquina", p.idMaquina > 0 ? QVariant(p.idMaquina) : QVariant(QMetaType(QMetaType::LongLong)));
        query.bindValue(":taxa", p.idMaquina > 0 || p.taxaMaquina > 0 ? QVariant(p.taxaMaquina)
                                                                      : QVariant(QMetaType(QMetaType::Double)));
        if (!query.exec()) {
            qDebug() << "inserirPagamentos falhou:" << query.lastError().text();
            return false;
        }
    }
    return true;
}

QList<PagamentoVendaDTO> Vendas_repository::listarPagamentos(qlonglong idVenda)
{
    QList<PagamentoVendaDTO> lista;
    if (!DatabaseConnection_service::open())
        return lista;
    QSqlQuery query(db);
    query.prepare("SELECT forma_pagamento, valor, valor_recebido, troco, id_maquina, taxa_maquina "
                  "FROM venda_pagamentos WHERE id_venda = :id ORDER BY id");
    query.bindValue(":id", idVenda);
    if (!query.exec()) {
        qDebug() << "listarPagamentos falhou:" << query.lastError().text();
        return lista;
    }
    while (query.next()) {
        PagamentoVendaDTO p;
        p.forma = query.value(0).toString();
        p.valor = query.value(1).toDouble();
        p.valorRecebido = query.value(2).toDouble();
        p.troco = query.value(3).toDouble();
        p.idMaquina = query.value(4).toLongLong();
        p.taxaMaquina = query.value(5).toDouble();
        lista << p;
    }
    return lista;
}

void Vendas_repository::listarVendasDeAteFormaPagamento(
    QSqlQueryModel *model,
    const QString& de,
    const QString& ate,
    VendasUtil::VendasFormaPagamento formaPag,
    qlonglong idCliente)
{
    if (!model) {
        qDebug() << "Model inválido em listarVendasDeAteFormaPagamento";
        return;
    }

    if (!DatabaseConnection_service::open()) {
        qDebug() << "Erro ao abrir banco";
        return;
    }

    QSqlQuery query(db);

    QString sql =
        "SELECT id, valor_final, forma_pagamento, data_hora, cliente, "
        "esta_pago, total, desconto, taxa, valor_recebido, troco "
        "FROM vendas2 "
        "WHERE data_hora BETWEEN :de AND :ate "
        "AND id_empresa = " + QString::number(EmpresaAtiva::id()) + " ";

    // filtro por cliente
    if (idCliente > 0) {
        sql += "AND id_cliente = :idCliente ";
    }

    // filtro por forma pagamento
    if (formaPag != VendasUtil::VendasFormaPagamento::Nenhuma) {
        sql += "AND forma_pagamento = :formaPag ";
    }

    sql += "ORDER BY id DESC LIMIT 240";

    if (!query.prepare(sql)) {
        qDebug() << "Erro ao preparar query:" << query.lastError().text();
        return;
    }

    query.bindValue(":de", de);
    query.bindValue(":ate", ate);

    if (idCliente > 0) {
        query.bindValue(":idCliente", idCliente);
    }

    if (formaPag != VendasUtil::VendasFormaPagamento::Nenhuma) {

        QString formaString;

        switch (formaPag) {
        case VendasUtil::VendasFormaPagamento::Dinheiro: formaString = "Dinheiro"; break;
        case VendasUtil::VendasFormaPagamento::Credito: formaString = "Crédito"; break;
        case VendasUtil::VendasFormaPagamento::Debito: formaString = "Débito"; break;
        case VendasUtil::VendasFormaPagamento::Pix: formaString = "Pix"; break;
        case VendasUtil::VendasFormaPagamento::Prazo: formaString = "Prazo"; break;
        case VendasUtil::VendasFormaPagamento::NaoSei: formaString = "Não Sei"; break;
        default: break;
        }

        query.bindValue(":formaPag", formaString);
    }

    if (!query.exec()) {
        qDebug() << "Erro SQL:" << query.lastError().text();
        return;
    }

    model->setQuery(query);
}

ResumoVendasDTO Vendas_repository::calcularResumo(
    const QString& dataDe,
    const QString& dataAte,
    bool somentePrazo,
    qlonglong idCliente)
{
    ResumoVendasDTO resumo;

    if (!DatabaseConnection_service::open()) {
        qDebug() << "Erro ao abrir banco (calcularResumo)";
        return resumo;
    }

    QSqlQuery query(db);

    QString where = " WHERE data_hora BETWEEN :de AND :ate AND id_empresa = " +
                    QString::number(EmpresaAtiva::id()) + " ";

    if (somentePrazo)
        where += " AND forma_pagamento = 'Prazo' ";

    if (idCliente > 0)
        where += " AND id_cliente = :idCliente ";

    // TOTAL E QUANTIDADE
    query.prepare(
        "SELECT SUM(total - desconto), COUNT(*) "
        "FROM vendas2 " + where
        );

    query.bindValue(":de", dataDe);
    query.bindValue(":ate", dataAte);

    if (idCliente > 0)
        query.bindValue(":idCliente", idCliente);

    if (query.exec() && query.next()) {
        resumo.total = query.value(0).toDouble();
        resumo.quantidade = query.value(1).toInt();
    }


    qDebug() << "resumo.lucro: " << resumo.lucro;

    return resumo;
}

bool Vendas_repository::deletarVenda(qlonglong id){
    if (!DatabaseConnection_service::open()) {
        qDebug() << "Erro ao abrir banco (deletarVenda)";
        return false;
    }

    QSqlQuery partes(db);
    partes.prepare("DELETE FROM venda_pagamentos WHERE id_venda = :id");
    partes.bindValue(":id", id);
    partes.exec();   // vendas de uma forma só não têm linhas aqui

    QSqlQuery query(db);

    query.prepare("DELETE FROM vendas2 WHERE id = :id");
    query.bindValue(":id", id);
    if(!query.exec()){
        qDebug()<< "Não executou query deletarVenda";
        return false;
    }else{
        return true;
    }
}

bool Vendas_repository::updateNewTotalTrocoValorFinal(double total, double troco, double valorFinal,
                                                      qlonglong id){
    if (!DatabaseConnection_service::open()) {
        qDebug() << "Erro ao abrir banco (updateNewTotalTrocoValorFinal)";
        return false;
    }

    QSqlQuery query(db);

    query.prepare("UPDATE vendas2 SET total = :total, troco = :troco, valor_final = :final "
                  "WHERE id = :id");
    query.bindValue(":total", total);
    query.bindValue(":troco", troco);
    query.bindValue(":final", valorFinal);
    query.bindValue(":id", id);
    if(!query.exec()){
        qDebug() << "Query não rodou updateNewTotalTrocoValorFinal";
        return false;
    }
    return true;
}

void Vendas_repository::listarVendasCliente(QSqlQueryModel *model, qlonglong idcliente)
{
    if (!model) {
        qDebug() << "Model inválido em listarVendasDeAteFormaPagamento";
        return;
    }

    if (!DatabaseConnection_service::open()) {
        qDebug() << "Erro ao abrir banco";
        return;
    }

    QSqlQuery query(db);
    query.prepare("SELECT id, valor_final, forma_pagamento, data_hora, cliente, "
            "esta_pago, total, desconto, taxa, valor_recebido, troco "
            "FROM vendas2 "
            "WHERE id_cliente = :idcli ORDER BY id DESC");
    query.bindValue(":idcli", idcliente);

    if (!query.exec()) {
        qDebug() << "Erro SQL:" << query.lastError().text();
        return;
    }

    model->setQuery(query);
}

qlonglong Vendas_repository::inserir(VendasDTO venda){
    if (!DatabaseConnection_service::open()) {
        qDebug() << "Erro ao abrir banco inserir venda";
        return -1;
    }

    QSqlQuery query(db);
    query.prepare("INSERT INTO vendas2 (cliente, total, data_hora, forma_pagamento, "
                  "valor_recebido, troco, taxa, valor_final, desconto, id_cliente, esta_pago, id_caixa, "
                  "id_operador_sessao, id_empresa, id_maquina, taxa_maquina) "
                  "VALUES (:cliente, :total, :datahora, :formapag, :recebido, :troco, :taxa, "
                  ":valorfinal, :desconto, :idcliente, :estapago, :idcaixa, :idoperadorsessao, :idempresa, "
                  ":idmaquina, :taxamaquina)");
    // só venda no cartão grava a taxa da máquina (NULL nas outras formas: dinheiro, Pix, prazo...)
    const bool cartao = venda.formaPagamento == QStringLiteral("Crédito") || venda.formaPagamento == QStringLiteral("Débito");
    query.bindValue(":idmaquina", cartao && venda.idMaquina > 0 ? QVariant(venda.idMaquina)
                                                               : QVariant(QMetaType(QMetaType::LongLong)));
    // venda mista: taxa_maquina guarda a taxa média ponderada das partes no cartão (a conta do lucro usa esse valor)
    query.bindValue(":taxamaquina", cartao || venda.misto() ? QVariant(venda.taxaMaquina) : QVariant(QMetaType(QMetaType::Double)));
    query.bindValue(":idempresa", venda.idEmpresa > 0 ? venda.idEmpresa : EmpresaAtiva::id());

    query.bindValue(":cliente", venda.clienteNome);
    query.bindValue(":total", venda.total);
    query.bindValue(":datahora", venda.dataHora);
    query.bindValue(":formapag", venda.formaPagamento);
    query.bindValue(":recebido", venda.valorRecebido);
    query.bindValue(":troco", venda.troco);
    query.bindValue(":taxa", venda.taxa);
    query.bindValue(":valorfinal", venda.valorFinal);
    query.bindValue(":desconto", venda.desconto);
    query.bindValue(":idcliente", venda.idCliente);
    query.bindValue(":estapago", venda.estaPago);
    query.bindValue(":idcaixa", venda.idCaixa > 0 ? QVariant(venda.idCaixa)
                                                  : QVariant(QMetaType(QMetaType::LongLong)));
    // -1 (sem sessão) vira NULL; 0 continua sendo o gerente do PIN geral, que é uma sessão válida
    query.bindValue(":idoperadorsessao", venda.idOperadorSessao >= 0
                                              ? QVariant(venda.idOperadorSessao)
                                              : QVariant(QMetaType(QMetaType::LongLong)));

    if(!query.exec()){
        qDebug() << "Query não executou inserir venda()" << query.lastError().text();
        return -1;
    }

    qlonglong idVenda = query.lastInsertId().toInt();

    return idVenda;
}

bool Vendas_repository::atualizarEstaPago(qlonglong idVenda, int estaPago){
    if (!DatabaseConnection_service::open()) {
        qDebug() << "Erro ao abrir banco atualizarEstaPago";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("UPDATE vendas2 SET esta_pago = :val WHERE id = :id");
    query.bindValue(":val", estaPago ? true : false);
    query.bindValue(":id", idVenda);

    if (!query.exec()) {
        qDebug() << "Query nao rodou atualizarEstaPago:" << query.lastError().text();
        return false;
    }
    return true;
}

