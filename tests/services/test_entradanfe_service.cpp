#include <QtTest>
#include <QTemporaryDir>
#include <QSqlQuery>
#include "test_entradanfe_service.h"
#include "services/entradanfe_service.h"
#include "services/notafiscal_service.h"
#include "util/chaveacessoutil.h"
#include "util/nfxmlutil.h"
#include "services/estoquenota_service.h"
#include "services/produtonota_service.h"
#include <QFile>
#include "infra/databaseconnection_service.h"

namespace {

const QString kCnpjEmpresa = "11111111000191";
const QString kCnpjFornec  = "11222333000181";

QString gerarChave(int nnf, int modelo = 55, const QString &cnpj = kCnpjFornec)
{
    QString base = QString("292602") + cnpj + QString::number(modelo) + "001"
                   + QString("%1").arg(nnf, 9, 10, QChar('0')) + "1" + "12345678";
    return base + QString::number(ChaveAcessoUtil::calcularDigitoVerificador(base));
}

QByteArray gerarNfeProc(const QString &chave, const QString &cnpjEmit = kCnpjFornec,
                        const QString &cnpjDest = kCnpjEmpresa, const QString &cStat = "100",
                        bool comProtocolo = true)
{
    const QString protocolo = comProtocolo
        ? QString("<protNFe versao=\"4.00\"><infProt><tpAmb>1</tpAmb><chNFe>%1</chNFe>"
                  "<dhRecbto>2026-02-03T14:38:37-03:00</dhRecbto><nProt>129260000000001</nProt>"
                  "<cStat>%2</cStat><xMotivo>Autorizado o uso da NF-e</xMotivo></infProt></protNFe>")
              .arg(chave, cStat)
        : QString();

    QString xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<nfeProc versao=\"4.00\"><NFe><infNFe Id=\"NFe" + chave + "\" versao=\"4.00\">"
        "<ide><cUF>29</cUF><mod>" + chave.mid(20, 2) + "</mod><serie>1</serie><nNF>"
        + QString::number(chave.mid(25, 9).toInt()) + "</nNF>"
        "<dhEmi>2026-02-03T14:38:10-03:00</dhEmi><tpNF>1</tpNF><tpAmb>1</tpAmb></ide>"
        "<emit><CNPJ>" + cnpjEmit + "</CNPJ><xNome>FORNECEDOR TESTE LTDA</xNome>"
        "<enderEmit><xLgr>RUA A</xLgr><nro>10</nro><xBairro>CENTRO</xBairro><cMun>2927408</cMun>"
        "<xMun>SALVADOR</xMun><UF>BA</UF><CEP>40000000</CEP></enderEmit><IE>123456789</IE></emit>"
        "<dest><CNPJ>" + cnpjDest + "</CNPJ><xNome>MINHA LOJA</xNome></dest>"
        // item 1: PIS com alíquota (grupo que antes derrubava a leitura dos itens)
        "<det nItem=\"1\"><prod><cProd>001</cProd><cEAN>7891000000011</cEAN><xProd>CAFE 500G</xProd>"
        "<NCM>09012100</NCM><CFOP>5102</CFOP><uCom>UN</uCom><qCom>10.0000</qCom>"
        "<vUnCom>10.0000000000</vUnCom><vProd>100.00</vProd></prod>"
        "<imposto><ICMS><ICMSSN102><orig>0</orig><CSOSN>102</CSOSN></ICMSSN102></ICMS>"
        "<PIS><PISAliq><CST>01</CST><vBC>10.00</vBC><pPIS>1.65</pPIS><vPIS>0.17</vPIS></PISAliq></PIS></imposto></det>"
        // item 2: PIS não tributado
        "<det nItem=\"2\"><prod><cProd>002</cProd><cEAN>SEM GTIN</cEAN><xProd>ACUCAR 1KG</xProd>"
        "<NCM>17019900</NCM><CFOP>5102</CFOP><uCom>UN</uCom><qCom>5.0000</qCom>"
        "<vUnCom>4.0000000000</vUnCom><vProd>20.00</vProd></prod>"
        "<imposto><ICMS><ICMSSN102><orig>0</orig><CSOSN>102</CSOSN></ICMSSN102></ICMS>"
        "<PIS><PISNT><CST>04</CST></PISNT></PIS></imposto></det>"
        "<total><ICMSTot><vProd>120.00</vProd><vNF>120.00</vNF></ICMSTot></total>"
        "</infNFe></NFe>" + protocolo + "</nfeProc>";
    return xml.toUtf8();
}

qlonglong contar(const QString &sql)
{
    DatabaseConnection_service::open();
    QSqlQuery q(DatabaseConnection_service::db());
    q.exec(sql);
    return q.next() ? q.value(0).toLongLong() : -1;
}

EntradaNfe_service *criar(const QTemporaryDir &dir)
{
    auto *svc = new EntradaNfe_service;
    svc->setPastaBase(dir.path());
    svc->setCnpjEmpresa(kCnpjEmpresa);
    return svc;
}

} // namespace

