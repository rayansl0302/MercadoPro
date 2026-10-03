#ifndef NUVEMCONTRATO_H
#define NUVEMCONTRATO_H

#include <QList>
#include <QString>
#include <QStringList>

struct ColunaContrato {
    QString nome;
    QString tipo;     // int, dec, real, text, bool, ts
};

struct TabelaContrato {
    QString nome;
    QStringList pk;
    QList<ColunaContrato> colunas;
};

// Contrato entre a loja e a API: lista de permissão das tabelas e colunas que podem subir para a nuvem.
// O arquivo (src/nuvem/contrato.json) é a fonte única; a API Node carrega uma cópia idêntica e um teste
// confere que as duas são iguais.
class NuvemContrato
{
public:
    static const NuvemContrato &instancia();

    bool valido() const { return !tabelasLista.isEmpty(); }
    int versao() const { return versaoContrato; }
    const QList<TabelaContrato> &tabelas() const { return tabelasLista; }
    const TabelaContrato *tabela(const QString &nome) const;
    // conteúdo bruto do JSON (a API compara o hash)
    const QByteArray &bruto() const { return jsonBruto; }

private:
    NuvemContrato();
    int versaoContrato = 0;
    QList<TabelaContrato> tabelasLista;
    QByteArray jsonBruto;
};

#endif // NUVEMCONTRATO_H
