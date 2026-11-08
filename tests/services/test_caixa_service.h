#ifndef TEST_CAIXA_SERVICE_H
#define TEST_CAIXA_SERVICE_H

#include <QObject>

class TestCaixaService : public QObject
{
    Q_OBJECT
private slots:
    // cada teste começa sem caixa aberto neste terminal
    void init();
    // deixa um caixa aberto para o que rodar depois
    void cleanupTestCase();

    void pin_formato();
    void pin_bloqueia_na_terceira_tentativa();
    void tolerancia_usa_maior_entre_valor_e_percentual();
    void fechar_acima_da_tolerancia_exige_justificativa();

    void operador_nome_unico_e_pin_nao_fica_em_texto();
    void abrir_exige_pin_correto_e_operador_ativo();
    void abrir_recusa_segundo_caixa_no_terminal();
    void abrir_recusa_operador_com_caixa_em_outro_terminal();
    void banco_impede_dois_caixas_abertos();
    void sangria_e_suprimento_exigem_valor_e_motivo_e_entram_no_esperado();
    void sangria_pode_ser_editada_ou_removida_no_caixa_aberto();
    void esperado_soma_vendas_e_recebimentos_por_forma();
    void fechar_dentro_da_tolerancia_sem_justificativa_e_marca_ocorrencia_tecnica();
    void fechar_valida_formas_pin_e_nao_fecha_duas_vezes();
    void troco_sugerido_vem_do_dinheiro_contado_no_ultimo_fechamento();
    void cancelamento_segue_as_regras_do_caixa();
    void cancelamento_bloqueado_com_recebimento_em_caixa_fechado();
    void recebimento_de_caixa_fechado_nao_pode_ser_excluido();
    void desativar_operador_com_caixa_aberto_e_recusado();
    void pin_do_gerente();
    void gerente_precisa_de_identidade_no_cadastro();
    void login_por_pingeral_entra_como_gerente();
    void sessao_grava_entrada_e_saida();
    void venda_registra_o_operador_da_sessao_e_nao_o_dono_do_caixa();
    void sessao_expira_e_novo_login_continua_valido();
    void sessao_bloqueia_e_desbloqueia_com_pin_do_operador_ou_do_gerente();
    void sessao_cai_quando_operador_e_desativado();
    void sessao_com_venda_em_andamento_so_bloqueia();
    void autorizador_bloqueia_administracao_sem_gerente();
    void caixa_e_do_operador_logado_e_abre_sem_pin_novo();
};

#endif // TEST_CAIXA_SERVICE_H
