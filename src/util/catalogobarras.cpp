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

int paginas(int quantidadeDeItens, Formato formato)
{
    if (quantidadeDeItens <= 0)
        return 1;
    const int porPagina = formato == Formato::Lista ? kLinhasPorPagina : kCartoesPorPagina;
    return (quantidadeDeItens + porPagina - 1) / porPagina;
}

namespace {

// Lista: uma linha por produto (nome | un. | preço | barras com o número embaixo)
void desenharLista(QPainter &p, QPdfWriter &pdf, const QString &titulo, const QString &empresa, const QList<Item> &itens)
{
    const int largura = pdf.width();
    const int altura = pdf.height();
    const int margem = 110;
    const int topo = 400;           // abaixo do cabeçalho da tabela
    const int rodape = 110;
    const int alturaLinha = (altura - topo - rodape) / kLinhasPorPagina;
    const int total = paginas(itens.size(), Formato::Lista);
    const QLocale ptBR(QLocale::Portuguese, QLocale::Brazil);

    const int xNome = margem + 10;
    const int larguraNome = 1000;
    const int xUn = xNome + larguraNome + 20;
    const int larguraUn = 110;
    const int xPreco = xUn + larguraUn;
    const int larguraPreco = 260;
    const int xCodigo = xPreco + larguraPreco + 40;
    const int larguraCodigo = largura - margem - xCodigo;

    QFont fonteTitulo("Arial", 20, QFont::Bold);
    QFont fonteSub("Arial", 10);
    QFont fonteCab("Arial", 9, QFont::Bold);
    QFont fonteNome("Arial", 9, QFont::Bold);
    QFont fontePreco("Arial", 10, QFont::Bold);
    QFont fonteCodigo("Arial", 8);
    QFont fonteRodape("Arial", 8);

    for (int pagina = 0; pagina < total; ++pagina) {
        if (pagina > 0)
            pdf.newPage();
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

        // cabeçalho da tabela
        p.setFont(fonteCab);
        p.setPen(QColor("#142D45"));
        const int yCab = 305;
        const int altCab = 70;
        p.drawText(QRect(xNome, yCab, larguraNome, altCab), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("PRODUTO"));
        p.drawText(QRect(xUn, yCab, larguraUn, altCab), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("UN."));
        p.drawText(QRect(xPreco, yCab, larguraPreco, altCab), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("PREÇO"));
        p.drawText(QRect(xCodigo, yCab, larguraCodigo, altCab), Qt::AlignHCenter | Qt::AlignVCenter, QStringLiteral("CÓDIGO"));
        p.setPen(QPen(QColor("#C9D3DF"), 3));
        p.drawLine(margem, topo - 8, largura - margem, topo - 8);

        for (int k = 0; k < kLinhasPorPagina; ++k) {
            const int indice = pagina * kLinhasPorPagina + k;
            if (indice >= itens.size())
                break;
            const Item &item = itens.at(indice);
            const int y = topo + k * alturaLinha;
            if (k % 2 == 1) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor("#F3F6F9"));
                p.drawRect(margem, y, largura - 2 * margem, alturaLinha);
                p.setBrush(Qt::NoBrush);
            }
            p.setFont(fonteNome);
            p.setPen(QColor("#142D45"));
            p.save();
            const QRect areaNome(xNome, y, larguraNome, alturaLinha);
            p.setClipRect(areaNome);
            p.drawText(areaNome, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, item.descricao);
            p.restore();
            p.setFont(fonteCodigo);
            p.setPen(QColor("#5B6B7F"));
            p.drawText(QRect(xUn, y, larguraUn, alturaLinha), Qt::AlignLeft | Qt::AlignVCenter, item.unidade.trimmed().toUpper());
            p.setFont(fontePreco);
            p.setPen(QColor("#107A2E"));
            p.drawText(QRect(xPreco, y, larguraPreco, alturaLinha), Qt::AlignRight | Qt::AlignVCenter,
                       QStringLiteral("R$ ") + ptBR.toString(item.preco, 'f', 2));

            QString erroCodigo;
            const QImage barras = Barcode_service::gerarCodigoBarras(item.codigo, &erroCodigo);
            const QRect areaBarras(xCodigo, y + 8, larguraCodigo, alturaLinha - 16 - 34);
            if (!barras.isNull()) {
                const QImage ajustada = barras.scaled(areaBarras.size(), Qt::KeepAspectRatio, Qt::FastTransformation);
                p.drawImage(QPoint(areaBarras.left() + (areaBarras.width() - ajustada.width()) / 2, areaBarras.top()), ajustada);
            }
            p.setFont(fonteCodigo);
            p.setPen(QColor("#142D45"));
            p.drawText(QRect(xCodigo, y + alturaLinha - 42, larguraCodigo, 34), Qt::AlignHCenter | Qt::AlignVCenter, item.codigo);
        }
        p.setPen(QColor("#5B6B7F"));
        p.setFont(fonteRodape);
        p.drawText(QRect(margem, altura - rodape, largura - 2 * margem, 70), Qt::AlignRight | Qt::AlignVCenter,
                   QStringLiteral("Página %1 de %2").arg(pagina + 1).arg(total));
    }
}

} // namespace

bool gerarPdf(const QString &caminho, const QString &titulo, const QString &empresa,
              const QList<Item> &itens, QString *erro, Formato formato)
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

    if (formato == Formato::Lista) {
        desenharLista(p, pdf, titulo, empresa, itens);
        p.end();
        return true;
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
