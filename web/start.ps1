$ErrorActionPreference = 'Stop'
$qrProjectRoot = Split-Path -Parent $PSScriptRoot
$qrRuntimeDir = Join-Path $PSScriptRoot '.runtime'
$qrLanIp = Get-NetIPAddress -AddressFamily IPv4 -InterfaceAlias 'Wi-Fi' |
    Where-Object { $_.IPAddress -notlike '169.254.*' } |
    Select-Object -First 1 -ExpandProperty IPAddress
if (-not $qrLanIp) { throw 'Khong tim thay dia chi IPv4 Wi-Fi cua may tinh.' }

$qrExisting = $null
try { $qrExisting = Invoke-RestMethod -Uri 'http://127.0.0.1:8765/api/status' -TimeoutSec 2 } catch {}
if ($qrExisting) {
    if ($qrExisting.base_url -ne "http://${qrLanIp}:8765/checkin") {
        throw "Web dang chay voi URL $($qrExisting.base_url). Can khoi dong lai server khi doi dia chi Wi-Fi."
    }
    Write-Output 'Web test da dang chay.'
} else {
    # Never stop another application's process if the port is already occupied.
    $qrPortOwner = Get-NetTCPConnection -LocalPort 8765 -State Listen -ErrorAction SilentlyContinue
    if ($qrPortOwner) { throw 'Cong 8765 dang duoc ung dung khac su dung.' }
    New-Item -ItemType Directory -Path $qrRuntimeDir -Force | Out-Null
    $qrPythonExe = (Get-Command python).Source
    $qrServerProcess = Start-Process -FilePath $qrPythonExe `
        -ArgumentList @('web/server.py', '--lan-ip', $qrLanIp, '--port', '8765') `
        -WorkingDirectory $qrProjectRoot -WindowStyle Hidden `
        -RedirectStandardOutput (Join-Path $qrRuntimeDir 'server.stdout.log') `
        -RedirectStandardError (Join-Path $qrRuntimeDir 'server.stderr.log') -PassThru
    $qrServerProcess.Id | Set-Content -LiteralPath (Join-Path $qrRuntimeDir 'server.pid')
    $qrReady = $false
    for ($qrAttempt = 0; $qrAttempt -lt 20; $qrAttempt++) {
        try {
            $qrStatus = Invoke-RestMethod -Uri 'http://127.0.0.1:8765/api/status' -TimeoutSec 1
            if ($qrStatus.base_url -eq "http://${qrLanIp}:8765/checkin") { $qrReady = $true; break }
        } catch {}
        Start-Sleep -Milliseconds 250
    }
    if (-not $qrReady) { throw 'Web chua san sang. Xem log trong web/.runtime.' }
}
Write-Output 'May tinh: http://127.0.0.1:8765/'
Write-Output "Dien thoai cung Wi-Fi: http://${qrLanIp}:8765/"
Write-Output "URL trong firmware phai khop: http://${qrLanIp}:8765/checkin"

