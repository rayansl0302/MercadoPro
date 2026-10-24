#ifndef LERCODIGOSNOTADIALOG_H
#define LERCODIGOSNOTADIALOG_H

#include <QDialog>
#include <QList>

#include "dto/ProdutoNota_dto.h"
#include "services/produtonota_service.h"
#include "services/produto_service.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

// Assistente para ler, com o leitor, o código de barras da embalagem de cada item de uma nota de entrada
// (as notas costumam vir com "SEM GTIN"). Mostra um item por vez; ao ler, grava e pula para o próximo item sem código.
class LerCodigosNotaDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LerCodigosNotaDialog(qlonglong idNf, QWidget *parent = nullptr);
    int codigosGravados() const { return gravados; }

private:
    qlonglong idNf;
    QList<ProdutoNotaDTO> itens;   // itens ainda não lançados no estoque
    int atual = -1;
    int gravados = 0;
    ProdutoNota_service prodNotaServ;
    Produto_Service prodServ;

    QLabel *lblProgresso;
    QLabel *lblDescricao;
    QLabel *lblDetalhe;
    QLabel *lblAviso;
    QLineEdit *edCodigo;
    QTableWidget *tabela;
    QPushButton *btnAnterior;
    QPushButton *btnPular;
    QPushButton *btnLimpar;

    void carregar();
    void irPara(int indice);
    void salvarEAvancar();
    void preencherLinha(int i);
    void atualizarProgresso();
    int proximoSemCodigo(int aPartirDe) const;
    bool confirmar(const QString &titulo, const QString &texto);
};

#endif // LERCODIGOSNOTADIALOG_H
