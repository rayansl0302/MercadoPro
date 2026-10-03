#include "notafiscal_service.h"
#include "caixa_service.h"
#include "../infra/empresaativa.h"
#include <qdebug.h>
#include <QDir>

NotaFiscal_service::NotaFiscal_service(QObject *parent)
    : QObject{parent}
{
    confDTO = confServ.carregarTudo();
}

NotaFiscal_service::Resultado NotaFiscal_service::salvarResNfe(NotaFiscalDTO resumoNota){
    // a nota pode já existir (ex.: lançada pela chave de acesso ou por XML): não duplica a linha
    if(!resumoNota.chNfe.isEmpty() && notaRepo.getIdFromChave(resumoNota.chNfe) > 0){
        return {true, NotaErro::Nenhum, "Nota já registrada."};
    }
    QString detalhe;
    if(notaRepo.salvarResNFe(resumoNota, &detalhe)){
        qDebug() << "Resumo nota salvo com sucesso!";
        return {true, NotaErro::Nenhum, ""};
    }else{
        const QString msg = detalhe.isEmpty()
            ? QString("Erro ao salvar resumo NFe.")
            : QString("Erro ao salvar resumo NFe: %1").arg(detalhe);
        return {false, NotaErro::Salvar, msg};
    }

}

qlonglong NotaFiscal_service::getIdFromChave(QString chnfe){
    return notaRepo.getIdFromChave(chnfe);
}

NotaFiscal_service::Resultado NotaFiscal_service::updateWhereChave(NotaFiscalDTO dto,
                                                                   QString chave){
    QString detalhe;
    if(notaRepo.updateWhereChave(dto, chave, &detalhe)){
        return {true, NotaErro::Nenhum, ""};
    }else{
        const QString msg = detalhe.isEmpty()
            ? QString("Erro ao atualizar Nota Fiscal")
            : QString("Erro ao atualizar Nota Fiscal: %1").arg(detalhe);
        return {false, NotaErro::Update, msg};
    }
}

qlonglong NotaFiscal_service::getIdFromIdVenda(qlonglong idvenda){
    return notaRepo.getIdFromIdVenda(idvenda);
}

qlonglong NotaFiscal_service::getProximoNNF(bool tpAmb, ModeloNota mod){
    QString serie = "1"; // ou algum valor padrão
    return getProximoNNF(serie, tpAmb, mod);
}

namespace {
QString cnpjDigitos(const QString &cnpj)
{
    QString r;
    for(const QChar c : cnpj)
        if(c.isDigit())
            r.append(c);
    return r.isNull() ? QStringLiteral("") : r;       // nunca nulo: nulo viraria NULL no banco
}
bool empresaLegada() { return EmpresaAtiva::id() <= 1; }
QString modeloTexto(ModeloNota mod) { return mod == ModeloNota::NFCe ? QStringLiteral("65") : QStringLiteral("55"); }
}

NotaFiscal_service::ReservaNNF NotaFiscal_service::reservarNNF(QString serie, bool tpAmb, ModeloNota mod,
                                                               qlonglong desejado)
{
    const QString cnpj = cnpjDigitos(confDTO.cnpjEmpresa);
    const QString terminal = Caixa_service::terminalAtual();
    ReservaNNF r;
    for(int tentativa = 0; tentativa < 100; ++tentativa){
        const qlonglong proximo = getProximoNNF(serie, tpAmb, mod);
        if(proximo <= 0){
            r.msg = "Não foi possível consultar a numeração das notas.";
            return r;
        }
        // o número pedido só vale na primeira tentativa e se ainda estiver livre (>= próximo)
        const qlonglong candidato = (tentativa == 0 && desejado >= proximo) ? desejado : proximo;
        QString erro;
        if(notaRepo.reservarNNF(cnpj, modeloTexto(mod), serie, tpAmb, candidato, terminal, &erro)){
            r.ok = true;
            r.nnf = candidato;
            r.alterado = desejado > 0 && candidato != desejado;
            return r;
        }
        // outro caixa ficou com esse número agora: a próxima volta já enxerga a reserva dele
    }
    r.msg = "Não foi possível reservar um número de nota livre. Tente novamente.";
    return r;
}

