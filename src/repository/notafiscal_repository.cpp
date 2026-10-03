#include <QDateTime>
#include "notafiscal_repository.h"
#include "../infra/databaseconnection_service.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QMapIterator>
#include "../util/datautil.h"

notafiscal_repository::notafiscal_repository(QObject *parent)
    : QObject{parent}
{
    db = DatabaseConnection_service::db();
}


bool notafiscal_repository::salvarResNFe(NotaFiscalDTO resumoNota, QString *erro){
    if(!DatabaseConnection_service::open()){
        qDebug() << "db nao aberto ao salvar resumo nota";
        if (erro)
            *erro = "Não foi possível abrir o banco de dados.";
        return false;
    }
    QString dataFormatada = DataUtil::getDataAgoraUS();
    QSqlQuery query(db);

    QString dhemi = resumoNota.dhEmi;
    QDateTime dt = QDateTime::fromString(dhemi, "dd/MM/yyyy HH:mm:ss");
    QString dhemiFormatada = dt.toString("yyyy-MM-dd HH:mm:ss");

    query.prepare("INSERT INTO notas_fiscais (cstat, modelo, tp_amb, xml_path, valor_total, atualizado_em,"
                  "cnpjemit, chnfe, nprot, cuf, finalidade, saida, nnf, serie, dhemi, adicionado_em) VALUES (:cstat, :modelo,"
                  " :tpamb, :xmlpath, :vnf, :atualizadoem, :cnpjemit, :chnf, :nprot, :cuf, :finalidade, "
                  ":saida, :nnf, :serie, :dhemi, :adicionadoem)");
    query.bindValue(":cstat", resumoNota.cstat);
    query.bindValue(":modelo", "55");
    query.bindValue(":tpamb", resumoNota.tpAmb != 0);
    query.bindValue(":xmlpath", resumoNota.xmlPath);
    query.bindValue(":vnf", resumoNota.valorTotal);
    query.bindValue(":atualizadoem", dataFormatada);
    query.bindValue(":cnpjemit", resumoNota.cnpjEmit);
    query.bindValue(":chnf", resumoNota.chNfe);
    query.bindValue(":nprot", resumoNota.nProt);
    query.bindValue(":cuf", resumoNota.cuf);
    query.bindValue(":finalidade", resumoNota.finalidade);
    query.bindValue(":saida", false);
    query.bindValue(":nnf", resumoNota.nnf);
    query.bindValue(":serie", QString::number(resumoNota.serie));
    query.bindValue(":dhemi", dhemiFormatada);
    query.bindValue(":adicionadoem", dataFormatada);

    if(!query.exec()){
        qDebug() << "ERRO INSERT notas_fiscais:" << query.lastError().text();
        if (erro)
            *erro = query.lastError().text();
        return false;
    } else {
        qDebug() << "Resumo nota salvo com sucesso!";
        return true;
    }
}


qlonglong notafiscal_repository::getIdFromChave(QString chnfe){
    if(!DatabaseConnection_service::open()){
            qDebug() << "Banco nao abriu getidfromchave()";
            return -1;
    }

    QSqlQuery query(db);
    query.prepare("SELECT id FROM notas_fiscais WHERE chnfe = :chnfe");
    query.bindValue(":chnfe", chnfe);
    QString id_nf = "";
    if(query.exec()){
        while(query.next()){
            id_nf = query.value(0).toString();
        }
    }else{
        qDebug() << "nao rodou query select idnf";
        return -1;
    }
    return id_nf.toLongLong();
}