void TestEntradaNfeService::cleanup()
{
    DatabaseConnection_service::open();
    QSqlQuery q(DatabaseConnection_service::db());
    q.exec("DELETE FROM eventos_fiscais");
    q.exec("DELETE FROM produtos_nota");
    q.exec("DELETE FROM notas_fiscais");
    q.exec("DELETE FROM clientes WHERE cpf = '" + kCnpjFornec + "'");
}

void TestEntradaNfeService::importa_xml_ok()
{
    QTemporaryDir dir;
    QScopedPointer<EntradaNfe_service> svc(criar(dir));
    const QString chave = gerarChave(1);

    auto r = svc->importarConteudo(gerarNfeProc(chave));
    QVERIFY2(r.ok, qPrintable(r.msg));
    QVERIFY(r.idNota > 0);

    QCOMPARE(contar(QString("SELECT COUNT(*) FROM notas_fiscais WHERE chnfe = '%1' "
                            "AND finalidade = 'ENTRADA EXTERNA' AND cstat = '100'").arg(chave)), 1LL);
    QCOMPARE(contar(QString("SELECT COUNT(*) FROM produtos_nota WHERE id_nf = %1").arg(r.idNota)), 2LL);
    QCOMPARE(contar(QString("SELECT COUNT(*) FROM clientes WHERE cpf = '%1'").arg(kCnpjFornec)), 1LL);

    // arquivo gravado e caminho relativo no banco
    const QString relativo = QString("xmlNf/entradas/%1-nfe.xml").arg(chave);
    QVERIFY(QFile::exists(dir.path() + "/" + relativo));
    NotaFiscal_service nfServ;
    QCOMPARE(nfServ.getNotaById(r.idNota).xmlPath, relativo);
}

void TestEntradaNfeService::importa_nota_duplicada_nao_duplica()
{
    QTemporaryDir dir;
    QScopedPointer<EntradaNfe_service> svc(criar(dir));
    const QString chave = gerarChave(2);

    auto r1 = svc->importarConteudo(gerarNfeProc(chave));
    QVERIFY2(r1.ok, qPrintable(r1.msg));
    auto r2 = svc->importarConteudo(gerarNfeProc(chave));

    QVERIFY(!r2.ok);
    QCOMPARE(r2.erro, EntradaNfeErro::Duplicada);
    QCOMPARE(r2.idNota, r1.idNota);
    QCOMPARE(contar(QString("SELECT COUNT(*) FROM notas_fiscais WHERE chnfe = '%1'").arg(chave)), 1LL);
    QCOMPARE(contar(QString("SELECT COUNT(*) FROM produtos_nota WHERE id_nf = %1").arg(r1.idNota)), 2LL);
}

void TestEntradaNfeService::destinatario_diferente_bloqueia_e_permite_com_confirmacao()
{
    QTemporaryDir dir;
    QScopedPointer<EntradaNfe_service> svc(criar(dir));
    const QString chave = gerarChave(3);
    const QByteArray xml = gerarNfeProc(chave, kCnpjFornec, "99888777000166");

    auto r1 = svc->importarConteudo(xml);
    QVERIFY(!r1.ok);
    QCOMPARE(r1.erro, EntradaNfeErro::DestinatarioDiferente);
    QCOMPARE(contar(QString("SELECT COUNT(*) FROM notas_fiscais WHERE chnfe = '%1'").arg(chave)), 0LL);

    auto r2 = svc->importarConteudo(xml, /*ignorarDestinatario=*/true);
    QVERIFY2(r2.ok, qPrintable(r2.msg));
}

