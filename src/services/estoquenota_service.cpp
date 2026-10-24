#include "estoquenota_service.h"
#include "../infra/apppath_service.h"
#include "../util/codigobarrasutil.h"
#include <QVariantMap>
#include <QRegularExpression>

namespace {
bool ncmValido(const QString &ncm)
{
    return ncm.size() == 8 && ncm != "00000000";
}
}

EstoqueNota_service::EstoqueNota_service(QObject *parent)
    : QObject{parent}
{}

bool EstoqueNota_service::ehGtinValido(const QString &codigo)
{
    const QString digitos = codigo.trimmed();
    static const QRegularExpression soDigitos("^\\d+$");
    if (!soDigitos.match(digitos).hasMatch())
        return false;
    const int tamanho = digitos.size();
    if (tamanho != 8 && tamanho != 12 && tamanho != 13 && tamanho != 14)
        return false;
    return digitos.count('0') != tamanho;
}

bool EstoqueNota_service::digitoVerificadorGtinConfere(const QString &codigo)
{
    const QString d = codigo.trimmed();
    if (!ehGtinValido(d))
        return false;
    const int n = d.size();
    int soma = 0;
    for (int i = 0; i < n - 1; ++i)      // da direita para a esquerda, a partir do número antes do dígito: peso 3, 1, 3, 1...
        soma += d.at(n - 2 - i).digitValue() * (i % 2 == 0 ? 3 : 1);
    return (10 - soma % 10) % 10 == d.at(n - 1).digitValue();
}

QList<ProdutoNotaDTO> EstoqueNota_service::itensPendentes(qlonglong idNota)
{
    QList<ProdutoNotaDTO> pendentes;
    for (const ProdutoNotaDTO &item : prodNotaServ.listarDtoPorNota(idNota)) {
        if (item.adicionado || item.status == "DEVOLVIDO" || item.descricao.trimmed().isEmpty())
            continue;
        pendentes << item;
    }
    return pendentes;
}

ResumoEstoqueNota EstoqueNota_service::contarPendentes(qlonglong idNota)
{
    ResumoEstoqueNota resumo;
    for (const ProdutoNotaDTO &item : itensPendentes(idNota)) {
        ++resumo.pendentes;
        if (ehGtinValido(item.codigoBarras))
            ++resumo.comCodigo;
        else
            ++resumo.semCodigo;
    }
    return resumo;
}

ResumoEstoqueNota EstoqueNota_service::adicionarPendentes(qlonglong idNota)
{
    ResumoEstoqueNota resumo = contarPendentes(idNota);
    const ConfigDTO cfg = confServ.carregarTudo();
    for (const ProdutoNotaDTO &item : itensPendentes(idNota))
        adicionarItem(item, cfg, resumo);
    return resumo;
}

double EstoqueNota_service::custoUnitario(const ProdutoNotaDTO &item)
{
    const QString xmlPath = AppPath_service::resolverXmlPath(prodNotaServ.getXmlPathPorId(item.id));
    const CustoItem custo = xmlUtil.calcularCustoItemSN(xmlPath, item.nitem);
    if (custo.custoUnitario > 0)
        return custo.custoUnitario;
    return item.preco;
}

void EstoqueNota_service::adicionarItem(const ProdutoNotaDTO &item, const ConfigDTO &cfg,
                                        ResumoEstoqueNota &resumo)
{
    const double custo = custoUnitario(item);
    QString erro;

    if (ehGtinValido(item.codigoBarras)) {
        const QString codigo = item.codigoBarras.trimmed();
        if (prodServ.codigoBarrasExiste(codigo)) {
            const ProdutoDTO existente = prodServ.getProdutoPeloCodBarras(codigo);
            if (!somarAoProduto(existente, item, custo, cfg, erro)) {
                resumo.falhas << item.descricao + ": " + erro;
                return;
            }
            prodNotaServ.marcarComoAdicionado(item.id);
            ++resumo.atualizados;
            return;
        }
        bool semNf = false;
        if (!cadastrarProduto(item, codigo, custo, cfg, semNf, erro)) {
            resumo.falhas << item.descricao + ": " + erro;
            return;
        }
        prodNotaServ.marcarComoAdicionado(item.id);
        ++resumo.cadastrados;
        if (semNf)
            ++resumo.semNf;
        return;
    }

    const ProdutoDTO mesmaDescricao = prodServ.getProdutoPelaDescricao(item.descricao);
    if (mesmaDescricao.id > 0) {
        if (!somarAoProduto(mesmaDescricao, item, custo, cfg, erro)) {
            resumo.falhas << item.descricao + ": " + erro;
            return;
        }
        prodNotaServ.marcarComoAdicionado(item.id);
        ++resumo.atualizados;
        return;
    }

    const QString codigoInterno = gerarCodigoInternoLivre();
    if (codigoInterno.isEmpty()) {
        resumo.falhas << item.descricao + ": não foi possível gerar um código interno.";
        return;
    }
    bool semNf = false;
    if (!cadastrarProduto(item, codigoInterno, custo, cfg, semNf, erro)) {
        resumo.falhas << item.descricao + ": " + erro;
        return;
    }
    prodNotaServ.marcarComoAdicionado(item.id);
    ++resumo.cadastrados;
    ++resumo.comCodigoInterno;
    if (semNf)
        ++resumo.semNf;
}