bool notafiscal_repository::updateWhereChave(NotaFiscalDTO dto, QString chave, QString *erro){
    if(!DatabaseConnection_service::open()){
        qDebug() << "Banco nao abriu updateWhereChave()";
        if (erro)
            *erro = "Não foi possível abrir o banco de dados.";
        return false;
    }

    QSqlQuery q(db);
    qDebug() << "Data DHEMI updateWHERECHAVE: " << dto.dhEmi;
    QString dhemi = dto.dhEmi;
    QDateTime dt = QDateTime::fromString(dhemi, Qt::ISODate);//formato do dhemi no xml
    QString dhemiFormatada = dt.toString("yyyy-MM-dd HH:mm:ss");


    QString dataAgoraFormatada = DataUtil::getDataAgoraUS();
    q.prepare("UPDATE notas_fiscais SET "
              "cstat = :cstat, "
              "nnf = :nnf, "
              "serie = :serie, "
              "modelo = :modelo, "
              "tp_amb = :tp_amb, "
              "xml_path = :xml_path, "
              "valor_total = :valor_total, "
              "cnpjemit = :cnpjemit, "
              "nprot = :nprot, "
              "cuf = :cuf, "
              "atualizado_em = :atualizadoem, "
              "finalidade = :finalidade, "
              "saida = :saida, "
              "id_emissorcliente = :idcliente, "
              "dhemi = :dhemi "
              "WHERE chnfe = :chnfe");
    q.bindValue(":cstat", dto.cstat);
    q.bindValue(":nnf", dto.nnf);
    q.bindValue(":serie", QString::number(dto.serie));
    q.bindValue(":modelo", dto.modelo);
    q.bindValue(":tp_amb", dto.tpAmb != 0);
    q.bindValue(":xml_path", dto.xmlPath);
    q.bindValue(":valor_total", dto.valorTotal);
    q.bindValue(":cnpjemit", dto.cnpjEmit);
    q.bindValue(":nprot", dto.nProt);
    q.bindValue(":cuf", dto.cuf);
    q.bindValue(":chnfe", chave);
    q.bindValue(":atualizadoem", dataAgoraFormatada);
    q.bindValue(":finalidade", "ENTRADA EXTERNA");
    q.bindValue(":saida", false);
    if (dto.idEmissorCliente <= 0)
        q.bindValue(":idcliente", QVariant());
    else
        q.bindValue(":idcliente", dto.idEmissorCliente);
    q.bindValue(":dhemi", dhemiFormatada);


    if (!q.exec()){
        qDebug() << "Erro ao atualizar NF:" << q.lastError();
        if (erro)
            *erro = q.lastError().text();
        return false;
    }else{
        qDebug() << "Nota fiscal atualizada com sucesso!";
    }
    qDebug() << "Linhas afetadas:" << q.numRowsAffected();
    return true;
}

qlonglong notafiscal_repository::getIdFromIdVenda(qlonglong idvenda){
    if(!DatabaseConnection_service::open()){
        qDebug() << "Banco nao abriu getIdFromIdVenda()";
        return -1;
    }

    QSqlQuery query(db);
    query.prepare("SELECT id FROM notas_fiscais WHERE id_venda = :idvenda");
    query.bindValue(":idvenda", idvenda);

    if(!query.exec()){
        qDebug() << "Query getIdFromIdVenda não rodou";
        return -1;
    }


    qlonglong idRetorno = -1;

    if(!query.next()){
        return -1;
    }else{
        idRetorno = query.value(0).toLongLong();
    }
    return idRetorno;
}


namespace {
// CNPJ gravado na nota pode ter pontuação (vem da configuração como foi digitado): compara só os dígitos
const char *kCnpjNota = "REPLACE(REPLACE(REPLACE(COALESCE(cnpjemit, ''), '.', ''), '/', ''), '-', '')";
// reserva "RESERVADA" que ninguém confirmou deixa de valer depois disso (caixa que travou no meio da emissão)
constexpr int kValidadeReservaMinutos = 15;

// QString vazio e nulo são coisas diferentes para o banco (nulo vira NULL e quebra a chave e a comparação)
QString naoNulo(const QString &t) { return t.isNull() ? QStringLiteral("") : t; }

QString limiteReserva()
{
    return QDateTime::currentDateTime().addSecs(-kValidadeReservaMinutos * 60).toString("yyyy-MM-dd HH:mm:ss");
}
}

