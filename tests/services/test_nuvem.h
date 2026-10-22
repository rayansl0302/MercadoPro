#ifndef TEST_NUVEM_H
#define TEST_NUVEM_H

#include <QObject>

// Módulo Nuvem: contrato, exportação, ciclo com a API (servidor de mentira), pedidos do painel e trava.
class TestNuvem : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void contrato_so_lista_colunas_que_existem_e_nenhum_segredo();
    void checksum_bate_com_os_vetores_compartilhados_com_a_api();
    void normalizar_converte_tipos_para_o_texto_do_contrato();
    void configuracao_valida_url_e_token();
    void ciclo_completo_confere_contagens_e_nao_vaza_segredos();
    void falha_no_meio_do_envio_cancela_e_a_proxima_tentativa_funciona();
    void token_recusado_e_contrato_incompativel_viram_mensagem_clara();
    void pedido_de_baixa_e_aplicado_pelo_servico_e_registrado_como_painel();
    void pedido_repetido_nao_aplica_duas_vezes();
    void resultado_que_nao_chegou_e_reenviado_sem_reaplicar();
    void pedido_com_conta_ja_paga_e_conflito_e_pedido_vencido_expira();
    void pedido_de_lancar_conta_parcelada_e_pedido_invalido_e_recusado();
    void so_um_computador_sincroniza_e_a_trava_vencida_e_assumida();
    void nuvem_desligada_nao_faz_nada();
    void queda_passageira_so_vira_erro_na_terceira_vez_seguida_e_volta_ao_normal();
    void impressao_digital_muda_quando_uma_venda_e_cancelada_ou_o_estoque_mexe();
};

#endif // TEST_NUVEM_H
