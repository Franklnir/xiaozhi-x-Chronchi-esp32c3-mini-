param(
    [Parameter(Mandatory = $true)][string]$Firmware,
    [Parameter(Mandatory = $true)][string]$Version,
    [Parameter(Mandatory = $true)][string]$Board,
    [Parameter(Mandatory = $true)][string]$DownloadUrl,
    [Parameter(Mandatory = $true)][string]$PrivateKey,
    [Parameter(Mandatory = $true)][string]$OutputManifest,
    [string]$Notes = '',
    [switch]$Mandatory
)

$ErrorActionPreference = 'Stop'
if (-not (Get-Command openssl -ErrorAction SilentlyContinue)) {
    throw 'OpenSSL tidak ditemukan di PATH.'
}
if ($DownloadUrl -notmatch '^https://') { throw 'DownloadUrl wajib memakai HTTPS.' }
if ($Notes.Contains("`n") -or $Notes.Contains("`r")) {
    throw 'Notes harus satu baris agar canonical signature tidak ambigu.'
}

$firmwarePath = (Resolve-Path -LiteralPath $Firmware).Path
$privateKeyPath = (Resolve-Path -LiteralPath $PrivateKey).Path
$manifestPath = [System.IO.Path]::GetFullPath($OutputManifest)
$size = (Get-Item -LiteralPath $firmwarePath).Length
$sha256 = (Get-FileHash -LiteralPath $firmwarePath -Algorithm SHA256).Hash.ToLowerInvariant()
$mandatoryText = $Mandatory.IsPresent.ToString().ToLowerInvariant()
$deviceCanonical = "ESPBridge-OTA-v1`n$Board`n$Version`n$size`n$sha256"
$deviceCanonicalFile = [System.IO.Path]::GetTempFileName()
$deviceSignatureFile = [System.IO.Path]::GetTempFileName()
$canonicalFile = [System.IO.Path]::GetTempFileName()
$signatureFile = [System.IO.Path]::GetTempFileName()

try {
    [System.IO.File]::WriteAllText(
        $deviceCanonicalFile,
        $deviceCanonical,
        [System.Text.UTF8Encoding]::new($false)
    )
    & openssl dgst -sha256 -sign $privateKeyPath -out $deviceSignatureFile $deviceCanonicalFile
    if ($LASTEXITCODE -ne 0) { throw 'Penandatanganan metadata perangkat gagal.' }
    $deviceSignature = [Convert]::ToBase64String([IO.File]::ReadAllBytes($deviceSignatureFile))
    $canonical = "2`n$Board`n$Version`n$size`n$sha256`n$DownloadUrl`n$mandatoryText`n$Notes`n$deviceSignature"
    [System.IO.File]::WriteAllText(
        $canonicalFile,
        $canonical,
        [System.Text.UTF8Encoding]::new($false)
    )
    & openssl dgst -sha256 -sign $privateKeyPath -out $signatureFile $canonicalFile
    if ($LASTEXITCODE -ne 0) { throw 'Penandatanganan manifest gagal.' }
    $signature = [Convert]::ToBase64String([IO.File]::ReadAllBytes($signatureFile))
    $manifest = [ordered]@{
        schema = 2
        board = $Board
        version = $Version
        size = $size
        sha256 = $sha256
        url = $DownloadUrl
        mandatory = $Mandatory.IsPresent
        notes = $Notes
        deviceSignature = $deviceSignature
        signature = $signature
    }
    $parent = Split-Path -Parent $manifestPath
    if ($parent) { [System.IO.Directory]::CreateDirectory($parent) | Out-Null }
    [System.IO.File]::WriteAllText(
        $manifestPath,
        ($manifest | ConvertTo-Json -Depth 3),
        [System.Text.UTF8Encoding]::new($false)
    )
    Write-Host "Manifest bertanda tangan: $manifestPath"
    Write-Host "Firmware SHA-256: $sha256"
} finally {
    Remove-Item -LiteralPath $deviceCanonicalFile -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $deviceSignatureFile -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $canonicalFile -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $signatureFile -Force -ErrorAction SilentlyContinue
}