// Próximo número = maior já usado + 1, olhando as notas salvas E as reservas em andamento de outros caixas.
// Cada CNPJ tem a sua numeração; nota antiga sem CNPJ gravado é da empresa original ("legado").
qlonglong notafiscal_repository::proximoNNF(const QString &modelo, const QString &serie, bool tpAmb,
                                            qlonglong nnfConfigurado, const QString &cnpj, bool legado)
{
    if(!DatabaseConnection_service::open()){
        qDebug() << "Banco nao abriu getProximoNNF()";
        return -1;
    }

    qlonglong ultimo = 0;
    {
        QSqlQuery query(db);
        query.prepare(QString(
            "SELECT MAX(nnf) FROM notas_fiscais "
            "WHERE modelo = :modelo AND serie = :serie AND tp_amb = :tp_amb "
            "AND finalidade != 'ENTRADA EXTERNA' "
            "AND (:cnpj = '' OR %1 = :cnpj2 OR (:legado = 1 AND %1 = ''))").arg(kCnpjNota));
        query.bindValue(":modelo", modelo);
        query.bindValue(":serie", serie);
        query.bindValue(":tp_amb", tpAmb ? 1 : 0);
        query.bindValue(":cnpj", naoNulo(cnpj));
        query.bindValue(":cnpj2", naoNulo(cnpj));
        query.bindValue(":legado", legado ? 1 : 0);
        if(!query.exec()){
            qWarning() << "Erro na consulta NNF:" << query.lastError().text();
            return -1;
        }
        if(query.next() && !query.value(0).isNull())
            ultimo = query.value(0).toLongLong();
    }

    // reservas de outros caixas (ainda válidas ou já confirmadas)
    {
        QSqlQuery query(db);
        query.prepare("SELECT MAX(nnf) FROM nnf_reservas "
                      "WHERE cnpj_emit = :cnpj AND modelo = :modelo AND serie = :serie AND tp_amb = :tp_amb "
                      "AND (status = 'USADA' OR reservado_em >= :limite)");
        query.bindValue(":cnpj", naoNulo(cnpj));
        query.bindValue(":modelo", modelo);
        query.bindValue(":serie", serie);
        query.bindValue(":tp_amb", tpAmb ? 1 : 0);
        query.bindValue(":limite", limiteReserva());
        if(query.exec() && query.next() && !query.value(0).isNull())
            ultimo = qMax(ultimo, query.value(0).toLongLong());
    }

    if(ultimo > 0)
        return ultimo + 1;
    // Se não encontrou nenhuma nota
    if(nnfConfigurado > 0)
        return nnfConfigurado + 1;
    return 1; // fallback final
}

qlonglong notafiscal_repository::getProximoNNF55(QString serie, bool tpAmb, qlonglong nnfConfigurado,
                                                 const QString &cnpj, bool legado){
    return proximoNNF("55", serie, tpAmb, nnfConfigurado, cnpj, legado);
}

qlonglong notafiscal_repository::getProximoNNF65(QString serie, bool tpAmb, qlonglong nnfConfigurado,
                                                 const QString &cnpj, bool legado){
    return proximoNNF("65", serie, tpAmb, nnfConfigurado, cnpj, legado);
}

