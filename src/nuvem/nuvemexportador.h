#ifndef NUVEMEXPORTADOR_H
#define NUVEMEXPORTADOR_H

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>
#include "nuvemcontrato.h"

// Uma linha exportada: todos os valores viram texto (ou nulo), na ordem das colunas do contrato.
using LinhaExportada = QList<QVariant>;

struct TabelaExportada {
    QString nome;
    QStringList colunas;
    QList<LinhaExportada> linhas;
    QString checksum;                 // sha256 do conteúdo (ver NuvemExportador::checksum)
};

struct Exportacao {
    bool ok = false;
    QString erro;
    QString geradoEm;                 // ISO com fuso da loja
    QList<TabelaExportada> tabelas;
    qint64 totalLinhas() const;
};

// Lê TODAS as tabelas do contrato de uma vez, dentro de uma única transação de leitura (foto consistente:
// uma venda gravada no meio da leitura não aparece pela metade). Só as colunas do contrato saem.
class NuvemExportador
{
public:
    Exportacao exportarTudo();

    // sha256 (hex minúsculo) de: valores separados por 0x1F, linhas terminadas em 0x1E, nulo = "\N".
    // A API recalcula do mesmo jeito para conferir que nada se perdeu no caminho.
    static QString checksum(const QList<LinhaExportada> &linhas);

    // converte o valor do banco para o texto do contrato (decimais com 2 casas, booleano 0/1, data ISO...)
    static QVariant normalizar(const QVariant &valor, const QString &tipo);
};

#endif // NUVEMEXPORTADOR_H
