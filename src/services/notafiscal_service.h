#ifndef NOTAFISCAL_SERVICE_H
#define NOTAFISCAL_SERVICE_H

#include <QObject>
#include <QSqlQueryModel>
#include <QMap>
#include <QList>
#include <QPair>
#include <QDateTime>
#include "../repository/notafiscal_repository.h"
#include "config_service.h"

enum class NotaErro{
    Nenhum,
    Banco,
    Salvar,
    Update
};

enum class ModeloNota{
    NFe,
    NFCe
};

class NotaFiscal_service : public QObject
{
    Q_OBJECT
public:
    struct Resultado {
        bool ok;
        NotaErro erro = NotaErro::Nenhum;
        QString msg;
    };
    explicit NotaFiscal_service(QObject *parent = nullptr);
    NotaFiscal_service::Resultado salvarResNfe(NotaFiscalDTO resumoNota);
    qlonglong getIdFromChave(QString chnfe);
    NotaFiscal_service::Resultado updateWhereChave(NotaFiscalDTO dto, QString chave);
    qlonglong getIdFromIdVenda(qlonglong idvenda);
    qlonglong getProximoNNF(bool tpAmb, ModeloNota mod);
    qlonglong getProximoNNF(QString serie, bool tpAmb, ModeloNota mod);

    struct ReservaNNF {
        bool ok = false;
        qlonglong nnf = 0;
        bool alterado = false;     // o número pedido já estava em uso e foi trocado pelo próximo livre
        QString msg;
    };
    // Reserva o número ANTES de montar o XML. desejado = 0 pega o próximo; se o desejado já foi usado por outro
    // caixa, pega o próximo livre (alterado = true).
    ReservaNNF reservarNNF(QString serie, bool tpAmb, ModeloNota mod, qlonglong desejado = 0);
    // número que não gerou nota (rejeição antes da autorização, erro de envio): volta para a fila
    void liberarNNF(QString serie, bool tpAmb, ModeloNota mod, qlonglong nnf);
    // número consumido (autorizada, denegada ou em contingência)
    void confirmarNNF(QString serie, bool tpAmb, ModeloNota mod, qlonglong nnf);
    NotaFiscalDTO getNotaNormalFromIdVenda(qlonglong idvenda);
    NotaFiscalDTO getNotaById(qlonglong id);
    NotaFiscal_service::Resultado inserir(NotaFiscalDTO nota);
    bool temNotaNormal(qlonglong idvenda);
    void listarEntradas(QSqlQueryModel *model, const QString &de = "", const QString &ate = "");
    void listarMonitor(QSqlQueryModel *model, const QStringList &finalidades);
    void listarContingencias(QSqlQueryModel *model);
    QMap<QString, int>             contarPorFinalidade(QDateTime dtIni, QDateTime dtFim, int tpAmb);
    QList<QPair<QString, QString>> buscarXmlsPorPeriodo(QDateTime dtIni, QDateTime dtFim, int tpAmb);
    QList<NotaFiscalDTO>           buscarPorPeriodo(QDateTime dtIni, QDateTime dtFim, int tpAmb);
    QString getXmlPathFromIdVenda(qlonglong idvenda);
private:
    notafiscal_repository notaRepo;
    Config_service confServ;
    ConfigDTO confDTO;

signals:
};

#endif // NOTAFISCAL_SERVICE_H
