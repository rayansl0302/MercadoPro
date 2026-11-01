#ifndef PRODUTO_DTO_H
#define PRODUTO_DTO_H

#include <QString>

struct ProdutoDTO {
    qlonglong id;
    double quantidade;
    QString descricao;
    double preco;
    QString codigoBarras;
    bool nf;
    // sem controle de estoque (ovos, a granel, lanches): venda e devolução não mexem na quantidade
    bool semControleEstoque = false;
    // uso da loja (material de limpeza, sacolas...): o painel online não soma o produto nos números de estoque
    bool usoLoja = false;
    QString uCom;
    double precoFornecedor;
    double percentLucro;
    QString ncm;
    QString cest;
    double aliquotaIcms;
    QString csosn;
    QString pis;
    QString local;
    QString adicionadoEm;
    QString atualizadoEm;
};

#endif
