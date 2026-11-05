#include <QCoreApplication>
#include <QtTest>

#include "db/test_db_factory.h"
#include "infra/databaseconnection_service.h"
#include "services/schemamigration_service.h"
#include "services/test_cliente_service.h"
#include "services/test_produto_service.h"
#include "services/test_barcode_service.h"
#include "util/test_dbutil.h"
#include "util/test_chaveacessoutil.h"
#include "services/test_manifestadordfe.h"
#include "services/test_vendas_service.h"
#include "services/test_produtovenda_service.h"
#include "services/test_fiscalemitter_service.h"
#include "services/test_eventofiscal_service.h"
#include "services/test_entradanfe_service.h"
#include "services/test_caixa_service.h"
#include "services/test_empresa_contas.h"
#include "services/test_contasreceber_service.h"
#include "services/test_numeracaonf.h"
#include "services/test_nuvem.h"
#include "services/test_lanches.h"
#include "services/test_saidausoproprio.h"
#include "services/test_maquinacartao.h"
#include "services/test_precotaxa.h"
#include "services/test_pagamentodividido.h"
#include "services/test_backup.h"
#include "services/test_tema.h"
#include <QSqlDatabase>
#include <QDebug>
#include "nota/acbrmanager.h"

int main(int argc, char *argv[])
{
    AcbrManager::setTestMode(true);
    QApplication app(argc, argv);

    int status = 0;
    // QE_SO_TESTE=PrecoTaxa roda só as classes de teste cujo nome contém o texto (rodada rápida durante o desenvolvimento)
    const QString filtro = qEnvironmentVariable("QE_SO_TESTE");
    auto rodar = [&](QObject *teste) {
        if (filtro.isEmpty() || QString::fromLatin1(teste->metaObject()->className()).contains(filtro, Qt::CaseInsensitive))
            status |= QTest::qExec(teste, argc, argv);
    };
#ifdef TEST_ENV
#ifdef TEST_POSTGRES
    // Se a varivel de compilação "TEST_WITH_POSTGRES=ON" estiver sendo usada, criará um banco de dados postgre usando as
    // credenciais definidas em variaveis de sistema (ver definição da função)
    TestDbFactory::createPostgres();
#else
    TestDbFactory::create();
#endif
#endif//TEST_ENV

    rodar(new TestChaveAcessoUtil);
    rodar(new TestProdutoService);
    rodar(new test_barcode_service);
    rodar(new test_manifestadordfe);
    rodar(new test_cliente_service);
    rodar(new TestVendasService);
    rodar(new TestProdutoVendaService);
    rodar(new TestFiscalEmitterService);
    rodar(new test_eventofiscal_service);
    rodar(new TestEntradaNfeService);
    rodar(new TestCaixaService);
    rodar(new TestEmpresaContas);
    rodar(new TestContasReceber);
    rodar(new TestNumeracaoNF);
    rodar(new TestNuvem);
    rodar(new TestLanches);
    rodar(new TestSaidaUsoProprio);
    rodar(new TestMaquinaCartao);
    rodar(new TestPrecoTaxa);
    rodar(new TestPagamentoDividido);
    rodar(new TestBackup);
    rodar(new TestTema);

#ifdef TEST_POSTGRES
    TestDbFactory::removerBDAtual();
#endif

    return status;
}
