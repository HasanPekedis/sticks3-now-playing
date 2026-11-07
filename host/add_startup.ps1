# "Simdi Calan" servisini Windows acilisina ekler. Kaldirmak icin: shell:startup klasorundeki "M5 Simdi Calan" kisayolunu sil.
$pyw = python -c "import sys,os;print(os.path.join(os.path.dirname(sys.executable),'pythonw.exe'))"
if (-not (Test-Path $pyw)) { $pyw = "$env:LOCALAPPDATA\Microsoft\WindowsApps\pythonw.exe" }
$svc = Join-Path $PSScriptRoot 'host_service.py'
$lnk = Join-Path ([Environment]::GetFolderPath('Startup')) 'M5 Simdi Calan.lnk'
$sc = (New-Object -ComObject WScript.Shell).CreateShortcut($lnk)
$sc.TargetPath = $pyw
$sc.Arguments = "`"$svc`""
$sc.WorkingDirectory = $PSScriptRoot
$sc.WindowStyle = 7
$sc.Save()
"Eklendi: $lnk"