bool notafiscal_repository::reservarNNF(const QString &cnpj, const QString &modelo, const QString &serie, bool tpAmb,
                                        qlonglong nnf, const QString &terminal, QString *erro)
{
    if(!DatabaseConnection_service::open()){
        if(erro) *erro = "Banco de dados indisponível.";
        return false;
    }
    // reservas que ninguém confirmou e já venceram saem do caminho (a chave única as segura se ficarem)
    {
        QSqlQuery limpa(db);
        limpa.prepare("DELETE FROM nnf_reservas WHERE status = 'RESERVADA' AND reservado_em < :limite");
        limpa.bindValue(":limite", limiteReserva());
        limpa.exec();
    }
    QSqlQuery query(db);
    query.prepare("INSERT INTO nnf_reservas (cnpj_emit, modelo, serie, tp_amb, nnf, status, terminal, reservado_em) "
                  "VALUES (:cnpj, :modelo, :serie, :tp_amb, :nnf, 'RESERVADA', :terminal, :agora)");
    query.bindValue(":cnpj", naoNulo(cnpj));
    query.bindValue(":modelo", modelo);
    query.bindValue(":serie", serie);
    query.bindValue(":tp_amb", tpAmb ? 1 : 0);
    query.bindValue(":nnf", nnf);
    query.bindValue(":terminal", terminal);
    query.bindValue(":agora", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    if(!query.exec()){
        if(erro) *erro = query.lastError().text();
        return false;
    }
    return true;
}

void notafiscal_repository::liberarNNF(const QString &cnpj, const QString &modelo, const QString &serie, bool tpAmb,
                                       qlonglong nnf)
{
    if(!DatabaseConnection_service::open())
        return;
    QSqlQuery query(db);
    query.prepare("DELETE FROM nnf_reservas WHERE cnpj_emit = :cnpj AND modelo = :modelo AND serie = :serie "
                  "AND tp_amb = :tp_amb AND nnf = :nnf AND status = 'RESERVADA'");
    query.bindValue(":cnpj", naoNulo(cnpj));
    query.bindValue(":modelo", modelo);
    query.bindValue(":serie", serie);
    query.bindValue(":tp_amb", tpAmb ? 1 : 0);
    query.bindValue(":nnf", nnf);
    query.exec();
}

void notafiscal_repository::confirmarNNF(const QString &cnpj, const QString &modelo, const QString &serie, bool tpAmb,
                                         qlonglong nnf)
{
    if(!DatabaseConnection_service::open())
        return;
    QSqlQuery query(db);
    query.prepare("UPDATE nnf_reservas SET status = 'USADA' WHERE cnpj_emit = :cnpj AND modelo = :modelo "
                  "AND serie = :serie AND tp_amb = :tp_amb AND nnf = :nnf");
    query.bindValue(":cnpj", naoNulo(cnpj));
    query.bindValue(":modelo", modelo);
    query.bindValue(":serie", serie);
    query.bindValue(":tp_amb", tpAmb ? 1 : 0);
    query.bindValue(":nnf", nnf);
    query.exec();
}

NotaFiscalDTO notafiscal_repository::getNotaNormalFromIdVenda(qlonglong idvenda){
    NotaFiscalDTO nota;
    if(!DatabaseConnection_service::open()){
        qDebug() << "Banco nao abriu getNotaNormalFromIdVenda()";
        return nota;
    }

    QSqlQuery query(db);
    query.prepare("SELECT cstat, nnf, serie, modelo, tp_amb, xml_path, valor_total, atualizado_em, "
                  "id_venda, cnpjemit, chnfe, nprot, cuf, finalidade, saida, id_nf_ref, dhemi, "
                  "id_emissorcliente, adicionado_em FROM notas_fiscais WHERE "
                  "id_venda = :idvenda AND finalidade = 'NORMAL'");
    query.bindValue(":idvenda", idvenda);

    if(!query.exec()){
        qDebug() << "Query nao rodou getNotaNormalFromIdVenda";
        return nota;
    }

    while(query.next()){
        nota.atualizadoEm = query.value("atualizado_em").toString();
        nota.chNfe = query.value("chnfe").toString();
        nota.cnpjEmit = query.value("cnpjemit").toString();
        nota.cstat = query.value("cstat").toString();
        nota.cuf = query.value("cuf").toString();
        nota.dhEmi = query.value("dhemi").toString();
        nota.finalidade = query.value("finalidade").toString();
        nota.idNfRef = query.value("id_nf_ref").toLongLong();
        nota.idEmissorCliente = query.value("id_emissorcliente").toLongLong();
        nota.idVenda = query.value("id_venda").toLongLong();
        nota.modelo = query.value("modelo").toString();
        nota.nnf = query.value("nnf").toLongLong();
        nota.nProt = query.value("nprot").toString();
        nota.saida = query.value("saida").toBool();
        nota.serie = query.value("serie").toInt();
        nota.tpAmb = query.value("tpamb").toInt();
        nota.valorTotal = query.value("valor_total").toDouble();
        nota.xmlPath = query.value("xml_path").toString();
        nota.adicionadoEm = query.value("adicionado_em").toString();
    }
    return nota;
}

bool notafiscal_repository::inserir(NotaFiscalDTO nota){
    if(!DatabaseConnection_service::open()){
        qDebug() << "db nao aberto ao salvar resumo nota";
        return false;
    }
    QString dataFormatada = DataUtil::getDataAgoraUS();
    QSqlQuery query(db);

    // QString dhemi = nota.dhEmi;
    // QString dhemiFormatada = dt.toString("yyyy-MM-dd HH:mm:ss");
    // qDebug() << "dhemi formatada:" << dhemiFormatada;
    if(nota.idNfRef <= 0){
        nota.idNfRef = 0;
    }

    query.prepare("INSERT INTO notas_fiscais (cstat, nnf, serie, modelo, tp_amb, xml_path, valor_total, "
                  "atualizado_em, id_venda, "
                  "cnpjemit, chnfe, nprot, cuf, finalidade, saida, id_nf_ref, dhemi, "
                  "id_emissorcliente, adicionado_em) "
                  "VALUES (:cstat, :nnf, :serie, :modelo, :tpamb, :xmlpath, :vnf, :atualizadoem, :idvenda, "
                  ":cnpjemit, :chnf, :nprot, :cuf, :finalidade, :saida, :idnfref, :dhemi, :idemissor, "
                  ":adicionadoem)");
    query.bindValue(":cstat", nota.cstat);
    query.bindValue(":nnf", nota.nnf);
    query.bindValue(":serie", nota.serie);
    query.bindValue(":modelo", nota.modelo);
    query.bindValue(":tpamb", nota.tpAmb);
    query.bindValue(":xmlpath", nota.xmlPath);
    query.bindValue(":vnf", nota.valorTotal);
    query.bindValue(":atualizadoem", dataFormatada);
    if(nota.idVenda <= 0){
        query.bindValue(":idvenda", QVariant());
    }else{
        query.bindValue(":idvenda", nota.idVenda);
    }
    query.bindValue(":cnpjemit", nota.cnpjEmit);
    query.bindValue(":chnf", nota.chNfe);
    query.bindValue(":nprot", nota.nProt);
    query.bindValue(":cuf", nota.cuf);
    query.bindValue(":finalidade", nota.finalidade);
    query.bindValue(":saida", nota.saida);
    //IDNFREF deve ser null quando <= 0
    if(nota.idNfRef <= 0){
        query.bindValue(":idnfref", QVariant());
    }else{
        query.bindValue(":idnfref", nota.idNfRef);
    }
    query.bindValue(":dhemi", nota.dhEmi);
    query.bindValue(":idemissor", nota.idEmissorCliente);
    if(nota.idEmissorCliente <= 0){
        query.bindValue(":idemissor", QVariant());
    }else{
        query.bindValue(":idemissor", nota.idEmissorCliente);
    }
    query.bindValue(":adicionadoem", dataFormatada);


    if(!query.exec()){
        qDebug() << "ERRO INSERT notas_fiscais:" << query.lastError().text();
        if(!query.exec()){
            qDebug() << "SQL:" << query.lastQuery();

            qDebug() << "ERRO INSERT notas_fiscais:"
                     << query.lastError().text();

            return false;
        }

        return false;
    } else {
        qDebug() << "Nota salvo com sucesso!";
        return true;
    }
}

void notafiscal_repository::listarEntradas(QSqlQueryModel *model, const QString &de, const QString &ate)
{
    if (!model) return;

    if (!DatabaseConnection_service::open()) {
        qDebug() << "Erro ao abrir banco listarEntradas()";
        return;
    }

    QString sql = R"(
        SELECT
            c.nome AS emitente,
            n.valor_total AS valor,
            n.dhemi AS emissao,
            n.cnpjemit AS cnpj,
            n.modelo AS modelo,
            n.chnfe AS chave,
            n.cstat AS cstat,
            n.id AS id_nf
        FROM notas_fiscais n
        LEFT JOIN clientes c
            ON c.id = n.id_emissorcliente
        WHERE n.finalidade = 'ENTRADA EXTERNA'
          AND n.cstat IN ('100', '150')
    )";

    if (!de.isEmpty() && !ate.isEmpty())
        sql += " AND n.dhemi BETWEEN '" + de + "' AND '" + ate + "'";

    sql += " ORDER BY n.dhemi DESC";

    model->setQuery(sql, db);

    if (model->lastError().isValid())
        qDebug() << "Erro SQL listarEntradas:" << model->lastError().text();

}

