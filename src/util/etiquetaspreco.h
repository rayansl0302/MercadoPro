#ifndef ETIQUETASPRECO_H
#define ETIQUETASPRECO_H

#include <QList>
#include <QString>

// Folha A4 de etiquetas de preço para recortar com tesoura e colar na gôndola (a térmica apaga com o tempo).
// Cada etiqueta tem o nome do produto e o preço grande; as etiquetas ficam coladas umas nas outras, com linhas
// tracejadas de corte entre elas.
namespace EtiquetasPreco {

struct Item {
    QString descricao;
    QString unidade;     // ex.: "UN", "KG" (opcional)
    double preco = 0;    // já é o preço que o cliente paga (com a taxa da maquininha, se for o caso)
};

constexpr int kColunas = 3;
constexpr int kLinhas = 9;
constexpr int kPorPagina = kColunas * kLinhas;   // 27 etiquetas por folha

// Quantas folhas o PDF terá para essa quantidade de itens (no mínimo 1).
int paginas(int quantidadeDeItens);

// Gera o PDF em "caminho". "observacao" sai no cabeçalho (fora da área de corte). Retorna false, com o motivo
// em "erro", se não foi possível criar o arquivo.
bool gerarPdf(const QString &caminho, const QString &empresa, const QString &observacao,
              const QList<Item> &itens, QString *erro = nullptr);

} // namespace EtiquetasPreco

#endif // ETIQUETASPRECO_H
