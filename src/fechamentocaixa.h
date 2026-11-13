#ifndef FECHAMENTOCAIXA_H
#define FECHAMENTOCAIXA_H

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMap>
#include "services/caixa_service.h"

namespace Ui {
class FechamentoCaixa;
}

class FechamentoCaixa : public QDialog
{
    Q_OBJECT

public:
    // idCaixaAlvo = 0: fecha o caixa aberto deste terminal. Com um id, fecha aquele caixa (ex.: turno
    // esquecido aberto em outro computador), sempre com o PIN de quem o abriu.
    explicit FechamentoCaixa(QWidget *parent = nullptr, qlonglong idCaixaAlvo = 0);
    ~FechamentoCaixa();
    bool fechou() const { return fechouOk; }
    qlonglong idCaixaFechado() const { return idCaixa; }

private slots:
    void on_Btn_Fechar_clicked();
    void on_Btn_Cancelar_clicked();
    void recalcularDiferencas();
    void atualizarDeposito();

private:
    Ui::FechamentoCaixa *ui;
    QLocale portugues;
    Caixa_service caixaServ;
    ResumoCaixaDTO resumoAtual;
    qlonglong idCaixa = 0;
    bool fechouOk = false;

    QMap<QString, double> valoresInformados() const;
    double dinheiroQueFica() const;
    QLineEdit *ledFica = nullptr;
    QLabel *lblDeposito = nullptr;
};

#endif // FECHAMENTOCAIXA_H