qlonglong notafiscal_repository::getIdNotaNormalFromIdVenda(qlonglong idvenda){
    if(!DatabaseConnection_service::open()){
        qDebug() << "db nao aberto ao salvar resumo nota";
        return -1;
    }
    QSqlQuery query(db);
    query.prepare("SELECT id FROM notas_fiscais WHERE id_venda = :idvenda AND finalidade = 'NORMAL'");
    query.bindValue(":idvenda", idvenda);
    if(!query.exec()){
        qDebug() << "Query nao rodou getIdNotaNormalFromIdVenda";
        return -1;
    }
    qlonglong id = -1;
    if(query.next()){
        id = query.value(0).toLongLong();
    }
    return id;

}

NotaFiscalDTO notafiscal_repository::getNotaById(qlonglong id){
    NotaFiscalDTO nota;
    if(!DatabaseConnection_service::open()){
        qDebug() << "Banco nao abriu getNotaById()";
        return nota;
    }

    QSqlQuery query(db);
    query.prepare("SELECT cstat, nnf, serie, modelo, tp_amb, xml_path, valor_total, atualizado_em, "
                  "id_venda, cnpjemit, chnfe, nprot, cuf, finalidade, saida, id_nf_ref, dhemi, "
                  "id_emissorcliente, adicionado_em FROM notas_fiscais WHERE id = :id");
    query.bindValue(":id", id);

    if(!query.exec()){
        qDebug() << "Query nao rodou getNotaById";
        return nota;
    }

    if(query.next()){
        nota.atualizadoEm      = query.value("atualizado_em").toString();
        nota.chNfe             = query.value("chnfe").toString();
        nota.cnpjEmit          = query.value("cnpjemit").toString();
        nota.cstat             = query.value("cstat").toString();
        nota.cuf               = query.value("cuf").toString();
        nota.dhEmi             = query.value("dhemi").toString();
        nota.finalidade        = query.value("finalidade").toString();
        nota.idNfRef           = query.value("id_nf_ref").toLongLong();
        nota.idEmissorCliente  = query.value("id_emissorcliente").toLongLong();
        nota.idVenda           = query.value("id_venda").toLongLong();
        nota.modelo            = query.value("modelo").toString();
        nota.nnf               = query.value("nnf").toLongLong();
        nota.nProt             = query.value("nprot").toString();
        nota.saida             = query.value("saida").toBool();
        nota.serie             = query.value("serie").toInt();
        nota.tpAmb             = query.value("tp_amb").toInt();
        nota.valorTotal        = query.value("valor_total").toDouble();
        nota.xmlPath           = query.value("xml_path").toString();
        nota.adicionadoEm           = query.value("adicionado_em").toString();

    }

    return nota;
}

