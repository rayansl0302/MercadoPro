#ifndef TEST_ENTRADANFE_SERVICE_H
#define TEST_ENTRADANFE_SERVICE_H

#include <QObject>

class TestEntradaNfeService : public QObject
{
    Q_OBJECT
private slots:
    void nota_de_fornecedor_do_regime_normal_grava_itens_sem_csosn_como_nulo();
    void digito_verificador_do_gtin_confere_ean8_ean13_e_upc();
    void codigo_lido_e_gravado_no_item_pendente_mas_nao_no_que_ja_foi_lancado();
    void cleanup();

    void importa_xml_ok();
    void importa_nota_duplicada_nao_duplica();
    void destinatario_diferente_bloqueia_e_permite_com_confirmacao();
    void nota_cancelada_e_recusada();
    void xml_sem_protocolo_e_recusado();
    void nfce_e_recusada();
    void xml_invalido_e_recusado();
    void nota_propria_e_recusada();
    void completa_nota_que_so_tinha_resumo();
    void importa_nota_com_pis_aliquota_sem_travar();

    void busca_chave_invalida_nao_consulta_sefaz();
    void busca_nota_ja_lancada_seleciona_sem_consultar();
    void busca_devolve_procnfe_e_grava();
    void busca_so_resumo_envia_ciencia_e_consulta_de_novo();
    void busca_resumo_persistente_aguarda_sem_lancar_nada();
    void busca_ciencia_rejeitada_explica();
    void busca_traduz_erros_da_sefaz();
    void busca_nota_cancelada_na_sefaz_e_recusada();
};

#endif // TEST_ENTRADANFE_SERVICE_H
