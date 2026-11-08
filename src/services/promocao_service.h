#ifndef PROMOCAO_SERVICE_H
#define PROMOCAO_SERVICE_H

#include <QDate>
#include <QList>
#include "../dto/Promocao_dto.h"
#include "../repository/promocao_repository.h"

// Produto em promoção (de/por). Enquanto dura, o preço do cadastro É o preço promocional (então venda, nota e
// painel já usam o valor certo); o preço de antes fica guardado e volta quando a promoção termina.
class Promocao_service
{
public:
    struct Resultado {
        bool ok = false;
        QString msg;
    };

    QList<PromocaoDTO> listar();

    // Preço de gôndola ("por") -> preço base do cadastro, de modo que base + taxa dê exatamente o valor da gôndola
    // (quando não há centavo que dê exato, devolve o mais próximo). taxa em %, 0 = sem maquininha.
    static double baseParaGondola(double precoGondola, double taxaPercentual);

    // Inicia (ou, se o produto já está em promoção, altera) a promoção. precoPorBase e o preço de "de" são os do
    // cadastro (base). "ate" inválido ou no passado é recusado; data nula = sem data para acabar.
    Resultado definir(qlonglong idProduto, double precoPorBase, const QDate &ate);
    Resultado encerrar(qlonglong idProduto);
    // Encerra (devolve o preço de antes) as promoções cuja data já passou. Retorna quantas encerrou.
    int encerrarVencidas(const QDate &hoje = QDate::currentDate());

private:
    Promocao_repository repo;
};

#endif // PROMOCAO_SERVICE_H
