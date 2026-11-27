#ifndef TEST_ENTREGA_H
#define TEST_ENTREGA_H

#include <QObject>
#include <QSqlDatabase>

// Pedido de entrega: preço por faixa de quantidade, valor em reais e lançamento da venda.
class TestEntrega : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void faixa_de_150_paes_muda_o_preco_para_030();
    void total_da_linha_usa_o_preco_da_faixa();
    void valor_em_reais_vira_quantidade_de_paes();
    void valor_em_reais_acima_da_faixa_usa_o_preco_da_faixa();
    void kg_aceita_quantidade_fracionada();
    void total_do_pedido_exemplo_fecha_em_49_08();

    void itens_padrao_sao_cadastrados_e_ligados_ao_estoque();
    void pedido_gravado_como_venda_marcada_como_entrega_e_baixa_estoque();
    void pedido_sem_quantidade_e_recusado();

private:
    QSqlDatabase db;
};

#endif // TEST_ENTREGA_H
