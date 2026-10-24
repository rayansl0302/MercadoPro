#ifndef PRODUTONOTA_DTO_H
#define PRODUTONOTA_DTO_H

#include <qstring.h>

struct ProdutoNotaDTO {
    qlonglong id = 0;
    double quantidade;
    QString descricao;
    double preco = 0;
    QString codigoBarras;
    QString uCom;
    QString cfop;
    int nitem = 0;
    qlonglong idNf = 0;
    QString status;
    QString cstIcms;
    bool temSt = false;
    qlonglong idNfDevol;
    bool adicionado = false;
    QString ncm;
    double aliquotaIcms = 0;
    QString csosn;
    QString pis;
    QString adicionadoEm;
    QString atualizadoEm;
};

#endif // PRODUTONOTA_DTO_H
