#ifndef PRECOTAXA_SERVICE_H
#define PRECOTAXA_SERVICE_H

#include <QString>

// Preço de venda = preço cadastrado + a taxa da maquininha mais cara, para o cliente (qualquer forma de pagamento)
// pagar o mesmo valor da gôndola e a loja receber, no cartão, o preço cadastrado.
//
// O cadastro do produto guarda o preço BASE (sem taxa); tudo que o cliente vê (venda, etiquetas, listas impressas)
// passa por aqui. Sem maquininha ativa cadastrada, o preço de venda é igual ao cadastrado.
//
// Cálculo "por dentro": base ÷ (1 − taxa). Com 3,9%: R$ 10,00 vira R$ 10,41, e tirando 3,9% de R$ 10,41 sobram R$ 10,00.
class PrecoTaxa
{
public:
    // Maior taxa (%) entre débito e crédito das maquininhas ativas; 0 se não houver nenhuma.
    static double maiorTaxa();

    // Preço de venda com a maior taxa das maquininhas ativas.
    static double comTaxa(double precoBase);

    // Mesma conta com a taxa informada (%). Taxa fora de 0 < t < 100 devolve o preço sem alteração.
    static double comTaxa(double precoBase, double taxaPercentual);

    // Texto para as telas de cadastro: "Preço de venda com a taxa da maquininha (3,90%): R$ 10,41 ...".
    // Vazio quando não há maquininha ativa ou o preço informado é zero.
    static QString textoPrecoDeVenda(double precoBase);

    // A taxa é lida com um pequeno cache (a tela de vendas pergunta a cada linha); o cadastro de maquininhas
    // chama isto ao salvar para valer na hora.
    static void invalidar();
};

#endif // PRECOTAXA_SERVICE_H