QMap<QString, int> notafiscal_repository::contarPorFinalidade(QDateTime dtIni, QDateTime dtFim, int tpAmb)
{
    QMap<QString, int> resultado;
    if (!DatabaseConnection_service::open()) {
        qDebug() << "Erro ao abrir banco. contarPorFinalidade";
        return resultado;
    }

    QSqlQuery q(db);
    q.prepare(R"(
        SELECT finalidade, COUNT(*)
        FROM notas_fiscais
        WHERE dhemi BETWEEN :ini AND :fim
        AND tp_amb = :tpamb
        GROUP BY finalidade
    )");
    q.bindValue(":ini", dtIni.toString("yyyy-MM-dd HH:mm:ss"));
    q.bindValue(":fim", dtFim.toString("yyyy-MM-dd HH:mm:ss"));
    q.bindValue(":tpamb", tpAmb);

    if (q.exec()) {
        while (q.next())
            resultado[q.value(0).toString()] = q.value(1).toInt();
    } else {
        qDebug() << "Erro contarPorFinalidade:" << q.lastError().text();
    }

    return resultado;
}

QList<QPair<QString, QString>> notafiscal_repository::buscarXmlsPorPeriodo(QDateTime dtIni, QDateTime dtFim, int tpAmb)
{
    QList<QPair<QString, QString>> resultado;
    if (!DatabaseConnection_service::open()) {
        qDebug() << "Erro ao abrir banco. buscarXmlsPorPeriodo (notas)";
        return resultado;
    }

    QSqlQuery q(db);
    q.prepare(R"(
        SELECT finalidade, xml_path
        FROM notas_fiscais
        WHERE dhemi BETWEEN :ini AND :fim
        AND tp_amb = :tpamb
        AND (cstat != 'CONTINGENCIA_FALHA')
    )");
    q.bindValue(":ini", dtIni.toString("yyyy-MM-dd HH:mm:ss"));
    q.bindValue(":fim", dtFim.toString("yyyy-MM-dd HH:mm:ss"));
    q.bindValue(":tpamb", tpAmb);

    if (q.exec()) {
        while (q.next())
            resultado.append({q.value("finalidade").toString(), q.value("xml_path").toString()});
    } else {
        qDebug() << "Erro buscarXmlsPorPeriodo (notas):" << q.lastError().text();
    }

    return resultado;
}