void TestEntradaNfeService::nota_cancelada_e_recusada()
{
    QTemporaryDir dir;
    QScopedPointer<EntradaNfe_service> svc(criar(dir));
    auto r = svc->importarConteudo(gerarNfeProc(gerarChave(4), kCnpjFornec, kCnpjEmpresa, "101"));
    QVERIFY(!r.ok);
    QCOMPARE(r.erro, EntradaNfeErro::NaoAutorizada);
    QVERIFY(r.msg.contains("101"));
}

void TestEntradaNfeService::xml_sem_protocolo_e_recusado()
{
    QTemporaryDir dir;
    QScopedPointer<EntradaNfe_service> svc(criar(dir));
    auto r = svc->importarConteudo(gerarNfeProc(gerarChave(5), kCnpjFornec, kCnpjEmpresa, "100", false));
    QVERIFY(!r.ok);
    QCOMPARE(r.erro, EntradaNfeErro::NaoAutorizada);
    QVERIFY(r.msg.contains("protocolo"));
}

void TestEntradaNfeService::nfce_e_recusada()
{
    QTemporaryDir dir;
    QScopedPointer<EntradaNfe_service> svc(criar(dir));
    auto r = svc->importarConteudo(gerarNfeProc(gerarChave(6, 65)));
    QVERIFY(!r.ok);
    QCOMPARE(r.erro, EntradaNfeErro::NaoEhNfe);
    QVERIFY(r.msg.contains("NFC-e"));
}

void TestEntradaNfeService::xml_invalido_e_recusado()
{
    QTemporaryDir dir;
    QScopedPointer<EntradaNfe_service> svc(criar(dir));
    QCOMPARE(svc->importarConteudo("isto nao e xml").erro, EntradaNfeErro::ArquivoInvalido);
    QCOMPARE(svc->importarConteudo("<resNFe><chNFe>1</chNFe></resNFe>").erro, EntradaNfeErro::NaoEhNfe);
    QCOMPARE(svc->importarXml(dir.path() + "/nao_existe.xml").erro, EntradaNfeErro::ArquivoInvalido);
}

void TestEntradaNfeService::nota_propria_e_recusada()
{
    QTemporaryDir dir;
    QScopedPointer<EntradaNfe_service> svc(criar(dir));
    const QString chave = gerarChave(7, 55, kCnpjEmpresa);
    auto r = svc->importarConteudo(gerarNfeProc(chave, kCnpjEmpresa, kCnpjEmpresa));
    QVERIFY(!r.ok);
    QCOMPARE(r.erro, EntradaNfeErro::NotaPropria);
}

void TestEntradaNfeService::completa_nota_que_so_tinha_resumo()
{
    QTemporaryDir dir;
    QScopedPointer<EntradaNfe_service> svc(criar(dir));
    const QString chave = gerarChave(8);

    // resumo, como o download automático (resNFe) grava
    NotaFiscalDTO resumo;
    resumo.chNfe = chave;
    resumo.cnpjEmit = kCnpjFornec;
    resumo.finalidade = "resNFe";
    resumo.cstat = "0";
    resumo.tpAmb = 1;
    resumo.saida = false;
    resumo.valorTotal = 120;
    resumo.dhEmi = "03/02/2026 14:38:10";
    NotaFiscal_service nfServ;
    QVERIFY(nfServ.salvarResNfe(resumo).ok);
    const qlonglong idResumo = nfServ.getIdFromChave(chave);
    QVERIFY(idResumo > 0);

    auto r = svc->importarConteudo(gerarNfeProc(chave));
    QVERIFY2(r.ok, qPrintable(r.msg));
    QCOMPARE(r.idNota, idResumo);
    QCOMPARE(contar(QString("SELECT COUNT(*) FROM notas_fiscais WHERE chnfe = '%1'").arg(chave)), 1LL);
    QCOMPARE(contar(QString("SELECT COUNT(*) FROM produtos_nota WHERE id_nf = %1").arg(idResumo)), 2LL);
    QCOMPARE(nfServ.getNotaById(idResumo).finalidade, QString("ENTRADA EXTERNA"));
}

