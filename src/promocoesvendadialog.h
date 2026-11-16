#ifndef PROMOCOESVENDADIALOG_H
#define PROMOCOESVENDADIALOG_H

#include <QDialog>
#include "services/promocaovenda_service.h"

class QTableWidget;
class QLabel;

// Cadastro das promoções de venda (ex.: "Promoção rede social"): o operador marca a promoção no fim da venda e a
// taxa da maquininha é anulada como desconto. Aqui só se cria, renomeia, liga (ON) / desliga (OFF) na mão e exclui.
class PromocoesVendaDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PromocoesVendaDialog(QWidget *parent = nullptr);

private:
    void recarregar();
    qlonglong idSelecionado() const;
    void nova();
    void renomear();
    void excluir();

    PromocaoVenda_service servico;
    QList<PromocaoVendaDTO> lista;
    QTableWidget *tabela = nullptr;
    QLabel *lblAviso = nullptr;
    bool carregando = false;
};

#endif // PROMOCOESVENDADIALOG_H
