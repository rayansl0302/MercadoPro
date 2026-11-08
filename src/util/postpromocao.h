#ifndef POSTPROMOCAO_H
#define POSTPROMOCAO_H

#include <QImage>
#include <QList>
#include <QString>
#include <QStringList>

// Imagens de promoção para postar no Instagram (feed 1080x1350, 4:5) e a legenda pronta para copiar.
// Os preços recebidos são os que o cliente paga (já com a taxa da maquininha).
namespace PostPromocao {

struct Oferta {
    QString nome;
    QString unidade;   // "KG", "UN"... (opcional)
    double de = 0;     // preço de antes
    double por = 0;    // preço da promoção
};

constexpr int kLargura = 1080;
constexpr int kAltura = 1350;
constexpr int kPorPost = 6;   // 2 colunas x 3 linhas

// % de desconto arredondado (0 quando não há desconto)
int descontoPercentual(double de, double por);

int quantidadeDePosts(int ofertas);

// Post em grade com até kPorPost ofertas (página 1..total).
QImage postEmGrade(const QString &empresa, const QString &validade, const QList<Oferta> &ofertas, int pagina, int totalPaginas);

// Post de uma oferta só, em destaque.
QImage postUnico(const QString &empresa, const QString &validade, const Oferta &oferta);

// Salva os PNGs em "pasta": a(s) grade(s) e, se pedido, um post por produto. Retorna os caminhos criados.
QStringList salvar(const QString &pasta, const QString &empresa, const QString &validade, const QList<Oferta> &ofertas,
                   bool postsIndividuais, QString *erro = nullptr);

// Texto para a legenda do post (com preços de/por e a validade).
QString legenda(const QString &empresa, const QString &validade, const QList<Oferta> &ofertas);

} // namespace PostPromocao

#endif // POSTPROMOCAO_H
