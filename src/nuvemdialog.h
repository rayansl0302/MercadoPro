#ifndef NUVEMDIALOG_H
#define NUVEMDIALOG_H

#include <QDialog>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

// Configuração e acompanhamento da sincronização com o painel online (módulo Nuvem). Só gerente abre.
class NuvemDialog : public QDialog
{
    Q_OBJECT
public:
    explicit NuvemDialog(QWidget *parent = nullptr);

private:
    QCheckBox *chAtiva;
    QCheckBox *chSincroniza;
    QLineEdit *edUrl;
    QLineEdit *edToken;
    QCheckBox *chMostrarToken;
    QSpinBox *spIntervalo;
    QLabel *lblStatus;
    QLabel *lblResultado;
    QTableWidget *tabelaLog;
    QPushButton *btnSincronizar;
    QPushButton *btnTestar;

    bool salvar();
    void atualizarStatus();
    void atualizarLog();
    void testar();
    void sincronizarAgora();
    void ocupado(bool sim);
};

#endif // NUVEMDIALOG_H