void NotaFiscal_service::liberarNNF(QString serie, bool tpAmb, ModeloNota mod, qlonglong nnf)
{
    notaRepo.liberarNNF(cnpjDigitos(confDTO.cnpjEmpresa), modeloTexto(mod), serie, tpAmb, nnf);
}

void NotaFiscal_service::confirmarNNF(QString serie, bool tpAmb, ModeloNota mod, qlonglong nnf)
{
    notaRepo.confirmarNNF(cnpjDigitos(confDTO.cnpjEmpresa), modeloTexto(mod), serie, tpAmb, nnf);
}

qlonglong NotaFiscal_service::getProximoNNF(QString serie, bool tpAmb, ModeloNota mod){
    qlonglong nnf;
    const QString cnpj = cnpjDigitos(confDTO.cnpjEmpresa);
    const bool legado = empresaLegada();

    if(mod == ModeloNota::NFCe){
        if(tpAmb){
            nnf = confDTO.nnfProdFiscal;
        }else{
            nnf = confDTO.nnfHomologFiscal;
        }

        return notaRepo.getProximoNNF65(serie, tpAmb, nnf, cnpj, legado);

    } else if(mod == ModeloNota::NFe){
        if(tpAmb){
            nnf = confDTO.nnfProdNfeFiscal;
        }else{
            nnf = confDTO.nnfHomologNfeFiscal;
        }

        return notaRepo.getProximoNNF55(serie, tpAmb, nnf, cnpj, legado);
    }

    return 0;
}

NotaFiscalDTO NotaFiscal_service::getNotaNormalFromIdVenda(qlonglong idvenda){
    return notaRepo.getNotaNormalFromIdVenda(idvenda);
}

NotaFiscalDTO NotaFiscal_service::getNotaById(qlonglong id){
    return notaRepo.getNotaById(id);
}

NotaFiscal_service::Resultado NotaFiscal_service::inserir(NotaFiscalDTO nota){
    QString caminhoCompleto = nota.xmlPath;

    if(confDTO.driverDB == 0){
        QDir baseDir(confDTO.pathPastaSqliteDB);

        nota.xmlPath = baseDir.relativeFilePath(caminhoCompleto);
    }else if(confDTO.driverDB == 1){
        QDir baseDir(confDTO.pathPastaPostgreDB);

        nota.xmlPath = baseDir.relativeFilePath(caminhoCompleto);
    }


    if(!notaRepo.inserir(nota)){
        return {false, NotaErro::Salvar, "Erro ao inserir Nota Fiscal"};
    }else{
        return {true, NotaErro::Nenhum, "Nota salva com sucesso."};
    }
}

bool NotaFiscal_service::temNotaNormal(qlonglong idvenda){
    qlonglong id = notaRepo.getIdNotaNormalFromIdVenda(idvenda);

    if(id != -1){
        return true;
    }else{
        return false;
    }
}

void NotaFiscal_service::listarEntradas(QSqlQueryModel *model, const QString &de, const QString &ate)
{
    notaRepo.listarEntradas(model, de, ate);
}

void NotaFiscal_service::listarMonitor(QSqlQueryModel *model, const QStringList &finalidades)
{
    notaRepo.listarMonitor(model, finalidades);
}

void NotaFiscal_service::listarContingencias(QSqlQueryModel *model)
{
    notaRepo.listarContingencias(model);
}

QMap<QString, int> NotaFiscal_service::contarPorFinalidade(QDateTime dtIni, QDateTime dtFim, int tpAmb)
{
    return notaRepo.contarPorFinalidade(dtIni, dtFim, tpAmb);
}

QList<QPair<QString, QString>> NotaFiscal_service::buscarXmlsPorPeriodo(QDateTime dtIni, QDateTime dtFim, int tpAmb)
{
    return notaRepo.buscarXmlsPorPeriodo(dtIni, dtFim, tpAmb);
}

QList<NotaFiscalDTO> NotaFiscal_service::buscarPorPeriodo(QDateTime dtIni, QDateTime dtFim, int tpAmb)
{
    return notaRepo.buscarPorPeriodo(dtIni, dtFim, tpAmb);
}

QString NotaFiscal_service::getXmlPathFromIdVenda(qlonglong idvenda){
    return notaRepo.getXmlPathFromIdVenda(idvenda);
}
