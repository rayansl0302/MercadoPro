#include "contasreceber_service.h"
#include "../repository/contasreceber_repository.h"
#include <QSet>
#include "cliente_service.h"
#include <QRegularExpression>

Cliente_service::Cliente_service(QObject *parent)
    : QObject{parent}
{}

qlonglong Cliente_service::contarQuantosRegistrosPorCPFCNPJ(const QString &cpfcnpj){
    return cliRepo.contarQuantosRegistrosPorCPFCNPJ(cpfcnpj);
}

Cliente_service::Resultado Cliente_service::inserirClienteEmitente(ClienteDTO emissor){

    if(emissor.cpf.length() == 14){
        emissor.ehPf =false;
    }else{
        emissor.ehPf = true;
    }
    if(!emissor.ie.isEmpty()){
        emissor.indIeDest = 1;
    }else{
        emissor.indIeDest = 0;
    }

    QString detalhe;
    if(!cliRepo.inserir(emissor, &detalhe)){
        const QString msg = detalhe.isEmpty()
            ? QString("Não conseguiu inserir cliente emissor.")
            : QString("Não conseguiu inserir cliente emissor: %1").arg(detalhe);
        return {false, ClienteErro::InsercaoInvalida, msg};
    }else{
        return {true, ClienteErro::Nenhum, ""};
    }
}

qlonglong Cliente_service::getIdFromCpfCnpj(const QString &cpfcnpj){
    return cliRepo.getIdFromCPFCNPJ(cpfcnpj);
}


void Cliente_service::listarClientes(QSqlQueryModel *model){
    return cliRepo.listarClientes(model);
}

namespace {
// nome para comparar duplicidade: sem diferenciar maiúsculas nem espaços sobrando
QString nomeNormalizado(const QString &nome){ return nome.simplified().toLower(); }
}

Cliente_service::Resultado Cliente_service::deletarCliente(qlonglong id){
    if(id > 1){
        // cliente com dívida aberta não se exclui (só se inativa); com histórico também não, para não
        // deixar vendas e lançamentos sem dono
        ContasReceber_service receber;
        const double devido = receber.totalDevido(id);
        if(devido > 0.004){
            return {false, ClienteErro::QuebraDeRegra,
                    QString("Este cliente tem dívida em aberto (R$ %1). Cliente com dívida não pode ser excluído: "
                            "inative-o em Contas a receber.").arg(QString::number(devido, 'f', 2))};
        }
        ContasReceber_repository histRepo;
        if(histRepo.quantidadeDeHistorico(id) > 0){
            return {false, ClienteErro::QuebraDeRegra,
                    "Este cliente tem histórico de compras. Para não perder o histórico, inative-o em Contas a "
                    "receber em vez de excluir."};
        }
        if(!cliRepo.deletarCliente(id)){
            return {false, ClienteErro::DeleteFalhou, "Ocorreu um erro ao deletar cliente"};
        }else{
            return {true, ClienteErro::Nenhum, ""};
        }
    }else{
        return {false, ClienteErro::QuebraDeRegra, "Não é possivel deletar o cliente Consumidor."};
    }

}

void Cliente_service::pesquisar(QSqlQueryModel* model, const QString &nome){
    return cliRepo.pesquisar(model, nome);
}


