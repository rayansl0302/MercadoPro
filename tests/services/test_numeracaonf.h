#ifndef TEST_NUMERACAONF_H
#define TEST_NUMERACAONF_H

#include <QObject>

// Numeração das notas fiscais com vários caixas: reserva sem repetição, por CNPJ.
class TestNumeracaoNF : public QObject
{
    Q_OBJECT
private slots:
    void sem_notas_comeca_em_1_ou_no_numero_configurado();
    void proximo_considera_as_notas_salvas_do_mesmo_cnpj();
    void reserva_da_chave_unica_impede_numero_repetido();
    void servico_reserva_numeros_distintos_e_troca_o_numero_ja_usado();
    void numero_liberado_volta_para_a_fila_e_o_confirmado_nao();
    void reserva_vencida_nao_trava_a_numeracao();
    void cada_cnpj_tem_a_sua_numeracao_e_nota_antiga_sem_cnpj_e_da_empresa_original();
};

#endif // TEST_NUMERACAONF_H
