#ifndef TEST_PROMOCAO_H
#define TEST_PROMOCAO_H

#include <QObject>

// Produto em promoção (de/por): preço da gôndola, início, fim, vencimento automático e posts.
class TestPromocao : public QObject
{
    Q_OBJECT
private slots:
    void preco_base_da_promocao_fecha_o_valor_exato_da_gondola();
    void promocao_troca_o_preco_do_cadastro_e_o_fim_devolve_o_de();
    void promocao_com_preco_invalido_ou_data_passada_e_recusada();
    void promocao_vencida_e_encerrada_sozinha();
    void post_e_legenda_trazem_de_por_e_desconto();
};

#endif // TEST_PROMOCAO_H
