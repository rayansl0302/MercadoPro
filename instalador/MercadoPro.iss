; Instalador do MercadoPro (Inno Setup 6). O workflow "Compilar para Windows" monta a pasta "dist" e chama:
;   ISCC.exe /DVersaoApp=1.0.7 instalador\MercadoPro.iss
; O programa continua se chamando QEstoqueLoja.exe por dentro: assim a pasta de dados e o config.ini
; das lojas que já usam o sistema são reaproveitados (nada é apagado nem movido).

#ifndef VersaoApp
  #define VersaoApp "1.0.0"
#endif

[Setup]
; Não troque o AppId: é ele que faz uma versão nova atualizar a anterior em vez de instalar em paralelo.
AppId={{6C2B7E5A-3F4D-4B8A-9E21-5D7A0C9F1B34}
AppName=MercadoPro
AppVersion={#VersaoApp}
AppPublisher=MercadoPro
DefaultDirName={autopf}\MercadoPro
DefaultGroupName=MercadoPro
DisableProgramGroupPage=yes
OutputDir=..\saida-instalador
OutputBaseFilename=MercadoPro-Setup-{#VersaoApp}
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
UninstallDisplayIcon={app}\QEstoqueLoja.exe
; programa aberto: o instalador pede para fechar e atualiza os arquivos
CloseApplications=yes
RestartApplications=no
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[Tasks]
Name: "desktopicon"; Description: "Criar atalho na Área de Trabalho"; GroupDescription: "Atalhos:"
Name: "pdvicon"; Description: "Criar atalho que abre direto na tela de venda (PDV)"; GroupDescription: "Atalhos:"

[Files]
; Só o que o workflow montou. As DLLs da ACBrLib NÃO vêm aqui (licença/tamanho): numa atualização sobre a pasta
; já instalada elas continuam lá, porque o instalador não apaga o que não faz parte do pacote.
Source: "..\dist\*"; DestDir: "{app}"; Excludes: "LEIA-ME.txt"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\MercadoPro"; Filename: "{app}\QEstoqueLoja.exe"
Name: "{group}\Desinstalar o MercadoPro"; Filename: "{uninstallexe}"
Name: "{autodesktop}\MercadoPro"; Filename: "{app}\QEstoqueLoja.exe"; Tasks: desktopicon
Name: "{autodesktop}\MercadoPro PDV"; Filename: "{app}\QEstoqueLoja.exe"; Parameters: "--pdv"; Tasks: pdvicon

[Run]
Filename: "{app}\QEstoqueLoja.exe"; Description: "Abrir o MercadoPro agora"; Flags: nowait postinstall skipifsilent

[Code]
// Sem as DLLs da ACBrLib o programa fecha ao abrir. Avisa na primeira instalação (em atualização elas já estão na pasta).
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) and (not WizardSilent) then
    if not FileExists(ExpandConstant('{app}\ACBrNFe64.dll')) then
      MsgBox('Atenção: faltam as DLLs da ACBrLib (ACBrNFe64.dll, ACBrMail64.dll e ACBrConsultaCNPJ64.dll) na pasta:' + #13#10 +
             ExpandConstant('{app}') + #13#10#13#10 +
             'Copie-as da instalação anterior para essa pasta antes de abrir o programa. ' +
             'Para atualizar sem esse passo, instale na mesma pasta onde o programa já está.', mbInformation, MB_OK);
end;
