#ifndef DESCONTOCPFDIALOG_H
#define DESCONTOCPFDIALOG_H

#include <QDialog>

class QCheckBox;
class QDoubleSpinBox;
class QDateEdit;

// Configura o desconto para cliente cadastrado (CPF): liga/desliga, percentual, período (campanha sazonal) e teto.
class DescontoCpfDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DescontoCpfDialog(QWidget *parent = nullptr);

private:
    void salvar();

    QCheckBox *chkAtivo = nullptr;
    QDoubleSpinBox *spnPercentual = nullptr;
    QCheckBox *chkPeriodo = nullptr;
    QDateEdit *dtDe = nullptr;
    QDateEdit *dtAte = nullptr;
    QDoubleSpinBox *spnTeto = nullptr;
};

#endif // DESCONTOCPFDIALOG_H
