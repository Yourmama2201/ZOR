$cert = Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -match 'WDKTestCert' }
if ($cert) {
    $cerPath = "$env:TEMP\wdktest.cer"
    Export-Certificate -Cert $cert -FilePath $cerPath -Type CERT | Out-Null
    Import-Certificate -FilePath $cerPath -CertStoreLocation Cert:\LocalMachine\Root | Out-Null
    Write-Host "Certificate installed"
}

$driverPath = "C:\Windows\System32\drivers\nxs_drv.sys"
if (Test-Path $driverPath) {
    sc.exe stop nxs_drv 2>$null
    sc.exe delete nxs_drv 2>$null
    Start-Sleep -Seconds 1
    sc.exe create nxs_drv binPath= $driverPath type= kernel start= demand | Out-Null
    sc.exe start nxs_drv
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Driver started OK"
    } else {
        Write-Host "Driver FAILED with exit code $LASTEXITCODE"
        sc.exe query nxs_drv
    }
} else {
    Write-Host "Driver file not found at $driverPath"
}
pause
