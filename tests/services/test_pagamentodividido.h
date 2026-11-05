#ifndef TEST_PAGAMENTODIVIDIDO_H
#define TEST_PAGAMENTODIVIDIDO_H

#include <QObject>

// Venda paga em mais de uma forma (Dinheiro + Pix...): conferência das partes, gravação, caixa e nota.
class TestPagamentoDividido : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void partes_conferem_e_o_troco_sai_do_dinheiro();
    void partes_invalidas_sao_recusadas();
    void venda_dividida_grava_as_partes_e_entra_no_esperado_do_caixa();
    void soma_diferente_do_valor_e_recusada_pelo_servico();
    void nota_fiscal_gera_uma_secao_por_forma_com_troco_na_ultima();
};

#endif // TEST_PAGAMENTODIVIDIDO_H
