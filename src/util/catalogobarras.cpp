#include "catalogobarras.h"

#include <QDate>
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QLocale>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPen>

#include "../services/barcode_service.h"

namespace CatalogoBarras {

int paginas(int quantidadeDeItens)
{
    if (quantidadeDeItens <= 0)
        return 1;
    return (quantidadeDeItens + kCartoesPorPagina - 1) / kCartoesPorPagina;
}

bool gerarPdf(const QString &caminho, const QString &titulo, const QString &empresa,
              const QList<Item> &itens, QString *erro)
{
    QPdfWriter pdf(caminho);
    pdf.setPageSize(QPageSize(QPageSize::A4));
    pdf.setResolution(300);            // 1 mm ≈ 11,8 pontos
    pdf.setPageMargins(QMarginsF(0, 0, 0, 0));

    QPainter p;
    if (!p.begin(&pdf)) {
        if (erro)
            *erro = QStringLiteral("Não foi possível criar o arquivo PDF. Confira se a pasta aceita gravação e se o arquivo não está aberto em outro programa.");
        return false;
    }

    const int largura = pdf.width();
    const int altura = pdf.height();
    const int margem = 110;
    const int topoCartoes = 330;
    const int rodape = 110;
    const int colunas = 2;
    const int linhasPorPagina = kCartoesPorPagina / colunas;
    const int espaco = 36;
    const int larguraCartao = (largura - 2 * margem - espaco * (colunas - 1)) / colunas;
    const int alturaCartao = (altura - topoCartoes - rodape - espaco * (linhasPorPagina - 1)) / linhasPorPagina;

    const QLocale ptBR(QLocale::Portuguese, QLocale::Brazil);
    const int totalPaginas = paginas(itens.size());

    QFont fonteTitulo("Arial", 20, QFont::Bold);
    QFont fonteSub("Arial", 10);
    QFont fonteNome("Arial", 11, QFont::Bold);
    QFont fontePreco("Arial", 16, QFont::Bold);
    QFont fonteRodape("Arial", 8);

    for (int pagina = 0; pagina < totalPaginas; ++pagina) {
        if (pagina > 0)
            pdf.newPage();

        // cabeçalho
        p.setPen(QColor("#142D45"));
        p.setFont(fonteTitulo);
        p.drawText(QRect(margem, 90, largura - 2 * margem, 110), Qt::AlignLeft | Qt::AlignVCenter, titulo);
        p.setFont(fonteSub);
        p.setPen(QColor("#5B6B7F"));
        p.drawText(QRect(margem, 205, largura - 2 * margem, 60), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("%1%2Emitido em %3 · %4 produto(s) · aponte o leitor para o código impresso")
                       .arg(empresa, empresa.isEmpty() ? QString() : QStringLiteral(" · "), QDate::currentDate().toString("dd/MM/yyyy")).arg(itens.size()));
        p.setPen(QPen(QColor("#17963A"), 6));
        p.drawLine(margem, 285, largura - margem, 285);

        // cartões
        for (int k = 0; k < kCartoesPorPagina; ++k) {
            const int indice = pagina * kCartoesPorPagina + k;
            if (indice >= itens.size())
                break;
            const Item &item = itens.at(indice);
            const int col = k % colunas;
            const int lin = k / colunas;
            const QRect cartao(margem + col * (larguraCartao + espaco), topoCartoes + lin * (alturaCartao + espaco),
                               larguraCartao, alturaCartao);
            p.setPen(QPen(QColor("#C9D3DF"), 3));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(cartao, 18, 18);

            // nome (até 2 linhas) e preço à direita
            const int larguraPreco = 330;
            const QRect areaNome(cartao.left() + 24, cartao.top() + 14, cartao.width() - larguraPreco - 48, 140);
            p.setPen(QColor("#142D45"));
            p.setFont(fonteNome);
            p.save();
            p.setClipRect(areaNome);                       // nome muito comprido é cortado na área (2 linhas)
            p.drawText(areaNome, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, item.descricao);
            p.restore();
            const QRect areaPreco(cartao.right() - larguraPreco - 24, cartao.top() + 14, larguraPreco, 90);
            p.setFont(fontePreco);
            p.setPen(QColor("#107A2E"));
            p.drawText(areaPreco, Qt::AlignRight | Qt::AlignTop, QStringLiteral("R$ ") + ptBR.toString(item.preco, 'f', 2));
            if (!item.unidade.trimmed().isEmpty()) {
                p.setFont(fonteSub);
                p.setPen(QColor("#5B6B7F"));
                p.drawText(QRect(areaPreco.left(), areaPreco.top() + 85, areaPreco.width(), 50), Qt::AlignRight | Qt::AlignTop,
                           QStringLiteral("por ") + item.unidade.trimmed().toLower());
            }

            // código de barras (Code 128) impresso: sem suavização, para as barras ficarem nítidas
            QString erroCodigo;
            const QImage barras = Barcode_service::gerarCodigoBarras(item.codigo, &erroCodigo);
            const QRect areaBarras(cartao.left() + 24, cartao.top() + 150, cartao.width() - 48, cartao.height() - 150 - 16);
            if (!barras.isNull()) {
                const QImage ajustada = barras.scaled(areaBarras.size(), Qt::KeepAspectRatio, Qt::FastTransformation);
                p.drawImage(QPoint(areaBarras.left() + (areaBarras.width() - ajustada.width()) / 2, areaBarras.top()), ajustada);
            } else {
                p.setPen(QColor("#B91C1C"));
                p.setFont(fonteSub);
                p.drawText(areaBarras, Qt::AlignCenter, QStringLiteral("Código: %1").arg(item.codigo));
            }
        }

        // rodapé
        p.setPen(QColor("#5B6B7F"));
        p.setFont(fonteRodape);
        p.drawText(QRect(margem, altura - rodape, largura - 2 * margem, 70), Qt::AlignRight | Qt::AlignVCenter,
                   QStringLiteral("Página %1 de %2").arg(pagina + 1).arg(totalPaginas));
    }
    p.end();
    return true;
}

} // namespace CatalogoBarras
