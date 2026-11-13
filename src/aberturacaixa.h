#ifndef ABERTURACAIXA_H
#define ABERTURACAIXA_H

#include <QDialog>
#include <QLocale>
#include "services/caixa_service.h"
#include "services/operador_service.h"

namespace Ui {
class AberturaCaixa;
}

class AberturaCaixa : public QDialog
{
    Q_OBJECT

public:
    explicit AberturaCaixa(QWidget *parent = nullptr);
    ~AberturaCaixa();
    qlonglong idCaixaAberto() const { return idCaixa; }

    // true se já há caixa aberto neste terminal; senão oferece abrir agora (usado antes de vender)
    static bool garantirCaixaAberto(QWidget *parent);
    // Caixa vencido (mais de 15 h ou de outro dia): obriga o operador a fechá-lo. Devolve true se não há
    // mais caixa vencido (já estava ok ou acabou de ser fechado).
    static bool exigirFechamentoDoVencido(QWidget *parent);

private slots:
    void on_Btn_Abrir_clicked();
    void on_Btn_Cancelar_clicked();
    void on_Btn_Operadores_clicked();

private:
    Ui::AberturaCaixa *ui;
    QLocale portugues;
    Caixa_service caixaServ;
    Operador_service operadorServ;
    qlonglong idCaixa = 0;
    double trocoSugerido = 0;

    void carregarOperadores();
};

#endif // ABERTURACAIXA_H
