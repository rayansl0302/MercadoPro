#include "postpromocao.h"

#include <QDir>
#include <QFont>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <cmath>

namespace PostPromocao {

namespace {
const QColor kVermelho("#C8102E");
const QColor kVermelhoEscuro("#7A0A1C");
const QColor kAmarelo("#FFD400");
const QColor kBranco("#FFFFFF");
const QColor kTexto("#1F2933");
const QColor kCinza("#6B7785");

QString reais(double v)
{
    return QStringLiteral("R$ ") + QLocale(QLocale::Portuguese, QLocale::Brazil).toString(v, 'f', 2);
}

QFont fonte(int pixels, bool negrito = true)
{
    QFont f("Arial");
    f.setPixelSize(pixels);
    f.setBold(negrito);
    return f;
}

// maior tamanho (em pixels, até "maximo") em que o texto cabe no retângulo, quebrando linhas
int ajustar(QPainter &p, const QString &texto, const QRect &area, int maximo, int minimo, bool negrito = true)
{
    for (int px = maximo; px > minimo; px -= 2) {
        p.setFont(fonte(px, negrito));
        const QRect usado = p.boundingRect(QRect(0, 0, area.width(), 100000), Qt::AlignCenter | Qt::TextWordWrap, texto);
        if (usado.height() <= area.height())
            return px;
    }
    return minimo;
}

void fundo(QPainter &p)
{
    QLinearGradient g(0, 0, 0, kAltura);
    g.setColorAt(0.0, kVermelho);
    g.setColorAt(1.0, kVermelhoEscuro);
    p.fillRect(QRect(0, 0, kLargura, kAltura), g);
    // faixa amarela atrás do título
    p.fillRect(QRect(0, 70, kLargura, 150), kAmarelo);
}

void cabecalho(QPainter &p, const QString &titulo)
{
    p.setPen(kVermelhoEscuro);
    p.setFont(fonte(112));
    p.drawText(QRect(0, 70, kLargura, 150), Qt::AlignCenter, titulo);
}

void rodape(QPainter &p, const QString &empresa, const QString &validade)
{
    p.setPen(kBranco);
    int y = kAltura - 150;
    if (!validade.trimmed().isEmpty()) {
        p.setFont(fonte(44));
        p.drawText(QRect(40, y, kLargura - 80, 60), Qt::AlignCenter, validade.trimmed());
        y += 62;
    }
    p.setFont(fonte(38, false));
    p.drawText(QRect(40, y, kLargura - 80, 60), Qt::AlignCenter, empresa.trimmed());
}

void selo(QPainter &p, const QPoint &centro, int raio, int percentual)
{
    if (percentual <= 0)
        return;
    p.setPen(Qt::NoPen);
    p.setBrush(kAmarelo);
    p.drawEllipse(centro, raio, raio);
    p.setPen(kVermelhoEscuro);
    p.setFont(fonte(int(raio * 0.85)));
    p.drawText(QRect(centro.x() - raio, centro.y() - raio, raio * 2, raio * 2), Qt::AlignCenter,
               QStringLiteral("-%1%").arg(percentual));
}

// um cartão da grade: nome, "de" riscado, preço grande e selo de desconto
void cartao(QPainter &p, const QRect &r, const Oferta &o)
{
    QPainterPath caminho;
    caminho.addRoundedRect(r, 28, 28);
    p.setPen(Qt::NoPen);
    p.setBrush(kBranco);
    p.drawPath(caminho);

    const QRect nome(r.left() + 24, r.top() + 22, r.width() - 48 - 70, 120);   // deixa espaço para o selo
    p.setPen(kTexto);
    p.setFont(fonte(ajustar(p, o.nome, nome, 40, 20)));
    p.drawText(nome, Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextWordWrap, o.nome);

    QFont fonteDe = fonte(34, false);
    fonteDe.setStrikeOut(true);
    p.setFont(fonteDe);
    p.setPen(kCinza);
    p.drawText(QRect(r.left(), r.top() + 150, r.width(), 46), Qt::AlignCenter, QStringLiteral("de ") + reais(o.de));

    const QString preco = reais(o.por);
    const QRect areaPreco(r.left() + 12, r.top() + 198, r.width() - 24, r.height() - 198 - 40);
    int px = 84;
    while (px > 30 && QFontMetrics(fonte(px)).horizontalAdvance(preco) > areaPreco.width())
        px -= 2;
    p.setFont(fonte(px));
    p.setPen(kVermelho);
    p.drawText(areaPreco, Qt::AlignHCenter | Qt::AlignVCenter, preco);

    if (!o.unidade.trimmed().isEmpty()) {
        p.setFont(fonte(28, false));
        p.setPen(kCinza);
        p.drawText(QRect(r.left(), r.bottom() - 46, r.width(), 36), Qt::AlignCenter,
                   QStringLiteral("por ") + o.unidade.trimmed().toLower());
    }
    selo(p, QPoint(r.right() - 50, r.top() + 50), 46, descontoPercentual(o.de, o.por));
}
} // namespace

int descontoPercentual(double de, double por)
{
    if (de <= 0 || por <= 0 || por >= de)
        return 0;
    return qRound((1.0 - por / de) * 100.0);
}

int quantidadeDePosts(int ofertas)
{
    return ofertas <= 0 ? 0 : (ofertas + kPorPost - 1) / kPorPost;
}

QImage postEmGrade(const QString &empresa, const QString &validade, const QList<Oferta> &ofertas, int pagina, int totalPaginas)
{
    QImage img(kLargura, kAltura, QImage::Format_ARGB32);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    fundo(p);
    cabecalho(p, totalPaginas > 1 ? QStringLiteral("OFERTAS %1/%2").arg(pagina).arg(totalPaginas) : QStringLiteral("OFERTAS"));

    const int inicio = (pagina - 1) * kPorPost;
    const int margem = 40, vao = 24, topo = 250;
    const int largura = (kLargura - 2 * margem - vao) / 2;
    const int alturaCartao = (kAltura - topo - 190 - 2 * vao) / 3;
    for (int k = 0; k < kPorPost && inicio + k < ofertas.size(); ++k) {
        const int col = k % 2, lin = k / 2;
        cartao(p, QRect(margem + col * (largura + vao), topo + lin * (alturaCartao + vao), largura, alturaCartao),
               ofertas.at(inicio + k));
    }
    rodape(p, empresa, validade);
    p.end();
    return img;
}

QImage postUnico(const QString &empresa, const QString &validade, const Oferta &o)
{
    QImage img(kLargura, kAltura, QImage::Format_ARGB32);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    fundo(p);
    cabecalho(p, QStringLiteral("PROMOÇÃO"));

    const QRect cartaoR(60, 270, kLargura - 120, kAltura - 270 - 200);
    QPainterPath caminho;
    caminho.addRoundedRect(cartaoR, 40, 40);
    p.setPen(Qt::NoPen);
    p.setBrush(kBranco);
    p.drawPath(caminho);

    const QRect nome(cartaoR.left() + 40, cartaoR.top() + 40, cartaoR.width() - 80, 300);
    p.setPen(kTexto);
    p.setFont(fonte(ajustar(p, o.nome, nome, 92, 36)));
    p.drawText(nome, Qt::AlignCenter | Qt::TextWordWrap, o.nome);

    QFont fonteDe = fonte(56, false);
    fonteDe.setStrikeOut(true);
    p.setFont(fonteDe);
    p.setPen(kCinza);
    p.drawText(QRect(cartaoR.left(), cartaoR.top() + 360, cartaoR.width(), 80), Qt::AlignCenter, QStringLiteral("de ") + reais(o.de));

    const QString preco = reais(o.por);
    const QRect areaPreco(cartaoR.left() + 20, cartaoR.top() + 440, cartaoR.width() - 40, 260);
    int px = 230;
    while (px > 60 && QFontMetrics(fonte(px)).horizontalAdvance(preco) > areaPreco.width())
        px -= 4;
    p.setFont(fonte(px));
    p.setPen(kVermelho);
    p.drawText(areaPreco, Qt::AlignCenter, preco);

    if (!o.unidade.trimmed().isEmpty()) {
        p.setFont(fonte(46, false));
        p.setPen(kCinza);
        p.drawText(QRect(cartaoR.left(), cartaoR.bottom() - 90, cartaoR.width(), 60), Qt::AlignCenter,
                   QStringLiteral("por ") + o.unidade.trimmed().toLower());
    }
    selo(p, QPoint(cartaoR.right() - 100, cartaoR.top() + 100), 90, descontoPercentual(o.de, o.por));
    rodape(p, empresa, validade);
    p.end();
    return img;
}

QStringList salvar(const QString &pasta, const QString &empresa, const QString &validade, const QList<Oferta> &ofertas,
                   bool postsIndividuais, QString *erro)
{
    QStringList criados;
    if (ofertas.isEmpty()) {
        if (erro)
            *erro = QStringLiteral("Nenhum produto em promoção.");
        return criados;
    }
    QDir dir(pasta);
    if (!dir.exists() && !dir.mkpath(".")) {
        if (erro)
            *erro = QStringLiteral("Não foi possível criar a pasta %1.").arg(pasta);
        return criados;
    }
    auto gravar = [&](const QImage &img, const QString &nome) {
        const QString caminho = dir.filePath(nome);
        if (!img.save(caminho, "PNG")) {
            if (erro)
                *erro = QStringLiteral("Não foi possível gravar %1.").arg(caminho);
            return false;
        }
        criados << caminho;
        return true;
    };
    const int total = quantidadeDePosts(ofertas.size());
    for (int pg = 1; pg <= total; ++pg)
        if (!gravar(postEmGrade(empresa, validade, ofertas, pg, total), QStringLiteral("promocoes-%1.png").arg(pg, 2, 10, QLatin1Char('0'))))
            return criados;
    if (postsIndividuais)
        for (int i = 0; i < ofertas.size(); ++i)
            if (!gravar(postUnico(empresa, validade, ofertas.at(i)), QStringLiteral("produto-%1.png").arg(i + 1, 2, 10, QLatin1Char('0'))))
                return criados;
    return criados;
}

QString legenda(const QString &empresa, const QString &validade, const QList<Oferta> &ofertas)
{
    QString t = QStringLiteral("🔥 OFERTAS NA %1! 🔥\n\n").arg(empresa.trimmed().isEmpty() ? QStringLiteral("LOJA") : empresa.trimmed().toUpper());
    for (const Oferta &o : ofertas) {
        t += QStringLiteral("✅ %1: de %2 por apenas %3").arg(o.nome, reais(o.de), reais(o.por));
        if (!o.unidade.trimmed().isEmpty())
            t += QStringLiteral(" (%1)").arg(o.unidade.trimmed().toLower());
        const int d = descontoPercentual(o.de, o.por);
        if (d > 0)
            t += QStringLiteral(" · %1% OFF").arg(d);
        t += '\n';
    }
    if (!validade.trimmed().isEmpty())
        t += QStringLiteral("\n⏰ %1\n").arg(validade.trimmed());
    t += QStringLiteral("\nVenha conferir! Enquanto durarem os estoques. 🛒\n\n#promoção #ofertas #supermercado #economia");
    return t;
}

} // namespace PostPromocao
