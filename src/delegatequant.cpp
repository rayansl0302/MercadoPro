#include "delegatequant.h"
#include <QPainter>
#include <QPalette>


DelegateQuant::DelegateQuant(QObject *parent) : QItemDelegate(parent){}

QWidget *DelegateQuant::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // Cria um editor de texto (QLineEdit)
    QLineEdit *editor = new QLineEdit(parent);

    // número (vírgula ou ponto) com sufixo opcional: g, gr, kg — ex.: 350g, 0,35 kg, 2
    editor->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^\\s*[0-9]*[.,]?[0-9]*\\s*(g|gr|kg)?\\s*$"), QRegularExpression::CaseInsensitiveOption), editor));
    editor->setPlaceholderText(QStringLiteral("ex.: 2 ou 350g"));
    editor->setToolTip(QStringLiteral("Digite a quantidade (2, 0,35) ou o peso em gramas (350g)."));
    editor->setStyleSheet("background-color: lightblue;");

    return editor;  // Retorna o editor para edição
}

void DelegateQuant::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    QString currentText = index.data().toString();
    QLineEdit *lineEdit = static_cast<QLineEdit*>(editor);
    lineEdit->setText(currentText);  // Preenche o editor com o valor da célula
}

void DelegateQuant::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    QLineEdit *lineEdit = static_cast<QLineEdit*>(editor);
    bool ok = false;
    const double q = lerQuantidade(lineEdit->text(), &ok);
    if (!ok || q <= 0)
        return;   // texto inválido: mantém a quantidade anterior
    QLocale l(QLocale::Portuguese, QLocale::Brazil);
    l.setNumberOptions(l.numberOptions() | QLocale::OmitGroupSeparator);
    const bool inteira = qAbs(q - qRound64(q)) < 1e-9;
    QString txt = l.toString(q, 'f', inteira ? 0 : 3);
    if (!inteira && txt.endsWith('0'))
        txt.chop(1);   // 1,500 -> 1,50 ; 0,125 continua 0,125
    model->setData(index, txt);
}

double DelegateQuant::lerQuantidade(const QString &texto, bool *ok)
{
    static const QRegularExpression re(QStringLiteral("^\\s*([0-9]*[.,]?[0-9]+|[0-9]+[.,])\\s*(g|gr|kg)?\\s*$"),
                                       QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(texto);
    if (!m.hasMatch()) {
        if (ok) *ok = false;
        return 0;
    }
    QString n = m.captured(1);
    n.replace(',', '.');
    bool numOk = false;
    double v = n.toDouble(&numOk);
    const QString un = m.captured(2).toLower();
    if (un == QLatin1String("g") || un == QLatin1String("gr"))
        v /= 1000.0;
    if (ok) *ok = numOk;
    return numOk ? v : 0;
}

void DelegateQuant::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    editor->setGeometry(option.rect);  // Define a posição e o tamanho do editor na célula
}

void DelegateQuant::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem optionCopy = option;


    painter->fillRect(option.rect, QColor(230, 240, 255)); // Azul claro permanente


    // Altera a cor de fundo para um tom suave de azul
    optionCopy.backgroundBrush = QBrush(QColor(230, 240, 255)); // Azul suave


    // Chama o método de pintura padrão

    QItemDelegate ::paint(painter, optionCopy, index);
}