void TestEntradaNfeService::importa_nota_com_pis_aliquota_sem_travar()
{
    QTemporaryDir dir;
    QScopedPointer<EntradaNfe_service> svc(criar(dir));
    auto r = svc->importarConteudo(gerarNfeProc(gerarChave(9)));
    QVERIFY2(r.ok, qPrintable(r.msg));
    QCOMPARE(contar(QString("SELECT COUNT(*) FROM produtos_nota WHERE id_nf = %1 AND pis = '01'").arg(r.idNota)), 1LL);
}

// ─── busca pela chave de acesso ──────────────────────────────────────────────

namespace {

QString retornoComDocumento(const QString &chave, const QString &schema, const QString &xml,
                            const QString &situacao = "1")
{
    return "[DistribuicaoDFe]\nCStat=138\nCUF=0\nMsg=Documento(s) localizado(s)\nXMotivo=Documento(s) localizado(s)\n"
           "tpAmb=1\nultNSU=000000000000986\n\n"
           "[ResDFe001]\nCNPJCPF=" + kCnpjFornec + "\nNSU=000000000000971\nXML=" + xml + "\n"
           "arquivo=/tmp/inexistente.xml\ncSitNFe=" + situacao + "\nchDFe=" + chave + "\nschema=" + schema + "\nvNF=120,00\n";
}

QString retornoProc(const QString &chave, const QString &situacao = "1")
{
    return retornoComDocumento(chave, "procNFe", QString::fromUtf8(gerarNfeProc(chave)), situacao);
}

QString retornoResumo(const QString &chave)
{
    return retornoComDocumento(chave, "resNFe",
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<resNFe versao=\"1.01\"><chNFe>" + chave
        + "</chNFe><CNPJ>" + kCnpjFornec + "</CNPJ><xNome>FORNECEDOR TESTE LTDA</xNome><vNF>120.00</vNF></resNFe>");
}

QString retornoSoCabecalho(const QString &cStat, const QString &motivo)
{
    return "[DistribuicaoDFe]\nCStat=" + cStat + "\nXMotivo=" + motivo + "\ntpAmb=1\n";
}

EventoFiscalDTO cienciaComStatus(const QString &cstat)
{
    EventoFiscalDTO e;
    e.tipoEvento = "Ciencia de Operacao";
    e.idLote = 1;
    e.cstat = cstat;
    e.justificativa = "Evento registrado";
    e.nProt = "891260000000001";
    e.idNf = 0;
    e.codigo = "210210";
    return e;
}

} // namespace

void TestEntradaNfeService::busca_chave_invalida_nao_consulta_sefaz()
{
    QTemporaryDir dir;
    QScopedPointer<EntradaNfe_service> svc(criar(dir));
    int consultas = 0;
    svc->setConsultaChave([&](const QString &) { ++consultas; return QString(); });

    auto r = svc->buscarPorChave(gerarChave(20).left(43) + "0");
    QVERIFY(!r.ok);
    QCOMPARE(r.erro, EntradaNfeErro::ChaveInvalida);
    QCOMPARE(consultas, 0);
}

void TestEntradaNfeService::busca_nota_ja_lancada_seleciona_sem_consultar()
{
    QTemporaryDir dir;
    QScopedPointer<EntradaNfe_service> svc(criar(dir));
    const QString chave = gerarChave(21);
    auto imp = svc->importarConteudo(gerarNfeProc(chave));
    QVERIFY2(imp.ok, qPrintable(imp.msg));

    int consultas = 0;
    svc->setConsultaChave([&](const QString &) { ++consultas; return QString(); });
    auto r = svc->buscarPorChave(chave);

    QVERIFY(!r.ok);
    QCOMPARE(r.erro, EntradaNfeErro::Duplicada);
    QCOMPARE(r.idNota, imp.idNota);
    QCOMPARE(consultas, 0);
    QCOMPARE(contar(QString("SELECT COUNT(*) FROM notas_fiscais WHERE chnfe = '%1'").arg(chave)), 1LL);
}

