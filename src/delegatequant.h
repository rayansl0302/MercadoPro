#ifndef DELEGATEQUANT_H
#define DELEGATEQUANT_H

#include <QItemDelegate>
#include <QDoubleValidator>
#include <QLineEdit>
#include <QRegularExpressionValidator>

class DelegateQuant : public QItemDelegate
{
public:
    DelegateQuant(QObject *parent = nullptr);
    // "350g", "350 g", "0,35kg", "2,5", "2": devolve a quantidade (em kg/unidades); ok=false se não entender
    static double lerQuantidade(const QString &texto, bool *ok = nullptr);
protected:
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

#endif // DELEGATEQUANT_H
