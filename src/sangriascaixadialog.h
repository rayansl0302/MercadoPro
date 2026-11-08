#ifndef SANGRIASCAIXADIALOG_H
#define SANGRIASCAIXADIALOG_H

#include <QDialog>
#include <QList>
#include <QLocale>
#include "services/caixa_service.h"

class QTableWidget;
class QLabel;

// Sangrias e suprimentos do caixa aberto: histórico, novo lançamento, correção e remoção
// (lançou duas vezes, valor errado). Cada correção pede o motivo e fica no log.
class SangriasCaixaDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SangriasCaixaDialog(bool iniciarComSuprimento = false, QWidget *parent = nullptr);

private slots:
    void novaSangria();
    void novoSuprimento();
    void editar();
    void remover();

private:
    void recarregar();
    qlonglong idSelecionado();
    void novo(bool suprimento);

    Caixa_service caixaServ;
    QLocale portugues;
    QTableWidget *tabela = nullptr;
    QLabel *resumo = nullptr;
    QList<MovimentacaoCaixaDTO> itens;
};

#endif // SANGRIASCAIXADIALOG_H
