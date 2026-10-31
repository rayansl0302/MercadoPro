#ifndef CATALOGOBARRAS_H
#define CATALOGOBARRAS_H

#include <QList>
#include <QString>

// PDF "tabela de preços com código de barras" para o operador de caixa: cada produto em um cartão com o nome, o
// preço e o código de barras impresso (o leitor lê direto do papel). Serve, por exemplo, para os produtos que o
// sistema gerou o código (itens sem código de barras na embalagem, a granel, pães, ovos...).
namespace CatalogoBarras {

struct Item {
    QString descricao;
    QString unidade;      // ex.: "UN", "KG" (opcional)
    double preco = 0;
    QString codigo;       // código de barras (Code 128)
};

constexpr int kCartoesPorPagina = 14;   // 2 colunas x 7 linhas, A4 em pé

// Quantas páginas o PDF terá para essa quantidade de itens (no mínimo 1).
int paginas(int quantidadeDeItens);

// Gera o PDF em "caminho". Retorna false (com o motivo em "erro") se não foi possível.
// Os itens são impressos na ordem recebida.
bool gerarPdf(const QString &caminho, const QString &titulo, const QString &empresa,
              const QList<Item> &itens, QString *erro = nullptr);

} // namespace CatalogoBarras

#endif // CATALOGOBARRAS_H
