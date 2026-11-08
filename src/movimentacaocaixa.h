#ifndef MOVIMENTACAOCAIXA_H
#define MOVIMENTACAOCAIXA_H

#include <QDialog>
#include <QLocale>
#include "services/caixa_service.h"

namespace Ui {
class MovimentacaoCaixa;
}

class MovimentacaoCaixa : public QDialog
{
    Q_OBJECT

public:
    enum class Tipo { Sangria, Suprimento };
    explicit MovimentacaoCaixa(Tipo tipo, QWidget *parent = nullptr);
    ~MovimentacaoCaixa();

private slots:
    void on_Btn_Confirmar_clicked();
    void on_Btn_Cancelar_clicked();

private:
    Ui::MovimentacaoCaixa *ui;
    QLocale portugues;
    Caixa_service caixaServ;
    class QComboBox *cbForma = nullptr;
};

#endif // MOVIMENTACAOCAIXA_H