void TestEntradaNfeService::busca_devolve_procnfe_e_grava()
{
    QTemporaryDir dir;
    QScopedPointer<EntradaNfe_service> svc(criar(dir));
    const QString chave = gerarChave(22);
    int consultas = 0, ciencias = 0;
    svc->setConsultaChave([&](const QString &) { ++consultas; return retornoProc(chave); });
    svc->setEnviarCiencia([&](const QString &) { ++ciencias; return cienciaComStatus("135"); });

    auto r = svc->buscarPorChave(gerarChave(22));
    QVERIFY2(r.ok, qPrintable(r.msg));
    QCOMPARE(consultas, 1);
    QCOMPARE(ciencias, 0);
    QCOMPARE(contar(QString("SELECT COUNT(*) FROM produtos_nota WHERE id_nf = %1").arg(r.idNota)), 2LL);
    QVERIFY(QFile::exists(dir.path() + QString("/xmlNf/entradas/%1-nfe.xml").arg(chave)));
}

void TestEntradaNfeService::busca_so_resumo_envia_ciencia_e_consulta_de_novo()
{
    QTemporaryDir dir;
    QScopedPointer<EntradaNfe_service> svc(criar(dir));
    const QString chave = gerarChave(23);
    int consultas = 0, ciencias = 0;
    svc->setConsultaChave([&](const QString &) {
        ++consultas;
        return consultas == 1 ? retornoResumo(chave) : retornoProc(chave);
    });
    svc->setEnviarCiencia([&](const QString &) { ++ciencias; return cienciaComStatus("135"); });

    auto r = svc->buscarPorChave(chave);
    QVERIFY2(r.ok, qPrintable(r.msg));
    QCOMPARE(consultas, 2);
    QCOMPARE(ciencias, 1);
    QCOMPARE(contar(QString("SELECT COUNT(*) FROM produtos_nota WHERE id_nf = %1").arg(r.idNota)), 2LL);
    // a ciência fica registrada na nota
    QCOMPARE(contar(QString("SELECT COUNT(*) FROM eventos_fiscais WHERE id_nf = %1").arg(r.idNota)), 1LL);
}

void TestEntradaNfeService::busca_resumo_persistente_aguarda_sem_lancar_nada()
{
    QTemporaryDir dir;
    QScopedPointer<EntradaNfe_service> svc(criar(dir));
    const QString chave = gerarChave(24);
    int consultas = 0, ciencias = 0;
    svc->setConsultaChave([&](const QString &) { ++consultas; return retornoResumo(chave); });
    svc->setEnviarCiencia([&](const QString &) { ++ciencias; return cienciaComStatus("135"); });

    auto r = svc->buscarPorChave(chave);
    QVERIFY(!r.ok);
    QCOMPARE(r.erro, EntradaNfeErro::AguardandoXml);
    QVERIFY(r.msg.contains("aguarde"));
    // no máximo 2 consultas e 1 ciência: nunca insiste sozinho
    QCOMPARE(consultas, 2);
    QCOMPARE(ciencias, 1);
    QCOMPARE(contar(QString("SELECT COUNT(*) FROM notas_fiscais WHERE chnfe = '%1'").arg(chave)), 0LL);
}

void TestEntradaNfeService::busca_ciencia_rejeitada_explica()
{
    QTemporaryDir dir;
    QScopedPointer<EntradaNfe_service> svc(criar(dir));
    const QString chave = gerarChave(25);
    int consultas = 0;
    svc->setConsultaChave([&](const QString &) { ++consultas; return retornoResumo(chave); });
    svc->setEnviarCiencia([&](const QString &) { return cienciaComStatus("-1"); });

    auto r = svc->buscarPorChave(chave);
    QVERIFY(!r.ok);
    QCOMPARE(r.erro, EntradaNfeErro::SefazRejeitou);
    QVERIFY(r.msg.contains("Ciência"));
    QCOMPARE(consultas, 1); // não consulta de novo se a ciência falhou
}

