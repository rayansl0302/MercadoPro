#ifndef TEST_MAQUINACARTAO_H
#define TEST_MAQUINACARTAO_H

#include <QObject>

// Maquininhas de cartão: cadastro, taxa gravada como custo (o cliente paga o preço de gôndola) e lucro.
class TestMaquinaCartao : public QObject
{
    Q_OBJECT
private slots:
    void cadastro_valida_nome_taxas_e_nome_repetido();
    void desativar_tira_da_lista_de_ativas_e_reativar_volta();
    void venda_no_cartao_grava_maquina_e_taxa_sem_mexer_no_valor_final();
    void venda_fora_do_cartao_nao_grava_taxa_de_maquina();
    void lucro_da_venda_desconta_a_taxa_da_maquina_e_mantem_o_legado();
    void custo_por_maquina_soma_as_taxas_do_periodo();
};

#endif // TEST_MAQUINACARTAO_H
