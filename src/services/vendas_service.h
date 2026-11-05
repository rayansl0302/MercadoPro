#ifndef VENDAS_SERVICE_H
#define VENDAS_SERVICE_H

#include <QObject>
#include "../repository/vendas_repository.h"
#include <QSqlQueryModel>
#include <QPair>
#include <QDate>
#include "../dto/Vendas_dto.h"
#include "../dto/ResumoVendas_dto.h"
#include "eventofiscal_service.h"
#include "notafiscal_service.h"
#include "produtovenda_service.h"
#include "Produto_service.h"
#include "entradasvendas_service.h"
#include "config_service.h"
#include "caixa_service.h"


enum class VendasErro {
    Nenhum,
    CampoVazio,
    Banco,
    InsercaoInvalida,
    DeleteFalhou,
    QuebraDeRegra,
    EventoFiscal,
    Produto,
    NotaFiscal,
    ProdutoVendido,
    EntradasVendas,
    UpdateFalhou

};

class Vendas_service : public QObject
{
    Q_OBJECT
public:
    struct Resultado {
        bool ok;
        VendasErro erro = VendasErro::Nenhum;
        QString msg;
    };
    struct ResultadoInsercaoRN{
        bool ok;
        VendasErro erro = VendasErro::Nenhum;
        QString msg;
        qlonglong idVendaInserida;
    };
    explicit Vendas_service(QObject *parent = nullptr);
    qlonglong getQuantidadeComprasCliente(qlonglong idcli);
    double getValorTotalVendasPrazoCliente(qlonglong idcliente);
    void listarVendas(QSqlQueryModel *model);
    QPair<QDate, QDate> getMinMaxData();
    VendasDTO getVenda(qlonglong id);
    // pagamento dividido: valida as partes e deixa a venda como "Misto" (texto vazio = ok; no-op sem partes)
    static QString prepararPagamentoMisto(VendasDTO &venda);
    ResumoVendasDTO calcularResumo(const QString &dataDe, const QString &dataAte, bool somentePrazo, qlonglong idCliente);
    Vendas_service::Resultado deletarVenda(qlonglong id);
    // motivoCancelamento é obrigatório quando a venda pertence a um caixa aberto
    Vendas_service::Resultado deletarVendaRegraNegocio(qlonglong idVenda, bool cancelarNf,
                                                       const QString &motivoCancelamento = QString());
    bool vendaPossuiNota(qlonglong idVenda);
    Vendas_service::Resultado updateNewTotalTrocoValorFinal(double total, double troco, double valorFinal, qlonglong id);
    Vendas_service::Resultado devolverProdutoRegraNegocio(qlonglong idProdVend, qlonglong idVenda);
    void listarVendasCliente(QSqlQueryModel *model, qlonglong idcliente);
    void listarVendasDeAteFormaPag(QSqlQueryModel *model, QString de, QString ate, VendasUtil::VendasFormaPagamento formaPag, qlonglong idcliente);
    Vendas_service::ResultadoInsercaoRN inserirVendaRegraDeNegocio(VendasDTO venda, QList<ProdutoVendidoDTO> listaProds);
    Vendas_service::Resultado atualizarEstaPago(qlonglong idVenda, bool estaPago);
private:
    QSqlDatabase db;
    Vendas_repository vendasRepo;
    EventoFiscal_service eventoServ;
    ProdutoVenda_service prodVendaServ;
    NotaFiscal_service notaServ;
    Produto_Service prodServ;
    EntradasVendas_service entradaServ;
    Config_service confServ;
    ConfigDTO confDTO;
    Caixa_service caixaServ;

signals:
};

#endif // VENDAS_SERVICE_H