void TestEntradaNfeService::busca_traduz_erros_da_sefaz()
{
    QTemporaryDir dir;
    QScopedPointer<EntradaNfe_service> svc(criar(dir));
    const QString chave = gerarChave(26);

    auto r137 = svc->processarRetornoDistribuicao(retornoSoCabecalho("137", "Nenhum documento localizado"), chave);
    QCOMPARE(r137.erro, EntradaNfeErro::SefazRejeitou);
    QVERIFY(r137.msg.contains("não encontrou"));

    auto r656 = svc->processarRetornoDistribuicao(retornoSoCabecalho("656", "Rejeicao: Consumo Indevido"), chave);
    QCOMPARE(r656.erro, EntradaNfeErro::SefazRejeitou);
    QVERIFY(r656.msg.contains("656"));
    QVERIFY(r656.msg.contains("1 hora"));

    auto r593 = svc->processarRetornoDistribuicao(retornoSoCabecalho("593", "CNPJ-Base difere do certificado"), chave);
    QVERIFY(r593.msg.contains("certificado"));

    auto rOutro = svc->processarRetornoDistribuicao(
        retornoSoCabecalho("999", "CNPJ do interessado nao e destinatario"), chave);
    QVERIFY(rOutro.msg.contains("999"));
    QVERIFY(rOutro.msg.contains("destinatária"));

    auto rVazio = svc->processarRetornoDistribuicao("", chave);
    QCOMPARE(rVazio.erro, EntradaNfeErro::SefazRejeitou);
}

void TestEntradaNfeService::busca_nota_cancelada_na_sefaz_e_recusada()
{
    QTemporaryDir dir;
    QScopedPointer<EntradaNfe_service> svc(criar(dir));
    const QString chave = gerarChave(27);
    auto r = svc->processarRetornoDistribuicao(retornoProc(chave, "3"), chave);
    QVERIFY(!r.ok);
    QCOMPARE(r.erro, EntradaNfeErro::NaoAutorizada);
    QVERIFY(r.msg.contains("cancelada"));
    QCOMPARE(contar(QString("SELECT COUNT(*) FROM notas_fiscais WHERE chnfe = '%1'").arg(chave)), 0LL);
}

void TestEntradaNfeService::nota_de_fornecedor_do_regime_normal_grava_itens_sem_csosn_como_nulo()
{
    // Nota de fornecedor do regime normal (CRT 3): ICMS com CST (20, 40), sem CSOSN, e itens sem GTIN.
    // No PostgreSQL a coluna csosn é inteira e recusa texto vazio: isso derrubava a importação inteira.
    const QByteArray xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?><nfeProc versao=\"4.00\" xmlns=\"http://www.portalfiscal.inf.br/nfe\"><NFe><infNFe versao=\"4.00\">"
        "<det nItem=\"1\"><prod><cProd>599</cProd><cEAN>SEM GTIN</cEAN><xProd>ACUCAR CRISTAL TESTE REGIME NORMAL</xProd><NCM>17019900</NCM>"
        "<CFOP>5102</CFOP><uCom>KG</uCom><qCom>90.0000</qCom><vUnCom>2.8800000000</vUnCom></prod>"
        "<imposto><ICMS><ICMS20><orig>0</orig><CST>20</CST><pICMS>20.5000</pICMS></ICMS20></ICMS><PIS><PISNT><CST>06</CST></PISNT></PIS></imposto></det>"
        "<det nItem=\"2\"><prod><cProd>393</cProd><cEAN>SEM GTIN</cEAN><xProd>ARROZ BRANCO TESTE REGIME NORMAL</xProd><NCM>10063021</NCM>"
        "<CFOP>5102</CFOP><uCom>KG</uCom><qCom>90.0000</qCom><vUnCom>4.3000000000</vUnCom></prod>"
        "<imposto><ICMS><ICMS40><orig>0</orig><CST>40</CST></ICMS40></ICMS></imposto></det>"   // sem grupo de PIS: pis vazio também
        "</infNFe></NFe></nfeProc>";
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString caminho = tmp.filePath("regime_normal.xml");
    {
        QFile f(caminho);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(xml);
    }
    NfXmlUtil util;
    const QList<ProdutoNotaDTO> itens = util.carregarProdutosDaNFe(caminho, 0);
    QCOMPARE(itens.size(), 2);
    QVERIFY(itens.at(0).csosn.isEmpty());          // regime normal: sem CSOSN
    QCOMPARE(itens.at(0).cstIcms, QString("20"));

    ProdutoNota_service servico;
    const auto r = servico.inserirListaProdutos(itens);
    QVERIFY2(r.ok, qPrintable(r.msg));

    QSqlQuery q(DatabaseConnection_service::db());
    QVERIFY(q.exec("SELECT descricao, csosn, pis, cst_icms FROM produtos_nota WHERE descricao LIKE '%TESTE REGIME NORMAL' ORDER BY nitem"));
    QVERIFY(q.next());
    QVERIFY2(q.value(1).isNull(), "csosn deve ficar NULL, e não texto vazio");
    QCOMPARE(q.value(2).toInt(), 6);                 // PISNT CST 06
    QCOMPARE(q.value(3).toString(), QString("20"));
    QVERIFY(q.next());
    QVERIFY2(q.value(1).isNull(), "csosn NULL");
    QVERIFY2(q.value(2).isNull(), "pis ausente também vira NULL");
    QCOMPARE(q.value(3).toString(), QString("40"));
}

