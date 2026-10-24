#ifndef ESTOQUENOTA_SERVICE_H
#define ESTOQUENOTA_SERVICE_H

#include <QObject>
#include <QStringList>
#include "produtonota_service.h"
#include "Produto_service.h"
#include "config_service.h"
#include "../util/nfxmlutil.h"
#include "../util/ibptutil.h"

struct ResumoEstoqueNota {
    int pendentes = 0;
    int comCodigo = 0;
    int semCodigo = 0;
    int cadastrados = 0;
    int atualizados = 0;
    int comCodigoInterno = 0;
    int semNf = 0;
    QStringList falhas;
};

// Lança no estoque, de uma vez, os itens de uma nota de compra que ainda não foram adicionados.
// Item com GTIN válido: soma ao produto de mesmo código ou cadastra um produto novo.
// Item sem GTIN: soma ao produto de mesma descrição ou cadastra com um código interno gerado.
class EstoqueNota_service : public QObject
{
    Q_OBJECT
public:
    explicit EstoqueNota_service(QObject *parent = nullptr);

    ResumoEstoqueNota contarPendentes(qlonglong idNota);
    ResumoEstoqueNota adicionarPendentes(qlonglong idNota);

    static bool ehGtinValido(const QString &codigo);
    // true se o último número é o dígito verificador certo do GTIN (EAN-8/12/13/14). Falso também para código inválido.
    static bool digitoVerificadorGtinConfere(const QString &codigo);

private:
    ProdutoNota_service prodNotaServ;
    Produto_Service prodServ;
    Config_service confServ;
    NfXmlUtil xmlUtil;
    IbptUtil ibpt;

    QList<ProdutoNotaDTO> itensPendentes(qlonglong idNota);
    double custoUnitario(const ProdutoNotaDTO &item);
    void adicionarItem(const ProdutoNotaDTO &item, const ConfigDTO &cfg, ResumoEstoqueNota &resumo);
    bool somarAoProduto(const ProdutoDTO &existente, const ProdutoNotaDTO &item, double custo,
                        const ConfigDTO &cfg, QString &erro);
    bool cadastrarProduto(const ProdutoNotaDTO &item, const QString &codigo, double custo,
                          const ConfigDTO &cfg, bool &semNf, QString &erro);
    QString gerarCodigoInternoLivre();
};

#endif // ESTOQUENOTA_SERVICE_H
