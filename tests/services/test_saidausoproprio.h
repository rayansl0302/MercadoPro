#ifndef TEST_SAIDAUSOPROPRIO_H
#define TEST_SAIDAUSOPROPRIO_H

#include <QObject>

// Saída de estoque para uso próprio: baixa o estoque, grava custo e preço do momento e guarda o histórico.
class TestSaidaUsoProprio : public QObject
{
    Q_OBJECT
private slots:
    void saida_baixa_o_estoque_e_grava_custo_e_preco_do_momento();
    void quantidade_invalida_motivo_invalido_e_estoque_insuficiente_sao_recusados();
    void mudar_o_preco_depois_nao_reescreve_o_historico();
    void produto_sem_controle_de_estoque_registra_sem_baixar();
    void remover_saida_devolve_o_estoque_e_so_remove_uma_vez();
    void remover_saida_de_produto_sem_controle_nao_mexe_no_estoque();
    void historico_filtra_por_periodo_e_motivo_e_soma_o_custo();
};

#endif // TEST_SAIDAUSOPROPRIO_H
