#ifndef PAGAMENTOMISTO_H
#define PAGAMENTOMISTO_H

#include <QList>
#include <QString>
#include <QtGlobal>
#include <sstream>
#include <string>
#include "../dto/Vendas_dto.h"

// Venda paga em mais de uma forma: uma seção [pagNNN] por forma no INI do ACBr (NFC-e e NF-e).
namespace PagamentoMistoNf {

struct Parte {
    QString tPag;    // código da forma na nota: 01 dinheiro, 03 crédito, 04 débito, 17 Pix
    double vPag = 0;
};

inline QString codigoTPag(const QString &forma)
{
    if (forma == "Crédito") return "03";
    if (forma == "Débito") return "04";
    if (forma == "Pix") return "17";
    return "01";   // Dinheiro
}

// em Dinheiro vale o que o cliente entregou (o troco sai em vTroco); nas outras formas, o valor da parte
inline QList<Parte> partes(const QList<PagamentoVendaDTO> &pagamentos)
{
    QList<Parte> lista;
    for (const PagamentoVendaDTO &p : pagamentos)
        lista.append({codigoTPag(p.forma), p.forma == "Dinheiro" && p.valorRecebido > 0 ? p.valorRecebido : p.valor});
    return lista;
}

inline std::string virgula(double v)
{
    return QString::number(v, 'f', 2).replace('.', ',').toStdString();
}

// vNf/apenasNf: quando só parte dos itens vai para a nota, as formas são reduzidas na mesma proporção (sem troco)
inline void escrever(std::stringstream &ini, QList<Parte> lista, double troco, bool apenasNf, double vNf)
{
    if (apenasNf && !lista.isEmpty()) {
        double soma = 0;
        for (const Parte &p : lista)
            soma += p.vPag;
        double acumulado = 0;
        for (int i = 0; i < lista.size(); ++i) {
            if (i == lista.size() - 1)
                lista[i].vPag = qRound64((vNf - acumulado) * 100.0) / 100.0;
            else
                lista[i].vPag = soma > 0 ? qRound64(lista[i].vPag / soma * vNf * 100.0) / 100.0 : 0;
            acumulado += lista[i].vPag;
        }
        troco = 0;
    }
    for (int i = 0; i < lista.size(); ++i) {
        const Parte &p = lista.at(i);
        const bool ultima = (i == lista.size() - 1);
        ini << "[pag" << QString("%1").arg(i + 1, 3, 10, QChar('0')).toStdString() << "]\n";
        ini << "indPag=0\n";
        ini << "tPag=" << p.tPag.toStdString() << "\n";
        ini << "xPag=\n";
        ini << "vPag=" << virgula(p.vPag) << "\n";
        ini << "dPag=\n";
        ini << "CNPJPag=\n";
        ini << "UFPag=\n";
        if (p.tPag == "03" || p.tPag == "04" || p.tPag == "17") {
            ini << "tpIntegra=2\n";
            ini << "tBand=99\n";
            ini << "cAut=000000\n";
        } else {
            ini << "tpIntegra=\n";
            ini << "tBand=\n";
            ini << "cAut=\n";
        }
        ini << "CNPJ=\n";
        ini << "CNPJReceb=\n";
        ini << "idTermPag=\n";
        // o troco é um só na nota: vai na última seção (o ACBr guarda o último valor lido)
        ini << "vTroco=" << virgula(ultima ? troco : 0.0) << "\n\n";
    }
}

} // namespace PagamentoMistoNf

#endif // PAGAMENTOMISTO_H
