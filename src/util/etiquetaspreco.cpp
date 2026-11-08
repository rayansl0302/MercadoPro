#include "etiquetaspreco.h"

#include <QDate>
#include <QFont>
#include <QFontMetrics>
#include <QLocale>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPen>
#include <QStringList>

namespace EtiquetasPreco {

int paginas(int quantidadeDeItens)
{
    if (quantidadeDeItens <= 0)
        return 1;
    return (quantidadeDeItens + kPorPagina - 1) / kPorPagina;
}

bool gerarPdf(const QString &caminho, const QString &empresa, const QString &observacao,
              const QList<Item> &itens, QString *erro)
{
    QPdfWriter pdf(caminho);
    pdf.setPageSize(QPageSize(QPageSize::A4));
    pdf.setResolution(300);
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
    const int topo = 330;           // abaixo do cabeçalho
    const int rodape = 110;
    const int larguraEtiqueta = (largura - 2 * margem) / kColunas;
    const int alturaEtiqueta = (altura - topo - rodape) / kLinhas;
    const QLocale ptBR(QLocale::Portuguese, QLocale::Brazil);
    const int total = paginas(itens.size());

    const QFont fonteTitulo("Arial", 18, QFont::Bold);
    const QFont fonteSub("Arial", 9);
    const QFont fonteNome("Arial", 11, QFont::Bold);
    const QFont fonteUn("Arial", 9);

    for (int pagina = 0; pagina < total; ++pagina) {
        if (pagina > 0)
            pdf.newPage();

        // cabeçalho (fica fora da grade, então não vai para a gôndola)
        p.setPen(QColor("#142D45"));
        p.setFont(fonteTitulo);
        p.drawText(QRect(margem, 80, largura - 2 * margem, 100), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("Etiquetas de preço — recortar nas linhas tracejadas"));
        p.setFont(fonteSub);
        p.setPen(QColor("#5B6B7F"));
        p.drawText(QRect(margem, 185, largura - 2 * margem, 55), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("%1%2Emitido em %3 · %4 produto(s) · folha %5 de %6")
                       .arg(empresa, empresa.isEmpty() ? QString() : QStringLiteral(" · "),
                            QDate::currentDate().toString("dd/MM/yyyy")).arg(itens.size()).arg(pagina + 1).arg(total));
        if (!observacao.isEmpty())
            p.drawText(QRect(margem, 235, largura - 2 * margem, 55), Qt::AlignLeft | Qt::AlignVCenter, observacao);

        // grade de etiquetas, coladas umas nas outras: uma linha de corte serve às duas vizinhas
        for (int k = 0; k < kPorPagina; ++k) {
            const int indice = pagina * kPorPagina + k;
            const int col = k % kColunas;
            const int lin = k / kColunas;
            const QRect area(margem + col * larguraEtiqueta, topo + lin * alturaEtiqueta, larguraEtiqueta, alturaEtiqueta);

            QPen corte(QColor("#9AA7B5"), 3, Qt::DashLine);
            p.setPen(corte);
            p.setBrush(Qt::NoBrush);
            p.drawRect(area);

            if (indice >= itens.size())
                continue;
            const Item &item = itens.at(indice);
            const QRect miolo = area.adjusted(24, 16, -24, -16);

            // nome: até 2 linhas, cortado se for maior
            const QRect areaNome(miolo.left(), miolo.top(), miolo.width(), 118);
            p.setPen(QColor("#142D45"));
            // reduz a letra até o nome inteiro caber na área (nunca corta)
            QFont fonteDoNome = fonteNome;
            double pontosNome = fonteNome.pointSizeF();
            while (pontosNome > 6.0) {
                p.setFont(fonteDoNome);
                const QRect usado = p.boundingRect(QRect(0, 0, areaNome.width(), 10000),
                                                   Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, item.descricao);
                if (usado.height() <= areaNome.height())
                    break;
                pontosNome -= 0.5;
                fonteDoNome.setPointSizeF(pontosNome);
            }
            p.setFont(fonteDoNome);
            p.drawText(areaNome, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, item.descricao);

            // preço grande, reduzido se não couber na largura da etiqueta
            const bool promocao = item.precoDe > item.preco + 0.004;
            const QString preco = QStringLiteral("R$ ") + ptBR.toString(item.preco, 'f', 2);
            int pontos = promocao ? 22 : 30;   // com o "de" riscado sobra menos altura
            QFont fontePreco("Arial", pontos, QFont::Bold);
            while (pontos > 12 && QFontMetrics(fontePreco).horizontalAdvance(preco) > miolo.width()) {
                --pontos;
                fontePreco.setPointSize(pontos);
            }
            QStringList partesRodape;
            if (!item.unidade.trimmed().isEmpty())
                partesRodape << QStringLiteral("por ") + item.unidade.trimmed().toLower();
            if (promocao && !item.validade.trimmed().isEmpty())
                partesRodape << item.validade.trimmed();
            const int alturaUn = partesRodape.isEmpty() ? 0 : 44;
            int topoPreco = areaNome.bottom() + 4;
            if (promocao) {
                // "de R$ x" riscado, menor, acima do preço promocional
                QFont fonteDe("Arial", 11);
                fonteDe.setStrikeOut(true);
                p.setFont(fonteDe);
                p.setPen(QColor("#5B6B7F"));
                const QRect areaDe(miolo.left(), topoPreco, miolo.width(), 44);
                p.drawText(areaDe, Qt::AlignHCenter | Qt::AlignVCenter,
                           QStringLiteral("de R$ ") + ptBR.toString(item.precoDe, 'f', 2));
                topoPreco = areaDe.bottom();
            }
            const QRect areaPreco(miolo.left(), topoPreco, miolo.width(), miolo.bottom() - topoPreco - alturaUn);
            p.setFont(fontePreco);
            p.setPen(promocao ? QColor("#C00000") : QColor("#000000"));
            p.drawText(areaPreco, Qt::AlignHCenter | Qt::AlignVCenter, preco);
            if (alturaUn > 0) {
                p.setFont(fonteUn);
                p.setPen(QColor("#5B6B7F"));
                p.drawText(QRect(miolo.left(), miolo.bottom() - alturaUn, miolo.width(), alturaUn), Qt::AlignHCenter | Qt::AlignVCenter,
                           partesRodape.join(QStringLiteral(" · ")));
            }
        }
    }
    p.end();
    return true;
}

} // namespace EtiquetasPreco
