#ifndef TEST_LANCHES_H
#define TEST_LANCHES_H

#include <QObject>

// Faixa de lanches (200-500), comanda térmica e auto-update.
class TestLanches : public QObject
{
    Q_OBJECT
private slots:
    void faixa_200_a_500_e_inclusiva_nas_pontas();
    void codigo_que_nao_e_inteiro_curto_nunca_vira_lanche();
    void carrinho_e_classificado_em_normal_lanche_ou_misto();
    void senha_do_dia_conta_so_vendas_de_lanche_ate_a_venda_atual();
    void comanda_tem_senha_itens_e_corte_e_nenhum_dado_fiscal();
    void comanda_quebra_nome_comprido_sem_estourar_a_largura();
    void comparar_versoes_trata_numeros_e_nao_texto();
    void release_so_vale_com_instalador_e_hash_publicados();
    void hash_e_lido_do_arquivo_sha256();
    void atualizacao_publicada_e_baixada_e_conferida();
    void instalador_com_hash_errado_e_descartado();
    void servidor_fora_do_ar_nao_trava_nem_levanta_erro();
    void versao_igual_ou_menor_nao_baixa_nada();
};

#endif // TEST_LANCHES_H
