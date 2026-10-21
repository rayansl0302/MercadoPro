#ifndef LANCHES_H
#define LANCHES_H

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

// Identificação de vendas de lanche (itens de produção/chapa) pelo código digitado ou lido no caixa.
// Tudo aqui é função pura (sem banco, sem impressora): fácil de testar e de reaproveitar.
namespace Lanches {

// Faixa de códigos (inclusiva) que identifica um item de lanche. Padrão: 200 a 500.
struct Faixa {
    int minimo = 200;
    int maximo = 500;
};

enum class TipoVenda {
    Normal, // nenhum item de lanche      -> cupom padrão
    Lanche, // todos os itens são lanche  -> comanda térmica de lanches
    Misto   // lanche junto com outros    -> comanda só dos lanches + cupom padrão
};

// Lê a faixa do config.ini ([pdv] lanche_codigo_min / lanche_codigo_max). Valores ausentes ou
// inválidos voltam ao padrão 200-500.
Faixa faixaConfigurada();

// true se o código é um número inteiro dentro da faixa. Só dígitos, sem zero à esquerda e até 4
// dígitos: assim um EAN-13 (ou "0200") nunca é confundido com um lanche.
bool codigoEhLanche(const QString &codigo, const Faixa &faixa = Faixa());

// Classifica um carrinho pelos códigos (de barras) dos itens. Carrinho vazio é Normal.
TipoVenda classificar(const QStringList &codigos, const Faixa &faixa = Faixa());

// Número da senha do pedido no dia: posição desta venda entre as vendas de lanche do dia, em
// ordem de id (estável mesmo com vários caixas vendendo ao mesmo tempo; volta a 1 todo dia).
// itensDoDia = pares (id da venda, código de barras do item) de todas as vendas do dia com id <= idVenda.
int senhaDoDia(const QList<QPair<qlonglong, QString>> &itensDoDia, qlonglong idVenda,
               const Faixa &faixa = Faixa());

} // namespace Lanches

#endif // LANCHES_H