QList<NotaFiscalDTO> notafiscal_repository::buscarPorPeriodo(QDateTime dtIni, QDateTime dtFim, int tpAmb)
{
    QList<NotaFiscalDTO> resultado;
    if (!DatabaseConnection_service::open()) {
        qDebug() << "Erro ao abrir banco. buscarPorPeriodo";
        return resultado;
    }

    QSqlQuery q(db);
    q.prepare(R"(
        SELECT nnf, dhemi, chnfe, valor_total, finalidade, cstat
        FROM notas_fiscais
        WHERE dhemi BETWEEN :ini AND :fim
        AND tp_amb = :tpamb
        AND finalidade != 'ENTRADA EXTERNA'
        AND (cstat != 'CONTINGENCIA_FALHA')
        ORDER BY dhemi
    )");
    q.bindValue(":ini", dtIni.toString("yyyy-MM-dd HH:mm:ss"));
    q.bindValue(":fim", dtFim.toString("yyyy-MM-dd HH:mm:ss"));
    q.bindValue(":tpamb", tpAmb);

    if (q.exec()) {
        while (q.next()) {
            NotaFiscalDTO nota;
            nota.nnf        = q.value("nnf").toLongLong();
            nota.dhEmi      = q.value("dhemi").toString();
            nota.chNfe      = q.value("chnfe").toString();
            nota.valorTotal = q.value("valor_total").toDouble();
            nota.finalidade = q.value("finalidade").toString();
            nota.cstat      = q.value("cstat").toString();
            resultado.append(nota);
        }
    } else {
        qDebug() << "Erro buscarPorPeriodo:" << q.lastError().text();
    }

    return resultado;
}

QList<NotaFiscalDTO> notafiscal_repository::buscarContingencias()
{
    QList<NotaFiscalDTO> lista;
    if (!DatabaseConnection_service::open()) {
        qDebug() << "Erro ao abrir banco. buscarContingencias()";
        return lista;
    }

    QSqlQuery q(db);
    q.prepare("SELECT cstat, nnf, serie, modelo, tp_amb, xml_path, valor_total, "
              "id_venda, cnpjemit, chnfe, nprot, cuf, finalidade, saida, id_nf_ref, dhemi "
              "FROM notas_fiscais WHERE cstat = 'CONTINGENCIA'");

    if (q.exec()) {
        while (q.next()) {
            NotaFiscalDTO nota;
            nota.cstat      = q.value("cstat").toString();
            nota.chNfe      = q.value("chnfe").toString();
            nota.xmlPath    = q.value("xml_path").toString();
            nota.modelo     = q.value("modelo").toString();
            nota.nnf        = q.value("nnf").toLongLong();
            nota.nProt      = q.value("nprot").toString();
            nota.finalidade = q.value("finalidade").toString();
            nota.dhEmi      = q.value("dhemi").toString();
            lista.append(nota);
        }
    } else {
        qDebug() << "Erro buscarContingencias:" << q.lastError().text();
    }

    return lista;
}