Cliente_service::Resultado Cliente_service::inserirCliente(ClienteDTO cliente)
{
    Resultado r;

    // 1️ Nome obrigatório
    if (cliente.nome.trimmed().isEmpty()) {
        r.ok = false;
        r.erro = ClienteErro::CampoVazio;
        r.msg = "Nome é obrigatório.";
        return r;
    }
    // nome único, sem diferenciar maiúsculas nem espaços a mais
    for (const ClienteDTO &outro : cliRepo.getListAllClientes()) {
        if (nomeNormalizado(outro.nome) == nomeNormalizado(cliente.nome)) {
            r.ok = false;
            r.erro = ClienteErro::InsercaoInvalida;
            r.msg = "Já existe um cliente com este nome.";
            return r;
        }
    }

    // 2️ CPF/CNPJ (opcional)
    if (!cliente.cpf.trimmed().isEmpty()) {

        for (const QChar &c : cliente.cpf) {
            if (!c.isDigit()) {
                r.ok = false;
                r.erro = ClienteErro::QuebraDeRegra;
                r.msg = "CPF/CNPJ deve conter apenas números.";
                return r;
            }
        }

        if (cliente.ehPf) {
            if (cliente.cpf.length() != 11) {
                r.ok = false;
                r.erro = ClienteErro::QuebraDeRegra;
                r.msg = "CPF deve conter 11 dígitos.";
                return r;
            }
        } else {
            if (cliente.cpf.length() != 14) {
                r.ok = false;
                r.erro = ClienteErro::QuebraDeRegra;
                r.msg = "CNPJ deve conter 14 dígitos.";
                return r;
            }
        }

        // Duplicidade só verifica se CPF foi informado
        if (cliRepo.contarQuantosRegistrosPorCPFCNPJ(cliente.cpf) > 0) {
            r.ok = false;
            r.erro = ClienteErro::InsercaoInvalida;
            r.msg = "Já existe cliente com este CPF/CNPJ.";
            return r;
        }
    }

    // 3️ Email opcional
    if (!cliente.email.trimmed().isEmpty()) {
        QRegularExpression emailRegex(R"((\w+)(\.\w+)*@(\w+)((\.\w+)+))");
        if (!emailRegex.match(cliente.email).hasMatch()) {
            r.ok = false;
            r.erro = ClienteErro::QuebraDeRegra;
            r.msg = "Email inválido.";
            return r;
        }
    }

    // 4️ Telefone opcional
    if (!cliente.telefone.trimmed().isEmpty()) {
        if (cliente.telefone.length() < 10) {
            r.ok = false;
            r.erro = ClienteErro::QuebraDeRegra;
            r.msg = "Telefone inválido.";
            return r;
        }

        for (const QChar &c : cliente.telefone) {
            if (!c.isDigit()) {
                r.ok = false;
                r.erro = ClienteErro::QuebraDeRegra;
                r.msg = "Telefone deve conter apenas números.";
                return r;
            }
        }
    }

    // if(!cliente.ie.isEmpty()){
    //     if (cliente.indIeDest == 0) {
    //         r.ok = false;
    //         r.erro = ClienteErro::QuebraDeRegra;
    //         r.msg = "Não contribuinte com Inscrição Estadual.";
    //         return
    // }

    if (!cliRepo.inserir(cliente)) {
        r.ok = false;
        r.erro = ClienteErro::Banco;
        r.msg = "Erro ao inserir cliente no banco.";
        return r;
    }

    r.ok = true;
    r.msg = "Cliente inserido com sucesso.";
    return r;
}


ClienteDTO Cliente_service::getClienteByID(qlonglong id){
    return cliRepo.getClienteByID(id);
}

Cliente_service::Resultado Cliente_service::updateCliente(qlonglong id, ClienteDTO cliente){
    Resultado r;

    // 1️ Nome obrigatório
    if (cliente.nome.trimmed().isEmpty()) {
        r.ok = false;
        r.erro = ClienteErro::CampoVazio;
        r.msg = "Nome é obrigatório.";
        return r;
    }
    for (const ClienteDTO &outro : cliRepo.getListAllClientes()) {
        if (outro.id != id && nomeNormalizado(outro.nome) == nomeNormalizado(cliente.nome)) {
            r.ok = false;
            r.erro = ClienteErro::InsercaoInvalida;
            r.msg = "Já existe outro cliente com este nome.";
            return r;
        }
    }

    // 2️ CPF/CNPJ (opcional)
    if (!cliente.cpf.trimmed().isEmpty()) {

        for (const QChar &c : cliente.cpf) {
            if (!c.isDigit()) {
                r.ok = false;
                r.erro = ClienteErro::QuebraDeRegra;
                r.msg = "CPF/CNPJ deve conter apenas números.";
                return r;
            }
        }

        if (cliente.ehPf) {
            if (cliente.cpf.length() != 11) {
                r.ok = false;
                r.erro = ClienteErro::QuebraDeRegra;
                r.msg = "CPF deve conter 11 dígitos.";
                return r;
            }
        } else {
            if (cliente.cpf.length() != 14) {
                r.ok = false;
                r.erro = ClienteErro::QuebraDeRegra;
                r.msg = "CNPJ deve conter 14 dígitos.";
                return r;
            }
        }

    }

    // 3️ Email opcional
    if (!cliente.email.trimmed().isEmpty()) {
        QRegularExpression emailRegex(R"((\w+)(\.\w+)*@(\w+)((\.\w+)+))");
        if (!emailRegex.match(cliente.email).hasMatch()) {
            r.ok = false;
            r.erro = ClienteErro::QuebraDeRegra;
            r.msg = "Email inválido.";
            return r;
        }
    }

    // 4️ Telefone opcional
    if (!cliente.telefone.trimmed().isEmpty()) {
        if (cliente.telefone.length() < 10) {
            r.ok = false;
            r.erro = ClienteErro::QuebraDeRegra;
            r.msg = "Telefone inválido.";
            return r;
        }

        for (const QChar &c : cliente.telefone) {
            if (!c.isDigit()) {
                r.ok = false;
                r.erro = ClienteErro::QuebraDeRegra;
                r.msg = "Telefone deve conter apenas números.";
                return r;
            }
        }
    }

    // if(!cliente.ie.isEmpty()){
    //     if (cliente.indIeDest == 0) {
    //         r.ok = false;
    //         r.erro = ClienteErro::QuebraDeRegra;
    //         r.msg = "Não contribuinte com Inscrição Estadual.";
    //         return
    // }

    if (!cliRepo.updateCliente(id, cliente)) {
        r.ok = false;
        r.erro = ClienteErro::Banco;
        r.msg = "Erro ao atualizar cliente no banco.";
        return r;
    }else{
        return {true, ClienteErro::Nenhum, ""};
    }
}

