#ifndef BACKUPDIALOG_H
#define BACKUPDIALOG_H

#include <QDialog>

#include "services/backup_service.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTimeEdit;

// Backup do banco de dados: escolher a pasta (ex.: pasta do MEGA), quantos guardar, backup diário
// automático e "fazer agora". Só gerente abre.
class BackupDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BackupDialog(QWidget *parent = nullptr);

private:
    QLineEdit *edPasta;
    QSpinBox *spManter;
    QCheckBox *chAutomatico;
    QTimeEdit *edHora;
    QLineEdit *edPgDump;
    QLabel *lblPgDump;
    QPushButton *btnPgDump;
    QPushButton *btnAgora;
    QLabel *lblStatus;
    QLabel *lblResultado;
    QTableWidget *tabela;

    bool salvar();
    void escolherPasta();
    void escolherPgDump();
    void fazerAgora();
    void atualizarStatus();
    void atualizarLista();
    void mostrarResultado(const Backup_service::Resultado &r);
};

#endif // BACKUPDIALOG_H
