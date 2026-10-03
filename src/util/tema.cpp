#include "tema.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QPalette>

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

} // namespace

void Tema::aplicar(QApplication &app)
{
    // fonte única e legível (no Windows a Segoe UI; em outros sistemas, a padrão)
    QFont fonte = app.font();
    if (QFontDatabase::families().contains(QStringLiteral("Segoe UI")))
        fonte.setFamily(QStringLiteral("Segoe UI"));
    fonte.setPointSize(10);
    app.setFont(fonte);

    QPalette p = app.palette();
    p.setColor(QPalette::Window, QColor(kFundo));
    p.setColor(QPalette::Highlight, QColor(kPrimaria));
    p.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(p);

    app.setStyleSheet(folhaDeEstilo());
}
