#ifndef PROMOCOESDIALOG_H
#define PROMOCOESDIALOG_H

#include <QDialog>
#include <QList>
#include "dto/Promocao_dto.h"

class QLineEdit;
class QTableWidget;

// Produtos em promoção (de/por): marcar e encerrar, imprimir as etiquetas de gôndola (A4) e gerar os posts do Instagram.
class PromocoesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PromocoesDialog(QWidget *parent = nullptr);

private slots:
    void adicionar();
    void alterar();
    void encerrar();
    void etiquetas();
    void posts();
    void copiarLegenda();

private:
    void recarregar();
    qlonglong idSelecionado() const;
    QString textoValidade() const;
    static QString dataCurta(const QString &iso);

    QTableWidget *tabela = nullptr;
    QLineEdit *validade = nullptr;
    QList<PromocaoDTO> promocoes;
};

#endif // PROMOCOESDIALOG_H
