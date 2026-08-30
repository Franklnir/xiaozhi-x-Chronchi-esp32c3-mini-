param(
    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
if (-not (Get-Command openssl -ErrorAction SilentlyContinue)) {
    throw 'OpenSSL tidak ditemukan di PATH.'
}

$target = [System.IO.Path]::GetFullPath($OutputDirectory)
[System.IO.Directory]::CreateDirectory($target) | Out-Null
$privateKey = Join-Path $target 'espbridge-ota-private.pem'
$publicPem = Join-Path $target 'espbridge-ota-public.pem'
$publicDer = Join-Path $target 'espbridge-ota-public.der'

if ((Test-Path -LiteralPath $privateKey) -or (Test-Path -LiteralPath $publicPem) -or
    (Test-Path -LiteralPath $publicDer)) {
    throw "File key sudah ada di $target; gunakan direktori kosong agar key tidak tertimpa."
}

& openssl ecparam -name prime256v1 -genkey -noout -out $privateKey
if ($LASTEXITCODE -ne 0) { throw 'Gagal membuat private key OTA.' }
& openssl ec -in $privateKey -pubout -out $publicPem
if ($LASTEXITCODE -ne 0) { throw 'Gagal membuat public key OTA.' }
& openssl pkey -pubin -in $publicPem -outform DER -out $publicDer
if ($LASTEXITCODE -ne 0) { throw 'Gagal mengubah public key ke format DER.' }

$publicBase64 = [Convert]::ToBase64String([IO.File]::ReadAllBytes($publicDer))
Write-Host "Key OTA dibuat di: $target"
Write-Host 'Simpan private key di secret manager/HSM dan jangan masukkan ke repository.'
Write-Host "ESPBRIDGE_OTA_PUBLIC_KEY_B64=$publicBase64"
