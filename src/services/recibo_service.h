#ifndef RECIBO_SERVICE_H
#define RECIBO_SERVICE_H

#include <QObject>
#include <QSqlDatabase>
#include <QLocale>
#include "produtovenda_service.h"
#include "vendas_service.h"
#include "config_service.h"
#include "entradasvendas_service.h"
#include "produto_service.h"
#include "../util/lanches.h"

class Recibo_service : public QObject
{
    Q_OBJECT
public:
    explicit Recibo_service(QObject *parent = nullptr);
    // Reimpressão: escolhe o layout pelo conteúdo da venda (comanda de lanches, cupom padrão ou os dois).
    bool imprimirReciboVenda(qlonglong idvenda, QString *erro = nullptr);

    // Chamado ao finalizar a venda. Lanche: a comanda da cozinha sai sempre (a cozinha precisa dela);
    // o cupom padrão só sai se querCupom. Venda normal: só imprime se querCupom.
    bool imprimirAposVenda(qlonglong idvenda, bool querCupom, QString *erro = nullptr);

    // Classifica a venda já gravada pelos códigos de barras dos itens. Se itensLanche vier, recebe
    // só os itens que são lanche.
    Lanches::TipoVenda tipoDaVenda(qlonglong idvenda, QList<ProdutoVendidoDTO> *itensLanche = nullptr);
private:
    bool imprimirCupomPadrao(qlonglong idvenda, QString *erro);
    bool imprimirComanda(qlonglong idvenda, const QList<ProdutoVendidoDTO> &itens, QString *erro);
    int senhaDoDia(qlonglong idvenda, const QString &dataHora);
    QSqlDatabase db;
    Produto_Service prodServ;
    QLocale portugues;
    ProdutoVenda_service prodVendaServ;
    Vendas_service vendaServ;
    Config_service confServ;
    EntradasVendas_service entradaServ;
signals:
};

#endif // RECIBO_SERVICE_H
