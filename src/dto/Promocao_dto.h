#ifndef PROMOCAO_DTO_H
#define PROMOCAO_DTO_H

#include <QString>

// Produto em promoção (de/por). Os preços são os do CADASTRO (base, sem a taxa da maquininha); o que o cliente
// vê na gôndola passa por PrecoTaxa::comTaxa.
struct PromocaoDTO {
    qlonglong idProduto = 0;
    QString descricao;
    QString codigoBarras;
    QString unidade;
    double precoDe = 0;     // preço de antes (base)
    double precoPor = 0;    // preço promocional (base): é o preço vigente do produto
    QString ate;            // AAAA-MM-DD; vazio = sem data para acabar
};

#endif // PROMOCAO_DTO_H
