#ifndef ENTREGA_SERVICE_H
#define ENTREGA_SERVICE_H

#include <QObject>
#include <QList>
#include "../repository/entrega_repository.h"
#include "../dto/Entrega_dto.h"
#include "vendas_service.h"
#include "Produto_service.h"

// Pedido de entrega: preços de entrega com faixa de quantidade, e lançamento da venda de uma vez.
class Entrega_service : public QObject
{
    Q_OBJECT
public:
    struct Resultado {
        bool ok = false;
        QString msg;
        qlonglong idVenda = -1;
        // true quando a venda foi gravada mas a marca de "entrega" não
        bool avisoMarca = false;
        // a venda e os itens gravados (para a tela imprimir o cupom ou emitir a nota fiscal)
        VendasDTO venda;
        QList<ProdutoVendidoDTO> produtos;
    };

    explicit Entrega_service(QObject *parent = nullptr);

    // ---- cálculos (sem banco)
    // preço unitário que vale para a quantidade pedida (faixa de quantidade)
    static double precoAplicado(const EntregaItemDTO &item, double quantidade);
    static double totalLinha(const EntregaItemDTO &item, double quantidade);
    static double totalPedido(const QList<EntregaLinhaDTO> &linhas);
    // quantas unidades cabem em um valor em reais ("quero R$ 30 de pão"); kg fica com 3 casas, o resto é inteiro
    static double quantidadePorValor(const EntregaItemDTO &item, double valor);
    // itens que vêm cadastrados na primeira vez
    static QList<EntregaItemDTO> itensPadrao();

    // ---- banco
    // cadastra os itens padrão se a tabela estiver vazia e liga cada item a um produto do estoque
    // (usa o produto de mesma descrição ou cria um, com estoque 0)
    QList<EntregaItemDTO> carregarItens();
    Entrega_service::Resultado registrarPedido(const EntregaPedidoDTO &pedido);
    bool vendaEhEntrega(qlonglong idVenda);

private:
    qlonglong garantirProduto(const EntregaItemDTO &item);

    Entrega_repository repo;
    Vendas_service vendasServ;
    Produto_Service prodServ;
};

#endif // ENTREGA_SERVICE_H
