#include "menuinicial.h"

#include "util/icones.h"

#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {
constexpr int kColunas = 5;
constexpr int kLargura = 158;
constexpr int kAltura = 112;

// o tamanho vem do sizeHint: com botão sem texto próprio o layout usaria o mínimo do estilo
class Tile : public QPushButton
{
public:
    QSize sizeHint() const override { return QSize(kLargura, kAltura); }
    QSize minimumSizeHint() const override { return QSize(kLargura, kAltura); }
};

const char *kEstiloNormal =
    "QPushButton { background: white; border: 1px solid #D5DEE9; border-radius: 12px; padding: 0; }"
    "QPushButton:hover { background: #EAF6EC; border: 2px solid #17963A; }"
    "QPushButton:pressed { background: #D4F0DA; }"
    "QPushButton:focus { border: 2px solid #17963A; }";

const char *kEstiloDestaque =
    "QPushButton { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #2DB14F, stop:1 #107A2E);"
    " border: 1px solid #107A2E; border-radius: 12px; padding: 0; }"
    "QPushButton:hover { background: #107A2E; }"
    "QPushButton:pressed { background: #107A2E; }"
    "QPushButton:focus { border: 2px solid #8FD9A3; }";
}

MenuInicial::MenuInicial(QWidget *parent)
    : QWidget(parent)
{
    auto *externo = new QVBoxLayout(this);
    externo->setContentsMargins(0, 0, 0, 0);

    auto *rolagem = new QScrollArea(this);
    rolagem->setWidgetResizable(true);
    rolagem->setFrameShape(QFrame::NoFrame);
    externo->addWidget(rolagem);

    auto *pagina = new QWidget;
    pagina->setObjectName(QStringLiteral("paginaMenuInicial"));
    pagina->setStyleSheet(QStringLiteral("#paginaMenuInicial { background: transparent; }"));
    rolagem->setWidget(pagina);
    rolagem->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));

    conteudo = new QVBoxLayout(pagina);
    conteudo->setContentsMargins(28, 20, 28, 24);
    conteudo->setSpacing(10);

    lblTitulo = new QLabel(pagina);
    lblTitulo->setStyleSheet(QStringLiteral("font-size: 22pt; font-weight: 700; color: #142D45; background: transparent;"));
    lblSubtitulo = new QLabel(pagina);
    lblSubtitulo->setStyleSheet(QStringLiteral("font-size: 11pt; color: #5B6B7F; background: transparent;"));
    conteudo->addWidget(lblTitulo);
    conteudo->addWidget(lblSubtitulo);
    btnAlerta = new QPushButton(pagina);
    btnAlerta->setCursor(Qt::PointingHandCursor);
    btnAlerta->setVisible(false);
    btnAlerta->setMinimumHeight(40);
    connect(btnAlerta, &QPushButton::clicked, this, [this]() { if (acaoAlerta) acaoAlerta(); });
    conteudo->addWidget(btnAlerta);
    conteudo->addSpacing(6);
    conteudo->addStretch(1);
}

void MenuInicial::definirSaudacao(const QString &titulo, const QString &subtitulo)
{
    lblTitulo->setText(titulo);
    lblSubtitulo->setText(subtitulo);
}

void MenuInicial::definirAlerta(const QString &texto, std::function<void()> acao, bool urgente)
{
    acaoAlerta = std::move(acao);
    btnAlerta->setVisible(!texto.isEmpty());
    btnAlerta->setText(texto);
    const QString cor = urgente ? QStringLiteral("#B91C1C") : QStringLiteral("#B45309");
    const QString fundo = urgente ? QStringLiteral("#FEF2F2") : QStringLiteral("#FFFBEB");
    btnAlerta->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %2; border: 1px solid %2; border-radius: 10px; font-weight: 700;"
        " padding: 6px 14px; text-align: left; }"
        "QPushButton:hover { background: white; }").arg(fundo, cor));
}

void MenuInicial::adicionarGrupo(const QString &titulo)
{
    auto *rotulo = new QLabel(titulo.toUpper());
    rotulo->setStyleSheet(QStringLiteral(
        "font-size: 10pt; font-weight: 700; color: #142D45; letter-spacing: 1px;"
        " background: transparent; border-bottom: 2px solid #17963A; padding: 10px 0 4px 0;"));

    auto *grade = new QGridLayout;
    grade->setHorizontalSpacing(12);
    grade->setVerticalSpacing(12);
    grade->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    gradeAtual = grade;
    posicao = 0;

    // antes do stretch final
    const int indiceStretch = conteudo->count() - 1;
    conteudo->insertWidget(indiceStretch, rotulo);
    conteudo->insertLayout(indiceStretch + 1, grade);
}

void MenuInicial::adicionarBotao(const QString &nomeIcone, const QString &titulo, const QString &dica,
                                 std::function<void()> acao, bool destaque)
{
    if (!gradeAtual)
        adicionarGrupo(QString());

    auto *botao = new Tile;
    botao->setFixedSize(kLargura, kAltura);
    botao->setCursor(Qt::PointingHandCursor);
    botao->setToolTip(dica);
    botao->setStyleSheet(QString::fromLatin1(destaque ? kEstiloDestaque : kEstiloNormal));
    botao->setAccessibleName(titulo);

    auto *layout = new QVBoxLayout(botao);
    layout->setContentsMargins(6, 10, 6, 8);
    layout->setSpacing(4);

    auto *icone = new QLabel;
    icone->setPixmap(Icones::pixmap(nomeIcone, destaque ? QColor(Qt::white) : QColor("#17963A"), 40));
    icone->setAlignment(Qt::AlignCenter);
    icone->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    icone->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto *texto = new QLabel(titulo);
    texto->setAlignment(Qt::AlignCenter);
    texto->setWordWrap(true);
    texto->setStyleSheet(QStringLiteral("font-size: 10.5pt; font-weight: 700; background: transparent; border: none; color: %1;")
                             .arg(destaque ? QStringLiteral("white") : QStringLiteral("#142D45")));
    texto->setAttribute(Qt::WA_TransparentForMouseEvents);

    layout->addWidget(icone, 1);
    layout->addWidget(texto);

    connect(botao, &QPushButton::clicked, this, [acao]() { acao(); });
    gradeAtual->addWidget(botao, posicao / kColunas, posicao % kColunas);
    ++posicao;
}
