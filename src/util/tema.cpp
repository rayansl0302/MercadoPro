#include "tema.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QEvent>
#include <QObject>
#include <QPalette>
#include <QRegularExpression>
#include <QSettings>
#include <QWidget>

#include "../infra/apppath_service.h"

namespace {

// cores da logo MercadoPro: verde (botões e destaques) e azul-marinho (títulos), com tons de apoio
constexpr auto kPrimaria = "#17963A";
constexpr auto kPrimariaEscura = "#107A2E";
constexpr auto kFundo = "#F2F7F4";
constexpr auto kBorda = "#C9D3DF";
constexpr auto kTexto = "#1F2937";

QString folhaDeEstilo()
{
    const QString css = QStringLiteral(R"(
QToolTip {
    background-color: #1F2937; color: white; border: none; padding: 6px 10px;
}

/* ---- campos de texto ---- */
QLineEdit, QPlainTextEdit, QTextEdit {
    background-color: white; color: @texto;
    border: 1px solid @borda; border-radius: 6px; padding: 5px 8px;
    selection-background-color: @primaria; selection-color: white;
}
QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus {
    border: 2px solid @primaria; padding: 4px 7px;
}
QLineEdit:read-only { background-color: #EEF2F7; }
QLineEdit:disabled, QPlainTextEdit:disabled, QTextEdit:disabled {
    background-color: #EEF1F5; color: #8A94A3;
}

QComboBox, QSpinBox, QDoubleSpinBox, QDateEdit, QDateTimeEdit {
    background-color: white; color: @texto;
    border: 1px solid @borda; border-radius: 6px; padding: 4px 8px; min-height: 22px;
    selection-background-color: @primaria; selection-color: white;
}
QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus, QDateEdit:focus, QDateTimeEdit:focus {
    border: 2px solid @primaria; padding: 3px 7px;
}
QComboBox:disabled { background-color: #EEF1F5; color: #8A94A3; }
QComboBox QAbstractItemView {
    background-color: white; border: 1px solid @borda; outline: none;
    selection-background-color: #D4F0DA; selection-color: #142D45;
}

/* ---- botões (telas com estilo próprio mantêm o delas) ---- */
QPushButton {
    background-color: @primaria; color: white; border: none; border-radius: 6px;
    padding: 6px 16px; min-height: 22px; font-weight: 600;
}
QPushButton:hover { background-color: @primariaEscura; }
QPushButton:pressed { background-color: #107A2E; }
QPushButton:disabled { background-color: #CBD5E1; color: #F8FAFC; }
QPushButton:focus { outline: none; border: 2px solid #8FD9A3; }

/* ---- tabelas ---- */
QTableView, QTableWidget, QTreeView, QListView {
    background-color: white; alternate-background-color: #F5F8FC;
    border: 1px solid @borda; border-radius: 6px; gridline-color: #E6EBF1;
    selection-background-color: #D4F0DA; selection-color: #142D45;
}
QTableView::item, QTreeView::item { padding: 4px 6px; }
QTableView::item:hover, QTreeView::item:hover { background-color: #EAF6EC; }
QHeaderView::section {
    background-color: #EAF3EC; color: #142D45; font-weight: 700;
    border: none; border-right: 1px solid #D5DEE9; border-bottom: 2px solid @primaria;
    padding: 7px 8px;
}
QTableCornerButton::section { background-color: #EAF3EC; border: none; }

/* ---- barras de rolagem finas ---- */
QScrollBar:vertical { background: transparent; width: 12px; margin: 2px; }
QScrollBar::handle:vertical { background: #B6C2D1; border-radius: 4px; min-height: 36px; }
QScrollBar::handle:vertical:hover { background: #8FA1B8; }
QScrollBar:horizontal { background: transparent; height: 12px; margin: 2px; }
QScrollBar::handle:horizontal { background: #B6C2D1; border-radius: 4px; min-width: 36px; }
QScrollBar::handle:horizontal:hover { background: #8FA1B8; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

/* ---- menus ---- */
QMenuBar { background-color: white; border-bottom: 1px solid #DDE4ED; padding: 2px 4px; }
QMenuBar::item { padding: 6px 12px; border-radius: 5px; background: transparent; color: @texto; }
QMenuBar::item:selected { background-color: #E3F4E7; color: #107A2E; }
QMenu { background-color: white; border: 1px solid @borda; padding: 6px; }
QMenu::item { padding: 7px 28px 7px 14px; border-radius: 5px; color: @texto; }
QMenu::item:selected { background-color: #E3F4E7; color: #107A2E; }
QMenu::item:disabled { color: #9AA5B4; }
QMenu::separator { height: 1px; background: #E2E8F0; margin: 5px 8px; }

/* ---- barra de status ---- */
QStatusBar { background-color: #E8F0EA; border-top: 1px solid #D5DEE9; }
QStatusBar::item { border: none; }

/* ---- abas, grupos, barras de progresso ---- */
QTabWidget::pane { border: 1px solid @borda; border-radius: 6px; background: white; top: -1px; }
QTabBar::tab {
    background: #E8F0EA; color: #475569; padding: 8px 18px; margin-right: 2px;
    border-top-left-radius: 6px; border-top-right-radius: 6px;
}
QTabBar::tab:selected { background: white; color: #107A2E; font-weight: 700; border-bottom: 3px solid @primaria; }
QTabBar::tab:hover:!selected { background: #DCE6F1; }
QGroupBox {
    border: 1px solid @borda; border-radius: 8px; margin-top: 14px; padding-top: 10px; font-weight: 600;
}
QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; color: #142D45; }
QProgressBar { border: 1px solid @borda; border-radius: 6px; text-align: center; background: white; }
QProgressBar::chunk { background-color: @primaria; border-radius: 5px; }

/* ---- caixas de mensagem ---- */
QMessageBox QLabel { color: @texto; font-size: 10.5pt; }
QMessageBox { background-color: white; }
QMessageBox QPushButton, QDialogButtonBox QPushButton { min-width: 84px; }
)");
    QString saida = css;
    saida.replace("@primariaEscura", kPrimariaEscura);
    saida.replace("@primaria", kPrimaria);
    saida.replace("@borda", kBorda);
    saida.replace("@texto", kTexto);
    return saida;
}

// Telas e widgets definem tamanhos de letra próprios (folha de estilo "font-size: 18pt" e fontes do .ui).
// Esse filtro escala esses tamanhos na primeira vez que cada widget é preparado para aparecer, para que
// o aumento valha em TODAS as telas, e não só no texto que usa a fonte padrão.
class EscalaDeFonte : public QObject
{
public:
    explicit EscalaDeFonte(double f) : fator(f) {}
    bool eventFilter(QObject *o, QEvent *e) override
    {
        if (e->type() != QEvent::Polish)
            return false;
        auto *w = qobject_cast<QWidget *>(o);
        if (!w)
            return false;
        if (!w->property("_escala_css").isValid()) {
            w->setProperty("_escala_css", true);
            const QString css = w->styleSheet();
            if (css.contains(QLatin1String("font-size"))) {
                const QString novo = Tema::escalarFolha(css, fator);
                if (novo != css)
                    w->setStyleSheet(novo);
            }
        }
        if (!w->property("_escala_fonte").isValid()) {
            w->setProperty("_escala_fonte", true);
            // fonte com tamanho próprio (definida no .ui ou no código); a que o widget só herda do pai não conta,
            // senão o aumento seria aplicado duas vezes
            const bool propria = (w->font().resolveMask() & QFont::SizeResolved) != 0;
            const bool herdada = w->parentWidget() && qFuzzyCompare(w->parentWidget()->font().pointSizeF(), w->font().pointSizeF());
            if (propria && !herdada && w->font().pointSizeF() > 0) {
                QFont f = w->font();
                f.setPointSizeF(f.pointSizeF() * fator);
                w->setFont(f);
            }
        }
        return false;
    }

private:
    double fator;
};

} // namespace

int Tema::percentualFonte()
{
    QSettings s(AppPath_service::configPath(), QSettings::IniFormat);
    const int p = s.value("interface/fonte_percentual", 0).toInt();
    return (p == 5 || p == 10 || p == 20) ? p : 0;
}

void Tema::definirPercentualFonte(int percentual)
{
    QSettings s(AppPath_service::configPath(), QSettings::IniFormat);
    s.setValue("interface/fonte_percentual", (percentual == 5 || percentual == 10 || percentual == 20) ? percentual : 0);
    s.sync();
}

double Tema::fatorFonte()
{
    return 1.0 + percentualFonte() / 100.0;
}

QString Tema::escalarFolha(const QString &css, double fator)
{
    static const QRegularExpression re(QStringLiteral("font-size\\s*:\\s*([0-9]+(?:\\.[0-9]+)?)\\s*(pt|px)"));
    QString saida;
    qsizetype ultimo = 0;
    auto it = re.globalMatch(css);
    while (it.hasNext()) {
        const auto m = it.next();
        const double valor = m.captured(1).toDouble() * fator;
        const bool pt = m.captured(2) == QLatin1String("pt");
        // pt com 1 casa decimal; px inteiro
        const QString numero = pt ? QString::number(qRound(valor * 10) / 10.0, 'f', 1) : QString::number(qRound(valor));
        saida += css.mid(ultimo, m.capturedStart() - ultimo);
        saida += QStringLiteral("font-size: %1%2").arg(numero, m.captured(2));
        ultimo = m.capturedEnd();
    }
    saida += css.mid(ultimo);

    // forma abreviada: "font: 700 9pt \"Segoe UI\"" (estilo, peso e tamanho na mesma propriedade)
    static const QRegularExpression abreviada(QStringLiteral(
        "\\bfont\\s*:\\s*((?:(?:italic|oblique|normal|bold|bolder|lighter)\\s+|[1-9]00\\s+)*)([0-9]+(?:\\.[0-9]+)?)(pt|px)"));
    QString final_;
    qsizetype ult2 = 0;
    auto it2 = abreviada.globalMatch(saida);
    while (it2.hasNext()) {
        const auto m = it2.next();
        const double valor = m.captured(2).toDouble() * fator;
        const bool pt = m.captured(3) == QLatin1String("pt");
        const QString numero = pt ? QString::number(qRound(valor * 10) / 10.0, 'f', 1) : QString::number(qRound(valor));
        final_ += saida.mid(ult2, m.capturedStart() - ult2);
        final_ += QStringLiteral("font: %1%2%3").arg(m.captured(1), numero, m.captured(3));
        ult2 = m.capturedEnd();
    }
    final_ += saida.mid(ult2);
    return final_;
}

void Tema::escalarFontesDeClasse(double fator, const QString &familia)
{
    static const char *classes[] = {
        "QAbstractItemView", "QTableView", "QTreeView", "QListView", "QTableWidget", "QTreeWidget", "QListWidget",
        "QHeaderView", "QMenu", "QMenuBar", "QStatusBar", "QMessageBox", "QDialogButtonBox",
        "QPushButton", "QToolButton", "QAbstractButton", "QCheckBox", "QRadioButton", "QGroupBox", "QTabBar", "QTabWidget",
        "QLabel", "QLineEdit", "QTextEdit", "QPlainTextEdit", "QTextBrowser", "QComboBox", "QAbstractSpinBox", "QSpinBox",
        "QDoubleSpinBox", "QDateEdit", "QTimeEdit", "QDateTimeEdit", "QCalendarWidget", "QProgressBar", "QDockWidget",
        "QToolTip", "QTipLabel"};
    for (const char *nome : classes) {
        QFont f = QApplication::font(nome); // a da plataforma, se existir; senão, a geral
        if (f.pointSizeF() > 0)
            f.setPointSizeF(f.pointSizeF() * fator);
        else if (f.pixelSize() > 0)
            f.setPixelSize(qRound(f.pixelSize() * fator));
        if (!familia.isEmpty())
            f.setFamily(familia);
        QApplication::setFont(f, nome);
    }
}

QString Tema::folhaDeFontesDeClasse(double fator)
{
    static const char *classes[] = {
        "QMenuBar", "QMenu", "QStatusBar", "QToolTip", "QTabBar", "QHeaderView",
        "QAbstractItemView", "QTableView", "QTreeView", "QListView",
        "QLineEdit", "QTextEdit", "QPlainTextEdit", "QComboBox", "QAbstractSpinBox", "QSpinBox", "QDoubleSpinBox",
        "QDateEdit", "QTimeEdit", "QDateTimeEdit", "QCheckBox", "QRadioButton", "QGroupBox", "QCalendarWidget"};
    QString css;
    for (const char *nome : classes) {
        const QFont f = QApplication::font(nome);
        // o tema do sistema pode dar o tamanho em pontos OU em pixels; se não der nenhum dos dois, assume 9 pt
        // (padrão do Windows). Nunca pula a classe: pular deixaria tabelas e menus no tamanho antigo.
        if (f.pointSizeF() <= 0 && f.pixelSize() > 0)
            css += QStringLiteral("%1 { font-size: %2px; }\n").arg(QLatin1String(nome), QString::number(qRound(f.pixelSize() * fator)));
        else {
            const double pt = f.pointSizeF() > 0 ? f.pointSizeF() : 9.0;
            css += QStringLiteral("%1 { font-size: %2pt; }\n").arg(QLatin1String(nome), QString::number(qRound(pt * fator * 10) / 10.0, 'f', 1));
        }
    }
    return css;
}

void Tema::aplicar(QApplication &app)
{
    // fonte única e legível (no Windows a Segoe UI; em outros sistemas, a padrão)
    QFont fonte = app.font();
    if (QFontDatabase::families().contains(QStringLiteral("Segoe UI")))
        fonte.setFamily(QStringLiteral("Segoe UI"));
    const double fator = fatorFonte();
    // 1) a fonte normal (100%): é a partir dela que cada classe é medida
    fonte.setPointSize(10);
    app.setFont(fonte);
    // 2) com aumento: as fontes próprias da plataforma (tabelas, menus, cabeçalhos...) e a geral crescem pelo mesmo fator
    QString regrasDeFonte;
    if (fator > 1.0) {
        regrasDeFonte = folhaDeFontesDeClasse(fator); // lida ANTES de crescer: parte do tamanho atual de cada classe
        escalarFontesDeClasse(fator, fonte.family());
        fonte.setPointSizeF(10 * fator);
        app.setFont(fonte);
    }
    if (fator > 1.0)
        app.installEventFilter(new EscalaDeFonte(fator)); // vive até o fim do programa

    QPalette p = app.palette();
    p.setColor(QPalette::Window, QColor(kFundo));
    p.setColor(QPalette::Highlight, QColor(kPrimaria));
    p.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(p);

    // as regras por classe vêm depois da folha do tema: valem para tabelas, menus, cabeçalhos e campos em qualquer sistema
    app.setStyleSheet(escalarFolha(folhaDeEstilo(), fator) + QLatin1Char('\n') + regrasDeFonte);
}
