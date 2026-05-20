$ROOT = Split-Path -Parent $MyInvocation.MyCommand.Path
$src  = Join-Path $ROOT 'adb_src'
$out  = Join-Path $ROOT 'adb_raw.h'

$files = @(
    @{ Var='g_adb_exe';        Size='ADB_EXE_SIZE';        Path="$src\adb.exe" },
    @{ Var='g_adbwinapi_dll';  Size='ADBWINAPI_DLL_SIZE';  Path="$src\AdbWinApi.dll" },
    @{ Var='g_adbwinusb_dll';  Size='ADBWINUSB_DLL_SIZE';  Path="$src\AdbWinUsbApi.dll" }
)

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine('#pragma once')
[void]$sb.AppendLine('#include <cstddef>')
[void]$sb.AppendLine('')

foreach ($f in $files) {
    $bytes = [IO.File]::ReadAllBytes($f.Path)
    Write-Host "Embedding $($f.Path): $($bytes.Length) bytes"
    [void]$sb.AppendLine("#define $($f.Size) $($bytes.Length)")
    [void]$sb.AppendLine("static const unsigned char $($f.Var)[$($f.Size)] = {")
    $line = New-Object System.Text.StringBuilder
    for ($i = 0; $i -lt $bytes.Length; $i++) {
        [void]$line.Append('0x')
        [void]$line.Append($bytes[$i].ToString('x2'))
        [void]$line.Append(',')
        if (($i + 1) % 24 -eq 0) {
            [void]$sb.AppendLine($line.ToString())
            [void]$line.Clear()
        }
    }
    if ($line.Length -gt 0) { [void]$sb.AppendLine($line.ToString()) }
    [void]$sb.AppendLine('};')
    [void]$sb.AppendLine('')
}

[IO.File]::WriteAllText($out, $sb.ToString())
Write-Host "Wrote $out ($((Get-Item $out).Length) bytes)"
