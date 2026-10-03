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
DefaultDirName={code:PastaPadrao}
DefaultGroupName=MercadoPro
DisableProgramGroupPage=yes
OutputDir=..\saida-instalador
OutputBaseFilename=MercadoPro-Setup-{#VersaoApp}
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
WizardImageFile=wizard-lateral.png,wizard-lateral-2x.png
WizardSmallImageFile=wizard-icone.png,wizard-icone-2x.png
UninstallDisplayIcon={app}\QEstoqueLoja.exe
SetupIconFile=..\Imagens\QEstoqueLOja\ICONEQe48ico.ico
; programa aberto: o instalador pede para fechar e atualiza os arquivos
CloseApplications=yes
RestartApplications=no
PrivilegesRequired=admin

[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[Tasks]
Name: "desktopicon"; Description: "Criar atalho na Área de Trabalho"; GroupDescription: "Atalhos:"
Name: "pdvicon"; Description: "Criar atalho que abre direto na tela de venda (PDV)"; GroupDescription: "Atalhos:"

[Files]
; Só o que o workflow montou, mais as DLLs da ACBrLib (vêm de instalador\acbr).
Source: "..\dist\*"; DestDir: "{app}"; Excludes: "LEIA-ME.txt"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "acbr\ACBrNFe64.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "acbr\ACBrMail64.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "acbr\ACBrConsultaCNPJ64.dll"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\MercadoPro"; Filename: "{app}\QEstoqueLoja.exe"
Name: "{group}\Desinstalar o MercadoPro"; Filename: "{uninstallexe}"
Name: "{autodesktop}\MercadoPro"; Filename: "{app}\QEstoqueLoja.exe"; Tasks: desktopicon
Name: "{autodesktop}\MercadoPro PDV"; Filename: "{app}\QEstoqueLoja.exe"; Parameters: "--pdv"; Tasks: pdvicon

[Run]
Filename: "{app}\QEstoqueLoja.exe"; Description: "Abrir o MercadoPro agora"; Flags: nowait postinstall skipifsilent

[Code]
var
  PastaExistente: String;   // pasta de uma instalação anterior (QEstoqueLoja ou MercadoPro); vazio = instalação nova
  VersaoExistente: String;

function TemPrograma(const Pasta: String): Boolean;
begin
  Result := (Pasta <> '') and FileExists(AddBackslash(Pasta) + 'QEstoqueLoja.exe');
end;

// Descobre a pasta a partir de uma entrada da lista "Programas e Recursos" do Windows.
function PastaDaEntrada(const Raiz: Integer; const Chave: String): String;
var
  V: String;
begin
  Result := '';
  if RegQueryStringValue(Raiz, Chave, 'Inno Setup: App Path', V) and TemPrograma(V) then Result := V
  else if RegQueryStringValue(Raiz, Chave, 'InstallLocation', V) and TemPrograma(V) then Result := V
  else if RegQueryStringValue(Raiz, Chave, 'DisplayIcon', V) and TemPrograma(ExtractFilePath(RemoveQuotes(V))) then Result := ExtractFilePath(RemoveQuotes(V))
  else if RegQueryStringValue(Raiz, Chave, 'UninstallString', V) and TemPrograma(ExtractFilePath(RemoveQuotes(V))) then Result := ExtractFilePath(RemoveQuotes(V));
end;

procedure ProcurarNoRegistro(const Raiz: Integer);
var
  Base, Chave, Nome: String;
  Subs: TArrayOfString;
  I: Integer;
  Pasta: String;
begin
  Base := 'Software\Microsoft\Windows\CurrentVersion\Uninstall';
  if (PastaExistente <> '') or (not RegGetSubkeyNames(Raiz, Base, Subs)) then Exit;
  for I := 0 to GetArrayLength(Subs) - 1 do
  begin
    Chave := Base + '\' + Subs[I];
    if RegQueryStringValue(Raiz, Chave, 'DisplayName', Nome) then
      if (Pos('qestoqueloja', LowerCase(Nome)) > 0) or (Pos('mercadopro', LowerCase(Nome)) > 0) then
      begin
        Pasta := PastaDaEntrada(Raiz, Chave);
        if Pasta <> '' then
        begin
          PastaExistente := RemoveBackslashUnlessRoot(Pasta);
          RegQueryStringValue(Raiz, Chave, 'DisplayVersion', VersaoExistente);
          Exit;
        end;
      end;
  end;
end;

procedure ProcurarExistente;
var
  Candidatas: array of String;
  I: Integer;
begin
  if PastaExistente <> '' then Exit;
  ProcurarNoRegistro(HKLM64);
  ProcurarNoRegistro(HKLM32);
  ProcurarNoRegistro(HKCU);
  if PastaExistente <> '' then Exit;
  // instalações antigas copiadas à mão (sem entrada no Windows): lugares mais comuns
  SetArrayLength(Candidatas, 7);
  Candidatas[0] := ExpandConstant('{autopf}\MercadoPro');
  Candidatas[1] := ExpandConstant('{autopf}\QEstoqueLoja');
  Candidatas[2] := ExpandConstant('{autopf32}\QEstoqueLoja');
  Candidatas[3] := ExpandConstant('{localappdata}\Programs\QEstoqueLoja');
  Candidatas[4] := ExpandConstant('{localappdata}\Programs\MercadoPro');
  Candidatas[5] := 'C:\QEstoqueLoja';
  Candidatas[6] := 'C:\MercadoPro';
  for I := 0 to GetArrayLength(Candidatas) - 1 do
    if TemPrograma(Candidatas[I]) then
    begin
      PastaExistente := Candidatas[I];
      Exit;
    end;
end;

function Atualizando: Boolean;
begin
  Result := PastaExistente <> '';
end;

// Usado em DefaultDirName: se já existe uma instalação, atualiza naquela pasta (assim as DLLs da ACBrLib continuam lá).
function PastaPadrao(Param: String): String;
begin
  ProcurarExistente;
  if Atualizando then Result := PastaExistente
  else Result := ExpandConstant('{autopf}\MercadoPro');
end;

function InitializeSetup: Boolean;
begin
  ProcurarExistente;
  Result := True;
end;

// Atualização: não pergunta a pasta (usa a que já existe).
function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := Atualizando and (PageID = wpSelectDir);
end;

procedure AplicarTemaMercadoPro;
begin
  WizardForm.WelcomeLabel1.Font.Color := $452D14;
  WizardForm.FinishedHeadingLabel.Font.Color := $452D14;
  WizardForm.PageNameLabel.Font.Color := $452D14;
  WizardForm.PageDescriptionLabel.Font.Color := $3A9617;
end;

procedure InitializeWizard;
begin
  AplicarTemaMercadoPro;
  if Atualizando then
  begin
    WizardForm.WelcomeLabel1.Caption := 'Atualizar o MercadoPro';
    WizardForm.WelcomeLabel2.Caption :=
      'Foi encontrada uma instalação nesta pasta:' + #13#10 + PastaExistente + #13#10#13#10 +
      'Só os arquivos do programa serão atualizados. Seus dados (banco, configurações, notas e certificados) e as DLLs da ACBrLib são mantidos.' + #13#10#13#10 +
      'Clique em Avançar para continuar.';
  end;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if not Atualizando then Exit;
  if CurPageID = wpReady then
  begin
    WizardForm.NextButton.Caption := 'Atualizar';
    WizardForm.ReadyLabel.Caption := 'O instalador está pronto para atualizar o MercadoPro em:' + #13#10 + PastaExistente + #13#10#13#10 + 'Clique em Atualizar para continuar.';
  end
  else if CurPageID = wpInstalling then
    WizardForm.StatusLabel.Caption := 'Atualizando o MercadoPro...'
  else if CurPageID = wpFinished then
    WizardForm.FinishedHeadingLabel.Caption := 'Atualização concluída';
end;