QStringList Cliente_service::listarClientesParaCompleter()
{
    QStringList lista;

    QList<ClienteDTO> clientes = cliRepo.getListAllClientes();

    // cliente inativo não aparece para venda (o Consumidor, id 1, sempre aparece)
    QSet<qlonglong> ativos;
    ContasReceber_repository creditoRepo;
    for(const ClienteCreditoDTO &c : creditoRepo.listarClientes(false))
        ativos.insert(c.id);

    for(const ClienteDTO &cli : clientes){
        if(cli.id > 1 && !ativos.contains(cli.id))
            continue;
        lista << QString("%1 (ID: %2)").arg(cli.nome).arg(cli.id);
    }

    return lista;
}

QPair<QString, int> Cliente_service::extrairNomeId(const QString &texto) {
    QRegularExpression regex("^(.*?)\\s*\\(ID:\\s*(\\d+)\\)$");
    QRegularExpressionMatch match = regex.match(texto);

    if (match.hasMatch()) {
        return qMakePair(match.captured(1).trimmed(), match.captured(2).toInt());
    }
    return qMakePair(QString(), -1); // Retorno inválido
}

Cliente_service::ResultadoValidacaoClienteCompleter
Cliente_service::validarClienteTexto(const QString &texto)
{
    ResultadoValidacaoClienteCompleter res;

    QString txt = texto.trimmed();

    if(txt.isEmpty()){
        res.erro = ClienteErro::CampoVazio;
        res.msg = "Cliente não informado.";
        return res;
    }

    auto [nome,id] = extrairNomeId(txt);

    if(id == -1)
    {
        // CPF/CNPJ digitado no lugar do nome (com ou sem pontuação)
        {
            QString digitos;
            for (const QChar &ch : txt)
                if (ch.isDigit())
                    digitos += ch;
            if ((digitos.size() == 11 || digitos.size() == 14) && digitos.size() >= txt.size() - 4) {
                QStringList formas{digitos};
                if (digitos.size() == 11)
                    formas << QString("%1.%2.%3-%4").arg(digitos.mid(0, 3), digitos.mid(3, 3), digitos.mid(6, 3), digitos.mid(9, 2));
                else
                    formas << QString("%1.%2.%3/%4-%5").arg(digitos.mid(0, 2), digitos.mid(2, 3), digitos.mid(5, 3), digitos.mid(8, 4), digitos.mid(12, 2));
                for (const QString &f : formas) {
                    const qlonglong idCpf = cliRepo.getIdFromCPFCNPJ(f);
                    if (idCpf > 0) {
                        res.ok = true;
                        res.clienteId = idCpf;
                        res.nomeCorrigido = QString("%1 (ID: %2)").arg(cliRepo.getClienteByID(idCpf).nome).arg(idCpf);
                        return res;
                    }
                }
            }
        }
        ClienteDTO cli = cliRepo.buscarClientePorNomeAproximado(txt);

        if(cli.id == 0){
            res.erro = ClienteErro::InsercaoInvalida;
            res.msg = "Cliente não encontrado.";
            return res;
        }

        res.ok = true;
        res.clienteId = cli.id;
        res.nomeCorrigido = QString("%1 (ID: %2)").arg(cli.nome).arg(cli.id);
        return res;
    }

    if(!cliRepo.verificarNomeId(nome,id)){
        res.erro = ClienteErro::QuebraDeRegra;
        res.msg = "Nome não corresponde ao ID.";
        return res;
    }

    res.ok = true;
    res.clienteId = id;

    return res;
}
