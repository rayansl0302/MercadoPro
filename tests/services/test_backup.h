#ifndef TEST_BACKUP_H
#define TEST_BACKUP_H

#include <QObject>

// Backup do banco para uma pasta escolhida (MEGA etc.): nomes, retenção, pasta inválida, cópia real.
class TestBackup : public QObject
{
    Q_OBJECT
private slots:
    void nome_do_arquivo_tem_data_e_extensao_do_banco();
    void pasta_vazia_inexistente_e_sem_permissao_sao_recusadas();
    void retencao_apaga_so_os_backups_mais_antigos_do_nosso_padrao();
    void lista_vem_do_mais_novo_para_o_mais_antigo();
    void pg_dump_nao_leva_a_senha_na_linha_de_comando();
    void caminho_do_pg_dump_informado_vale_primeiro();
    void backup_sqlite_gera_copia_consistente_sem_arquivo_parcial();
    void pasta_invalida_falha_com_motivo_e_nao_apaga_nada();
};

#endif // TEST_BACKUP_H
