#ifndef TEST_PROMOCAOVENDA_H
#define TEST_PROMOCAOVENDA_H

#include <QObject>

// Promoção de venda ("Promoção rede social"): anula a taxa da maquininha como desconto no fim da venda.
class TestPromocaoVenda : public QObject
{
    Q_OBJECT
private slots:
    void tirar_a_taxa_do_preco_de_venda_devolve_o_preco_base();
    void desconto_e_a_taxa_embutida_nos_itens_da_venda();
    void sem_taxa_de_maquininha_nao_ha_desconto();
    void cadastro_cria_recusa_repetido_desativa_e_exclui();
    void venda_grava_o_nome_da_promocao_usada();
};

#endif // TEST_PROMOCAOVENDA_H