bool EstoqueNota_service::somarAoProduto(const ProdutoDTO &existente, const ProdutoNotaDTO &item,
                                         double custo, const ConfigDTO &cfg, QString &erro)
{
    QVariantMap campos;
    campos["quantidade"] = Produto_Service::round2(existente.quantidade + item.quantidade);

    if (custo > 0 && !qFuzzyCompare(existente.precoFornecedor, custo)) {
        const double lucro = cfg.porcentLucroFinanceiro;
        campos["preco_fornecedor"] = Produto_Service::round2(custo);
        campos["porcent_lucro"] = lucro;
        campos["preco"] = Produto_Service::round2(prodServ.calcularPrecoFinal(custo, lucro));
    }

    if (existente.descricao.trimmed().isEmpty())
        campos["descricao"] = Produto_Service::normalizeText(item.descricao);
    if (existente.uCom.trimmed().isEmpty() && !item.uCom.trimmed().isEmpty())
        campos["un_comercial"] = item.uCom;

    const QString ncm = ncmValido(existente.ncm) ? existente.ncm : item.ncm;
    if (ncmValido(ncm) && ncm != existente.ncm) {
        campos["ncm"] = ncm;
        campos["aliquota_imposto"] = static_cast<double>(ibpt.get_Aliquota_From_Csv(ncm));
    }

    auto r = prodServ.atualizarCamposMap(existente.id, campos, !existente.nf);
    if (!r.ok) {
        erro = r.msg;
        return false;
    }
    return true;
}

bool EstoqueNota_service::cadastrarProduto(const ProdutoNotaDTO &item, const QString &codigo, double custo,
                                           const ConfigDTO &cfg, bool &semNf, QString &erro)
{
    const bool ncmOk = ncmValido(item.ncm) && ibpt.eh_Valido_NCM(item.ncm);
    semNf = !ncmOk;

    ProdutoDTO novo;
    novo.id = 0;
    novo.quantidade = item.quantidade;
    novo.descricao = Produto_Service::normalizeText(item.descricao);
    novo.codigoBarras = codigo;
    novo.nf = ncmOk;
    novo.uCom = item.uCom;
    novo.precoFornecedor = custo;
    novo.percentLucro = cfg.porcentLucroFinanceiro;
    novo.preco = prodServ.calcularPrecoFinal(custo, cfg.porcentLucroFinanceiro);
    novo.ncm = item.ncm;
    novo.cest = QString();
    novo.aliquotaIcms = ncmOk ? static_cast<double>(ibpt.get_Aliquota_From_Csv(item.ncm)) : 0.0;
    novo.csosn = cfg.csosnPadraoProduto;
    novo.pis = cfg.pisPadraoProduto;
    novo.local = QString();

    auto r = prodServ.inserir(novo);
    if (!r.ok) {
        erro = r.msg;
        return false;
    }
    return true;
}

QString EstoqueNota_service::gerarCodigoInternoLivre()
{
    for (int tentativa = 0; tentativa < 50; ++tentativa) {
        const QString codigo = CodigoBarrasUtil::gerarNumeroCodigoBarrasNaoFiscal();
        if (!prodServ.codigoBarrasExiste(codigo))
            return codigo;
    }
    return QString();
}
