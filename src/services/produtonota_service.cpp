#include "produtonota_service.h"
#include "../infra/databaseconnection_service.h"
#include <QFile>
#include <QDomDocument>



ProdutoNota_service::ProdutoNota_service(QObject *parent)
    : QObject{parent}
{}

ProdutoNota_service::Resultado ProdutoNota_service::inserirListaProdutos(QList<ProdutoNotaDTO> lista){
    if (lista.isEmpty()) {
        qDebug() << "Nenhum produto encontrado no XML";
        return {false, ProdutoNotaErro::CampoVazio ,"Nenhum produto encontrado no XML"};
    }

    // Tudo ou nada: se um item falhar, nenhum fica gravado. Sem isso a nota ficava com parte dos itens e a
    // nova tentativa de importar pulava os itens ("a nota já tem itens"), deixando a nota incompleta.
    QSqlDatabase conexao = DatabaseConnection_service::db();
    const bool abriu = DatabaseConnection_service::open();
    const bool transacao = abriu && conexao.transaction();
    for (const ProdutoNotaDTO &p : lista) {
        if(!prodNotaRepo.inserir(p)){
            if (transacao)
                conexao.rollback();
            return {false, ProdutoNotaErro::InsercaoInvalida,
                    "Erro ao inserir o produto: " + p.descricao};
        }
    }
    if (transacao && !conexao.commit()) {
        conexao.rollback();
        return {false, ProdutoNotaErro::InsercaoInvalida, "Não foi possível concluir a gravação dos itens da nota."};
    }

    return {true, ProdutoNotaErro::Nenhum, ""};
}

void ProdutoNota_service::listarPorNota(QSqlQueryModel *model, qlonglong idNf)
{
    prodNotaRepo.listarPorNota(model, idNf);
}

QList<ProdutoNotaDTO> ProdutoNota_service::listarDtoPorNota(qlonglong idNf)
{
    return prodNotaRepo.listarDtoPorNota(idNf);
}

ProdutoNotaDTO ProdutoNota_service::getProdutoNota(qlonglong id){
    return prodNotaRepo.getProdutoNota(id);
}

QString ProdutoNota_service::getXmlPathPorId(qlonglong id){
    return prodNotaRepo.getXmlPathPorId(id);
}

bool ProdutoNota_service::marcarComoAdicionado(qlonglong id){
    return prodNotaRepo.marcarComoAdicionado(id);
}

QVariantMap ProdutoNota_service::getProdutoNotaComXmlPath(qlonglong id){
    return prodNotaRepo.getProdutoNotaComXmlPath(id);
}

bool ProdutoNota_service::marcarComoDevolvido(qlonglong id, qlonglong idNfDevol){
    return prodNotaRepo.marcarComoDevolvido(id, idNfDevol);
}

