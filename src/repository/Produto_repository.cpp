#include <QRegularExpression>
#include "Produto_repository.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include "../services/Produto_service.h"
#include "../infra/databaseconnection_service.h"
#include "../util/dbutil.h"
#include "../services/estoquenota_service.h"
#include "../util/datautil.h"

Produto_Repository::Produto_Repository(QObject *parent)
: QObject{parent}
{
    db = DatabaseConnection_service::db();
    dataAgoraUs = DataUtil::getDataAgoraUS();
}

bool Produto_Repository::codigoBarrasExiste(const QString &codigo)
{
    if(!DatabaseConnection_service::open()){
        qDebug() << "db nao aberto ao salvar resumo nota";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM produtos WHERE codigo_barras = :codigo");
    query.bindValue(":codigo", codigo);

    if (!query.exec()) {
        qDebug() << "SQL ERROR:" << query.lastError().text();
        return false;
    }

    query.next();
    return query.value(0).toInt() > 0;
}

bool Produto_Repository::inserir(const ProdutoDTO &p, QString &erroSQL)
{
    if(!DatabaseConnection_service::open()){
        qDebug() << "db nao aberto ao salvar resumo nota";
        return false;
    }
    QString dataAgora = DataUtil::getDataAgoraUS();
    QSqlQuery query(db);
    query.prepare(
        "INSERT INTO produtos "
        "(quantidade, descricao, preco, codigo_barras, nf, un_comercial, "
        "preco_fornecedor, porcent_lucro, ncm, cest, aliquota_imposto, csosn, pis, sem_controle_estoque, "
        "adicionado_em, atualizado_em) "
        "VALUES "
        "(:quantidade, :descricao, :preco, :codigo_barras, :nf, :un_comercial, "
        ":preco_fornecedor, :porcent_lucro, :ncm, :cest, :aliquota_imposto, :csosn, :pis, :sem_controle_estoque, "
        ":adicionadoem, :atualizadoem)"
        );

    query.bindValue(":quantidade", p.quantidade);
    query.bindValue(":descricao", p.descricao);
    query.bindValue(":preco", p.preco);
    query.bindValue(":codigo_barras", p.codigoBarras);
    query.bindValue(":nf", p.nf);
    query.bindValue(":un_comercial", p.uCom);
    query.bindValue(":preco_fornecedor", p.precoFornecedor);
    query.bindValue(":porcent_lucro", p.percentLucro);
    query.bindValue(":ncm", p.ncm);
    query.bindValue(":cest", p.cest);
    query.bindValue(":aliquota_imposto", p.aliquotaIcms);
    query.bindValue(":csosn", p.csosn);
    query.bindValue(":pis", p.pis);
    query.bindValue(":sem_controle_estoque", p.semControleEstoque);
    query.bindValue(":adicionadoem", dataAgora);
    query.bindValue(":atualizadoem", dataAgora);


    if (!query.exec()) {
        erroSQL = query.lastError().text();
        qDebug() << "[SQL ERROR]" << erroSQL;
        qDebug() << "[SQL QUERY]" << query.lastQuery();
        return false;
    }
    return true;
}

void Produto_Repository::listarProdutos(QSqlQueryModel* model)
{
    if (!model) {
        qDebug() << "Model inválido em listarProdutos";
        return;
    }
    if(!DatabaseConnection_service::open()){
        qDebug() << "db nao aberto ao salvar resumo nota";
        return;
    }
    model->setQuery("SELECT * FROM produtos ORDER BY id DESC LIMIT 240", db);

}

QSqlQueryModel* Produto_Repository::getProdutoPeloCodigo(const QString &codigoBarras){
    if(!DatabaseConnection_service::open()){
        qDebug() << "db nao aberto ao salvar resumo nota";
        return nullptr;
    }

    auto *model = new QSqlQueryModel();
    QString sql = QString(
                      "SELECT * FROM produtos WHERE codigo_barras = '%1' ORDER BY id DESC"
                      ).arg(codigoBarras);
    model->setQuery(sql, db);

    if (model->lastError().isValid()) {
        qDebug() << "Erro SQL:" << model->lastError().text();
    }
    return model;
}


bool Produto_Repository::deletar(const QString &id, QString &erroSQL)
{
    if(!DatabaseConnection_service::open()){
        qDebug() << "db nao aberto ao salvar resumo nota";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("DELETE FROM produtos WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        erroSQL = query.lastError().text();
        return false;
    }

    if (query.numRowsAffected() == 0) {
        erroSQL = "Produto não encontrado para exclusão.";
        return false;
    }
    return true;
}

QList<ProdutoVendaRefDTO> Produto_Repository::vendasDoProduto(const QString &id)
{
    QList<ProdutoVendaRefDTO> lista;
    if (!DatabaseConnection_service::open())
        return lista;
    QSqlQuery q(db);
    q.prepare("SELECT pv.id_venda, v.data_hora, v.cliente, pv.quantidade, pv.preco_vendido, v.valor_final, "
              "v.forma_pagamento, v.id_empresa, v.esta_pago "
              "FROM produtos_vendidos pv LEFT JOIN vendas2 v ON v.id = pv.id_venda "
              "WHERE pv.id_produto = :id ORDER BY pv.id_venda DESC");
    q.bindValue(":id", id);
    if (!q.exec()) {
        qDebug() << "vendasDoProduto:" << q.lastError().text();
        return lista;
    }
    while (q.next()) {
        ProdutoVendaRefDTO r;
        r.idVenda = q.value(0).isNull() ? 0 : q.value(0).toLongLong();
        r.dataHora = q.value(1).toString();
        r.cliente = q.value(2).toString();
        r.quantidade = q.value(3).toDouble();
        r.precoVendido = q.value(4).toDouble();
        r.valorFinalVenda = q.value(5).toDouble();
        r.formaPagamento = q.value(6).toString();
        r.idEmpresa = q.value(7).isNull() ? 0 : q.value(7).toLongLong();
        r.estaPago = q.value(8).isNull() ? true : q.value(8).toBool();
        lista.append(r);
    }
    return lista;
}

void Produto_Repository::pesquisar(const QStringList &palavras,
                                              const QString &textoNormalizado, QSqlQueryModel* model)
{
    if (!model) {
        qDebug() << "Model inválido em pesquisar";
        return;
    }

    if(!DatabaseConnection_service::open()){
        qDebug() << "db nao aberto ao salvar resumo nota";
        return;
    }

    QString sql = "SELECT * FROM produtos WHERE ";
    QStringList conditions;

    if (palavras.size() > 1) {
        for (int i = 0; i < palavras.size(); ++i) {
            conditions << QString("descricao LIKE :p%1").arg(i);
        }
        sql += conditions.join(" AND ");
    } else {
        sql += "descricao LIKE :busca OR codigo_barras LIKE :busca";
    }

    sql += " ORDER BY id DESC";

    QSqlQuery query(db);
    query.prepare(sql);

    if (palavras.size() > 1) {
        for (int i = 0; i < palavras.size(); ++i) {
            query.bindValue(QString(":p%1").arg(i), "%" + palavras[i] + "%");
        }
    } else {
        query.bindValue(":busca", "%" + textoNormalizado + "%");
    }

    if (!query.exec()) {
        qDebug() << "[SQL ERROR]" << query.lastError().text();
        return;
    }

    model->setQuery(query);
}

bool Produto_Repository::alterar(
    const ProdutoDTO &p,
    const QString &id,
    QString &erro
    ){
    if(!DatabaseConnection_service::open()){
        qDebug() << "db nao aberto ao salvar resumo nota";
        return false;
    }
    QString dataAgora = DataUtil::getDataAgoraUS();
    QSqlQuery query(db);

    query.prepare(R"(
        UPDATE produtos SET
            quantidade = :quant,
            descricao = :desc,
            preco = :preco,
            codigo_barras = :barras,
            nf = :nf,
            un_comercial = :ucom,
            preco_fornecedor = :precoforn,
            porcent_lucro = :porcentlucro,
            ncm = :ncm,
            cest = :cest,
            aliquota_imposto = :aliquotaimp,
            csosn = :csosn,
            pis = :pis,
            sem_controle_estoque = :semcontrole,
            atualizado_em = :atualizadoem
        WHERE id = :id
    )");

    query.bindValue(":id", id);
    query.bindValue(":quant", p.quantidade);
    query.bindValue(":desc", Produto_Service::normalizeText(p.descricao));
    query.bindValue(":preco", p.preco);
    query.bindValue(":barras", p.codigoBarras);
    query.bindValue(":nf", p.nf);
    query.bindValue(":ucom", p.uCom);
    query.bindValue(":precoforn", p.precoFornecedor);
    query.bindValue(":porcentlucro", p.percentLucro);
    query.bindValue(":ncm", p.ncm);
    query.bindValue(":cest", p.cest);
    query.bindValue(":aliquotaimp", p.aliquotaIcms);
    query.bindValue(":csosn", p.csosn);
    query.bindValue(":pis", p.pis);
    query.bindValue(":semcontrole", p.semControleEstoque);
    query.bindValue(":atualizadoem", dataAgora);


    if (!query.exec()) {
        erro = query.lastError().text();
        return false;
    }
    return true;
}


QStringList Produto_Repository::listarLocais()
{
    QStringList sugestoes;
    if(!DatabaseConnection_service::open()){
        qDebug() << "db nao aberto ao salvar resumo nota";
        return sugestoes;
    }

    QSqlQuery query(db);
    if (query.exec("SELECT DISTINCT local FROM produtos WHERE local IS NOT NULL AND TRIM(local) != ''")) {
        while (query.next()) {
            sugestoes << query.value(0).toString();
        }
    } else {
        qDebug() << "Erro SQL listarLocais:" << query.lastError().text();
    }
    return sugestoes;
}

bool Produto_Repository::atualizarLocal(int id, const QString &local, QString &erroSQL)
{
    if(!DatabaseConnection_service::open()){
        qDebug() << "db nao aberto ao salvar resumo nota";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("UPDATE produtos SET local = :local, atualizado_em = :atualizado WHERE id = :id");
    query.bindValue(":local", local);
    query.bindValue(":atualizado", dataAgoraUs);

    query.bindValue(":id", id);

    if (!query.exec()) {
        erroSQL = query.lastError().text();
        return false;
    }
    return true;
}

bool Produto_Repository::updateAumentarQuantidadeProduto(qlonglong idprod, double quantia){
    if(!DatabaseConnection_service::open()){
        qDebug() << "db nao aberto ao updateAumentarQuantidadeProduto";
        return false;
    }
    QSqlQuery query(db);
    query.prepare("UPDATE produtos SET quantidade = quantidade + :quant, atualizado_em = :atualizadoem "
                  "WHERE id = :id");
    query.bindValue(":quant", quantia);
    query.bindValue(":atualizadoem", dataAgoraUs);

    query.bindValue(":id", idprod);

    if(!query.exec()){
        qDebug() << "Query updateAumentarQuantidadeProduto nao rodou.";
        return false;
    }else{
        return true;
    }
}

ProdutoDTO Produto_Repository::getProduto(qlonglong id){
    ProdutoDTO prod;
    if(!DatabaseConnection_service::open()){
        qDebug() << "db nao aberto ao updateAumentarQuantidadeProduto";
        return prod;
    }

    QSqlQuery query(db);
    query.prepare("SELECT quantidade, descricao, preco, codigo_barras, nf, un_comercial, "
                  "preco_fornecedor, porcent_lucro, ncm, cest, aliquota_imposto, csosn, pis, local, sem_controle_estoque, "
                  "adicionado_em, atualizado_em "
                  "FROM produtos WHERE id = :id");
    query.bindValue(":id", id);
    if(!query.exec()){
        qDebug() << "Query nao rodou getProduto()";
        return prod;
    }

    while(query.next()){
        prod.aliquotaIcms = query.value("aliquota_imposto").toDouble();
        prod.cest = query.value("cest").toString();
        prod.codigoBarras = query.value("codigo_barras").toString();
        prod.csosn = query.value("csosn").toString();
        prod.descricao = query.value("descricao").toString();
        prod.ncm = query.value("ncm").toString();
        prod.nf = query.value("nf").toBool();
        prod.semControleEstoque = query.value("sem_controle_estoque").toBool();
        prod.percentLucro = query.value("porcent_lucro").toDouble();
        prod.pis = query.value("pis").toString();
        prod.preco = query.value("preco").toDouble();
        prod.precoFornecedor = query.value("preco_fornecedor").toDouble();
        prod.quantidade = query.value("quantidade").toDouble();
        prod.uCom = query.value("un_comercial").toString();
        prod.local = query.value("local").toString();
        prod.adicionadoEm = query.value("adicionado_em").toString();
        prod.atualizadoEm = query.value("atualizado_em").toString();

    }
    return prod;
}

QList<ProdutoDTO> Produto_Repository::listarComCodigoInterno(){
    QList<ProdutoDTO> lista;
    if(!DatabaseConnection_service::open()){
        qDebug() << "db nao aberto ao listarComCodigoInterno";
        return lista;
    }
    QSqlQuery query(db);
    if(!query.exec("SELECT id, descricao, preco, codigo_barras, un_comercial FROM produtos "
                   "ORDER BY descricao")){
        qDebug() << "Query listarComCodigoInterno falhou";
        return lista;
    }
    static const QRegularExpression codigoInterno("^3562\\d{5}$");   // o formato que o sistema gera (CodigoBarrasUtil)
    while(query.next()){
        const QString codigo = query.value("codigo_barras").toString().trimmed();
        // código de embalagem (GTIN/EAN válido) o leitor já lê; o resto é código interno (gerado, curto, manual...)
        if(codigo.isEmpty() || EstoqueNota_service::digitoVerificadorGtinConfere(codigo))
            continue;
        ProdutoDTO p;
        p.id = query.value("id").toLongLong();
        p.descricao = query.value("descricao").toString();
        p.preco = query.value("preco").toDouble();
        p.codigoBarras = codigo;
        p.uCom = query.value("un_comercial").toString();
        lista.append(p);
    }
    return lista;
}

ProdutoDTO Produto_Repository::getProdutoPeloCodBarras(const QString &codigo){
    ProdutoDTO prod;
    if(!DatabaseConnection_service::open()){
        qDebug() << "db nao aberto ao getProdutoPeloCodBarras";
        return prod;
    }
    QSqlQuery query(db);
    query.prepare("SELECT * FROM produtos WHERE codigo_barras = :cod");
    query.bindValue(":cod", codigo);

    if(!query.exec()){
        qDebug() << " Query não executou getProdutoPeloCodBarras";
        return prod;
    }

    while(query.next()){
        prod.id = query.value("id").toLongLong();
        prod.quantidade = query.value("quantidade").toDouble();
        prod.descricao = query.value("descricao").toString();
        prod.preco =query.value("preco").toDouble();
        prod.codigoBarras = query.value("codigo_barras").toString();
        prod.nf = query.value("nf").toBool();
        prod.semControleEstoque = query.value("sem_controle_estoque").toBool();
        prod.uCom = query.value("un_comercial").toString();
        prod.precoFornecedor = query.value("preco_fornecedor").toDouble();
        prod.percentLucro = query.value("porcent_lucro").toDouble();
        prod.ncm = query.value("ncm").toString();
        prod.cest = query.value("cest").toString();
        prod.aliquotaIcms = query.value("aliquota_imposto").toDouble();
        prod.csosn = query.value("csosn").toString();
        prod.pis = query.value("pis").toString();
        prod.local = query.value("local").toString();
        prod.adicionadoEm = query.value("adicionado_em").toString();
        prod.atualizadoEm = query.value("atualizado_em").toString();

    }

    return prod;
}

ProdutoDTO Produto_Repository::getProdutoPelaDescricao(const QString &descricao){
    ProdutoDTO prod;
    prod.id = 0;
    if(!DatabaseConnection_service::open()){
        qDebug() << "db nao aberto ao getProdutoPelaDescricao";
        return prod;
    }
    QSqlQuery query(db);
    query.prepare("SELECT codigo_barras FROM produtos WHERE UPPER(TRIM(descricao)) = UPPER(TRIM(:desc)) "
                  "ORDER BY id LIMIT 1");
    query.bindValue(":desc", descricao);
    if(!query.exec()){
        qDebug() << "Query não executou getProdutoPelaDescricao" << query.lastError().text();
        return prod;
    }
    if(!query.next())
        return prod;
    return getProdutoPeloCodBarras(query.value(0).toString());
}

bool Produto_Repository::updateDiminuirQuantidadeProduto(qlonglong idprod, double quantia){
    if(!DatabaseConnection_service::open()){
        qDebug() << "db nao aberto ao updateDiminuirQuantidadeProduto";
        return false;
    }
    QSqlQuery query(db);
    query.prepare("UPDATE produtos SET quantidade = quantidade - :quant , atualizado_em = :atualizadoem "
                  "WHERE id = :id");
    query.bindValue(":quant", quantia);
    query.bindValue(":atualizadoem", DataUtil::getDataAgoraUS());

    query.bindValue(":id", idprod);

    if(!query.exec()){
        qDebug() << "Query updateAumentarQuantidadeProduto nao rodou.";
        return false;
    }else{
        return true;
    }
}

bool Produto_Repository::atualizarCamposMap(qlonglong id, const QVariantMap &campos, bool marcarNf){
    if(!DatabaseConnection_service::open()){
        qDebug() << "erro ao abrir banco atualizarCamposMap";
        return false;
    }

    QStringList sets;
    QVariantList binds;

    for(auto it = campos.constBegin(); it != campos.constEnd(); ++it){
        sets << QString("%1 = ?").arg(it.key());
        binds << it.value();
    }

    if(marcarNf){
        sets << "nf = ?";
        binds << true;
    }

    if(sets.isEmpty()){
        return true;
    }

    sets << "atualizado_em = ?";

    QSqlQuery query(db);
    query.prepare("UPDATE produtos SET " + sets.join(", ") + " WHERE id = ?");

    // binds dos campos
    for(const QVariant &v : binds)
        query.addBindValue(v);

    // atualizado_em
    query.addBindValue(DataUtil::getDataAgoraUS());

    // id
    query.addBindValue(id);

    if(!query.exec()){
        qDebug() << "SQL:" << query.lastQuery();
        qDebug() << "ERRO:" << query.lastError().text();
        return false;
    }
    return true;
}

QVariantMap Produto_Repository::getProdutoPorCodBarrasMap(const QString &codigo){
    if(!DatabaseConnection_service::open()){
        qDebug() << "erro ao abrir banco getProdutoPorCodBarrasMap";
        return {};
    }

    QSqlQuery query(db);
    query.prepare("SELECT * FROM produtos WHERE codigo_barras = :codigo");
    query.bindValue(":codigo", codigo);

    if(!query.exec()){
        qDebug() << "getProdutoPorCodBarrasMap: query falhou" << query.lastError().text();
        return {};
    }

    QVector<QVariantMap> registros = DBUtil::extrairResultados(query);
    return registros.isEmpty() ? QVariantMap{} : registros.first();
}