void TestEntradaNfeService::digito_verificador_do_gtin_confere_ean8_ean13_e_upc()
{
    for (const char *ok : {"7891000100103", "4006381333931", "7898079990017", "96385074", "036000291452"})
        QVERIFY2(EstoqueNota_service::digitoVerificadorGtinConfere(ok), ok);
    for (const char *ruim : {"7891000100104", "4006381333932", "SEM GTIN", "", "123", "00000000", "78910001001O3"})
        QVERIFY2(!EstoqueNota_service::digitoVerificadorGtinConfere(ruim), ruim);
    QVERIFY(EstoqueNota_service::digitoVerificadorGtinConfere(" 7891000100103 "));   // espaços do leitor
}

void TestEntradaNfeService::codigo_lido_e_gravado_no_item_pendente_mas_nao_no_que_ja_foi_lancado()
{
    ProdutoNota_service servico;
    auto novo = [](const QString &desc, int nitem) {
        ProdutoNotaDTO p;
        p.descricao = desc;
        p.nitem = nitem;
        p.quantidade = 2;
        p.preco = 3.5;
        p.codigoBarras = "SEM GTIN";
        p.uCom = "UN";
        p.cfop = "5102";
        return p;
    };
    QVERIFY(servico.inserirListaProdutos({novo("ITEM LEITURA A", 1), novo("ITEM LEITURA B", 2)}).ok);

    QSqlQuery q(DatabaseConnection_service::db());
    auto idDe = [&](const QString &desc) {
        q.exec(QString("SELECT id FROM produtos_nota WHERE descricao = '%1'").arg(desc));
        return q.next() ? q.value(0).toLongLong() : -1LL;
    };
    const qlonglong a = idDe("ITEM LEITURA A"), b = idDe("ITEM LEITURA B");
    QVERIFY(a > 0 && b > 0);

    QVERIFY(servico.atualizarCodigoBarras(a, " 7891000100103 "));
    QCOMPARE(servico.getProdutoNota(a).codigoBarras, QString("7891000100103"));       // espaços do leitor removidos
    QCOMPARE(servico.getProdutoNota(b).codigoBarras, QString("SEM GTIN"));            // o outro item não é tocado

    // item já lançado no estoque: o código não muda mais
    QVERIFY(servico.marcarComoAdicionado(b));
    QVERIFY(!servico.atualizarCodigoBarras(b, "4006381333931"));
    QCOMPARE(servico.getProdutoNota(b).codigoBarras, QString("SEM GTIN"));
    QVERIFY(!servico.atualizarCodigoBarras(999999999, "7891000100103"));              // id que não existe
}