bool notafiscal_repository::atualizarRetornoContingencia(const QString &chNfe, const QString &cstat, const QString &nProt)
{
    if (!DatabaseConnection_service::open()) {
        qDebug() << "Erro ao abrir banco. atualizarRetornoContingencia()";
        return false;
    }

    QSqlQuery q(db);
    q.prepare("UPDATE notas_fiscais SET cstat = :cstat, nprot = :nprot, atualizado_em = :atualizadoem "
              "WHERE chnfe = :chnfe");
    q.bindValue(":cstat",       cstat);
    q.bindValue(":nprot",       nProt);
    q.bindValue(":chnfe",       chNfe);
    q.bindValue(":atualizadoem", DataUtil::getDataAgoraUS());

    bool ok = q.exec();
    if (!ok)
        qDebug() << "Erro atualizarRetornoContingencia:" << q.lastError().text();
    else
        qDebug() << "Contingência atualizada para chave:" << chNfe << "cStat:" << cstat;

    return ok;
}

void notafiscal_repository::listarMonitor(QSqlQueryModel *model, const QStringList &finalidades)
{
    if(!model) return;

    if(!DatabaseConnection_service::open()){
        qDebug() << "Erro ao abrir banco listarMonitor()";
        return;
    }

    QStringList placeholders;
    for(const QString &f : finalidades)
        placeholders << QString("'%1'").arg(f);

    QString sql = R"(
        SELECT
            id,
            valor_total,
            modelo,
            nnf,
            dhemi,
            tp_amb,
            chnfe,
            cnpjemit,
            finalidade,
            xml_path
        FROM notas_fiscais
        WHERE finalidade IN ()" + placeholders.join(", ") + R"()
        ORDER BY dhemi DESC
    )";

    model->setQuery(sql, db);

    if(model->lastError().isValid())
        qDebug() << "Erro SQL listarMonitor:" << model->lastError().text();

}

void notafiscal_repository::listarContingencias(QSqlQueryModel *model)
{
    if(!model) return;

    if(!DatabaseConnection_service::open()){
        qDebug() << "Erro ao abrir banco listarContingencias()";
        return;
    }

    QString sql = R"(
        SELECT
            id,
            valor_total,
            modelo,
            nnf,
            dhemi,
            tp_amb,
            chnfe,
            cnpjemit,
            finalidade,
            xml_path,
            cstat
        FROM notas_fiscais
        WHERE cstat IN ('CONTINGENCIA', 'CONTINGENCIA_FALHA')
        ORDER BY (cstat = 'CONTINGENCIA_FALHA') DESC, dhemi DESC
    )";

    model->setQuery(sql, db);

    if(model->lastError().isValid())
        qDebug() << "Erro SQL listarContingencias:" << model->lastError().text();
}

QString notafiscal_repository::getXmlPathFromIdVenda(qlonglong idvenda){
    if(!DatabaseConnection_service::open()){
        qDebug() << "Erro ao abrir banco getXmlPathFromIdVEnda()";
        return "";
    }
    QString xmlPath = "";
    QSqlQuery q(db);
    q.prepare("SELECT xml_path FROM notas_fiscais WHERE id_venda = :idvenda");
    q.bindValue(":idvenda", idvenda);
    if (!q.exec()) {
        qDebug() << q.lastError().text();
        return {};
    }

    if (!q.next()) {
        return {};
    }
    return q.value(0).toString();

}
