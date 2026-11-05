#ifndef SANGRIASDIALOG_H
#define SANGRIASDIALOG_H

#include <QDialog>
#include "services/caixa_service.h"

class QDateEdit;
class QLabel;
class QTableWidget;

// Relatório de sangrias: o que já saiu do caixa no dia (ou período), com motivo e quem retirou.
class SangriasDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SangriasDialog(QWidget *parent = nullptr);

private:
    Caixa_service caixaServ;
    QDateEdit *dtDe = nullptr;
    QDateEdit *dtAte = nullptr;
    QTableWidget *tabela = nullptr;
    QLabel *lblTotal = nullptr;
    QList<MovimentacaoCaixaDTO> atuais;
    void atualizar();
    void imprimir();
};

#endif // SANGRIASDIALOG_H
