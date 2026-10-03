# Confere se o pacote (pasta dist) tem TODAS as DLLs que o programa e as bibliotecas dele precisam.
# Lê a tabela de importação de cada .exe/.dll (sem depender de ferramentas externas).
param([string]$Pasta = 'dist')

$ErrorActionPreference = 'Stop'

function Get-DllsImportadas([string]$arquivo) {
    $b = [IO.File]::ReadAllBytes($arquivo)
    if ($b.Length -lt 64 -or $b[0] -ne 0x4D -or $b[1] -ne 0x5A) { return @() }
    $pe = [BitConverter]::ToInt32($b, 0x3C)
    if ($pe -lt 0 -or ($pe + 24) -gt $b.Length -or [BitConverter]::ToUInt32($b, $pe) -ne 0x00004550) { return @() }
    $nSec    = [BitConverter]::ToUInt16($b, $pe + 6)
    $optSize = [BitConverter]::ToUInt16($b, $pe + 20)
    $opt     = $pe + 24
    $magic   = [BitConverter]::ToUInt16($b, $opt)
    $ddOff   = if ($magic -eq 0x20B) { $opt + 112 } else { $opt + 96 }
    $impRva  = [BitConverter]::ToUInt32($b, $ddOff + 8)
    if ($impRva -eq 0) { return @() }
    $secTab  = $opt + $optSize

    $rvaParaOffset = {
        param([uint32]$rva)
        for ($i = 0; $i -lt $nSec; $i++) {
            $s   = $secTab + 40 * $i
            $vs  = [BitConverter]::ToUInt32($b, $s + 8)
            $va  = [BitConverter]::ToUInt32($b, $s + 12)
            $rs  = [BitConverter]::ToUInt32($b, $s + 16)
            $raw = [BitConverter]::ToUInt32($b, $s + 20)
            $tam = [Math]::Max($vs, $rs)
            if ($rva -ge $va -and $rva -lt ($va + $tam)) { return [int]($rva - $va + $raw) }
        }
        return -1
    }

    $nomes = New-Object System.Collections.Generic.List[string]
    $o = & $rvaParaOffset $impRva
    if ($o -lt 0) { return @() }
    while (($o + 20) -le $b.Length) {
        $nomeRva = [BitConverter]::ToUInt32($b, $o + 12)
        if ($nomeRva -eq 0) { break }
        $no = & $rvaParaOffset $nomeRva
        if ($no -lt 0) { break }
        $fim = $no
        while ($fim -lt $b.Length -and $b[$fim] -ne 0) { $fim++ }
        $nomes.Add([Text.Encoding]::ASCII.GetString($b, $no, $fim - $no))
        $o += 20
    }
    return $nomes.ToArray()
}

$raiz = (Resolve-Path $Pasta).Path
$temNaPasta = @{}
Get-ChildItem $raiz -Recurse -Filter *.dll | ForEach-Object { $temNaPasta[$_.Name.ToLower()] = $true }

# DLLs que o Windows fornece; e as que ficam na instalacao do cliente (o script de atualizacao preserva):
# libpq/openssl/etc. (usadas pelo driver do PostgreSQL) e ACBrLib.
$externasPermitidas = '^(libpq|libssl.*|libcrypto.*|libintl.*|libiconv.*|libwinpthread.*|libgcc_s.*|libstdc\+\+.*|acbr.*)\.dll$'

$faltando = @{}
$arquivos = Get-ChildItem $raiz -Recurse -Include *.dll, *.exe
foreach ($f in $arquivos) {
    foreach ($n in (Get-DllsImportadas $f.FullName)) {
        $nome = $n.ToLower()
        if ($temNaPasta.ContainsKey($nome)) { continue }
        if ($nome -match '^(api-ms-win|ext-ms-win)-') { continue }
        if (Test-Path (Join-Path $env:WINDIR "System32\$nome")) { continue }
        if ($nome -match $externasPermitidas) { continue }
        if (-not $faltando.ContainsKey($nome)) { $faltando[$nome] = New-Object System.Collections.Generic.List[string] }
        $faltando[$nome].Add($f.Name)
    }
}

# runtime do Visual C++ junto do programa (evita "ponto de entrada nao encontrado" em PCs com runtime antigo)
$runtime = @('vcruntime140.dll', 'msvcp140.dll')
$semRuntime = $runtime | Where-Object { -not $temNaPasta.ContainsKey($_) }

# arquivos essenciais
$essenciais = @('QEstoqueLoja.exe', 'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'Qt6Sql.dll', 'Qt6Qml.dll',
                'QtRPT.dll', 'quazip1-qt6.dll', 'zint.dll', 'zlib1.dll', 'platforms\qwindows.dll',
                'sqldrivers\qsqlite.dll', 'sqldrivers\qsqlpsql.dll')
$semEssencial = $essenciais | Where-Object { -not (Test-Path (Join-Path $raiz $_)) }

Write-Host "Arquivos verificados: $($arquivos.Count)"
$erro = $false
if ($semEssencial) { Write-Host "FALTAM ARQUIVOS ESSENCIAIS: $($semEssencial -join ', ')"; $erro = $true }
if ($semRuntime)   { Write-Host "FALTA O RUNTIME DO VISUAL C++: $($semRuntime -join ', ')"; $erro = $true }
if ($faltando.Count -gt 0) {
    $erro = $true
    Write-Host 'DLLs que o pacote precisa e nao tem:'
    foreach ($k in ($faltando.Keys | Sort-Object)) { Write-Host "  $k  (usada por: $(($faltando[$k] | Select-Object -Unique) -join ', '))" }
}
if ($erro) { exit 1 }
Write-Host 'Pacote completo: todas as dependencias estao presentes.'
