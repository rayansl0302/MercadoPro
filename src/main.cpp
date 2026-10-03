    #include "mainwindow.h"
    #include "util/tema.h"
    #include "loginoperador.h"
    #include "operadores.h"

    #include <QApplication>
    #include <QLocale>
    #include <QMessageBox>
    #include <QTranslator>
    #include <qicon.h>
    #include <QStyleFactory>
    #include <QGuiApplication>
    #include <QTimer>
    #include <QMetaObject>
    #include <QLockFile>
    #include <QDir>
    #include <QDebug>

    #include "services/operador_service.h"
    #include "services/sessao_service.h"
    #include "services/contaspagar_service.h"
    #include "services/contasreceber_service.h"
    #include "nuvem/nuvem_service.h"

    namespace {

    // Entrada sem PIN, só para gerar telas em desenvolvimento.
    // Ignorada em Release mesmo com a variável de ambiente presente.
    bool autologinDesenvolvimento()
    {
    #if defined(QEL_MODO_DESENVOLVIMENTO)
        return qEnvironmentVariableIntValue("QESTOQUELOJA_DEV_AUTOLOGIN") == 1;
    #else
        return false;
    #endif
    }

    SessaoDTO sessaoDesenvolvimento()
    {
        SessaoDTO sessao;
        const QList<OperadorDTO> ativos = Operador_service().listar(true);
        if (!ativos.isEmpty()) {
            sessao.idOperador = ativos.first().id;
            sessao.nomeOperador = ativos.first().nome;
            sessao.gerente = ativos.first().gerente;
        } else {
            sessao.idOperador = kOperadorGerenteId;
            sessao.nomeOperador = QString::fromLatin1(kOperadorGerenteNome);
            sessao.gerente = true;
            sessao.pinGeral = true;
        }
        return sessao;
    }

    } // namespace

    int main(int argc, char *argv[])
    {
        QGuiApplication::setDesktopFileName("qestoqueloja");


        QApplication a(argc, argv);

        // Força estilo independente do sistema
        QApplication::setStyle(QStyleFactory::create("Fusion"));

        // Configura paleta clara manualmente
        QPalette lightPalette;

        lightPalette.setColor(QPalette::Window, QColor(255, 255, 255));
        lightPalette.setColor(QPalette::WindowText, Qt::black);
        lightPalette.setColor(QPalette::Base, QColor(255, 255, 255));
        lightPalette.setColor(QPalette::AlternateBase, QColor(220, 234, 245));
        lightPalette.setColor(QPalette::ToolTipBase, Qt::black);
        lightPalette.setColor(QPalette::ToolTipText, Qt::white);
        lightPalette.setColor(QPalette::Text, Qt::black);
        lightPalette.setColor(QPalette::Button, QColor(240, 240, 240));
        lightPalette.setColor(QPalette::ButtonText, Qt::black);
        lightPalette.setColor(QPalette::BrightText, Qt::red);
        lightPalette.setColor(QPalette::Link, QColor(0, 0, 255));

        lightPalette.setColor(QPalette::Highlight, QColor(0, 120, 215));
        lightPalette.setColor(QPalette::HighlightedText, Qt::white);

        a.setPalette(lightPalette);
        Tema::aplicar(a);

        QTranslator translator;
        const QStringList uiLanguages = QLocale::system().uiLanguages();
        for (const QString &locale : uiLanguages) {
            const QString baseName = "QEstoqueLoja_" + QLocale(locale).name();
            if (translator.load(":/i18n/" + baseName)) {
                a.installTranslator(&translator);
                break;
            }
        }

        // Uma instância por computador: duas janelas dividiriam o mesmo terminal, o mesmo caixa e
        // a mesma sessão de log (uma fecharia a sessão da outra como "sem logout").
        QLockFile travaInstancia(QDir::temp().absoluteFilePath("QEstoqueLoja.instancia.lock"));
        travaInstancia.setStaleLockTime(0);   // só considera velha se o processo não existe mais
        if (!travaInstancia.tryLock(200)) {
            QMessageBox::warning(nullptr, "MercadoPro",
                "O MercadoPro já está aberto neste computador.");
            return 0;
        }

        // Regra de administração também no serviço: cadastrar, desbloquear, redefinir PIN e marcar
        // gerente só passam com gerente logado (ou elevação por PIN do gerente), mesmo se uma tela
        // nova esquecer de conferir.
        Operador_service::definirAutorizador([]() {
            return Sessao_service::instancia()->autorizadoParaAdministrar();
        });

        // A MainWindow é construída antes do login porque é ela que aponta a conexão do banco
        // e roda a migração; a auditoria da sessão precisa das duas coisas prontas.
        // Estornar pagamento e cancelar conta a pagar: só gerente (ou elevação), também no serviço.
        ContasPagar_service::definirAutorizador([]() {
            return Sessao_service::instancia()->autorizadoParaAdministrar();
        });
        // Contas a receber: cancelar lançamento, estornar pagamento, inativar cliente, mexer no limite e
        // liberar compra acima do limite só passam com gerente (ou elevação por PIN).
        ContasReceber_service::definirAutorizador([]() {
            return Sessao_service::instancia()->autorizadoParaAdministrar();
        });

        MainWindow w;

        // Nenhuma tela de venda abre antes de existir uma sessão aceita.
        const bool modoDesenvolvimento = autologinDesenvolvimento();
        SessaoDTO sessaoDTO;
        if (modoDesenvolvimento) {
            sessaoDTO = sessaoDesenvolvimento();
            qWarning().noquote() << "\n=== MODO DESENVOLVIMENTO: login automatico como"
                                 << sessaoDTO.nomeOperador << "===\n";
        } else {
            bool cancelou = false;
            sessaoDTO = LoginOperador::executar(&cancelou, &w);
            if (cancelou || sessaoDTO.nomeOperador.isEmpty())
                return 0;
        }

        QString erroSessao;
        // o DTO só é aceito sem conferência no login de desenvolvimento (build Debug)
        Sessao_service::instancia()->abrir(sessaoDTO, &erroSessao, modoDesenvolvimento);
        if (!erroSessao.isEmpty()) {
            QMessageBox::critical(&w, "Sessão do operador", erroSessao);
            return 1;
        }

        w.setModoDesenvolvimento(modoDesenvolvimento);
        w.aplicarSessao();
        w.show();
        // sincronização com o painel online (desligada por padrão; só age se configurada neste computador)
        Nuvem_service::instancia()->iniciar();
        // várias empresas (CNPJs): pergunta com qual vai trabalhar antes de vender
        if (!QCoreApplication::arguments().contains("--preview-caixa"))
            w.perguntarEmpresaSeNecessario();
        // atalho da Área de Trabalho: QEstoqueLoja --pdv abre direto a tela de venda
        const QStringList args = QCoreApplication::arguments();
        if (args.contains("--pdv"))
            w.abrirPdv();

        const int prev = args.indexOf("--preview-caixa");
        if (prev >= 0 && prev + 1 < args.size()) {
            const QString tela = args.at(prev + 1);
            QTimer::singleShot(800, &w, [tela, &w]() {
                const char *slot = nullptr;
                if (tela == "operadores") slot = "operadoresClicked";
                else if (tela == "abrir") slot = "abrirCaixaClicked";
                else if (tela == "fechar") slot = "fecharCaixaClicked";
                else if (tela == "historico") slot = "historicoCaixaClicked";
                else if (tela == "sangria") slot = "sangriaClicked";
                else if (tela == "contas") slot = "abrirContasPagar";
                else if (tela == "receber") slot = "abrirContasReceber";
                else if (tela == "nuvem") slot = "nuvemClicked";
                else if (tela == "produto") slot = "on_Btn_AddProd_clicked";
                else if (tela == "clientes") slot = "on_Btn_Clientes_clicked";
                else if (tela == "vendas") slot = "on_Btn_Venda_clicked";
                else if (tela == "orcamento") slot = "on_Btn_Orcamento_clicked";
                else if (tela == "entradas") slot = "on_Btn_Entradas_clicked";
                else if (tela == "relatorios") slot = "on_Btn_Relatorios_clicked";
                else if (tela == "config") slot = "on_actionConfig_triggered";
                else if (tela == "monitor") slot = "on_actionMonitor_Fiscal_triggered";
                else if (tela == "empresa") slot = "escolherEmpresaClicked";
                if (slot)
                    QMetaObject::invokeMethod(&w, slot);
            });
        }
        return a.exec();
    }
