$ROOT = Split-Path -Parent $MyInvocation.MyCommand.Path
$bytes = [System.IO.File]::ReadAllBytes((Join-Path $ROOT 'daemon_src\daemon'))
$lines = @('unsigned char g_daemon_raw[] = {')
$row = '    '
for ($i = 0; $i -lt $bytes.Length; $i++) {
    $row += '0x' + [Convert]::ToString($bytes[$i], 16).PadLeft(2,'0') + ', '
    if (($i + 1) % 16 -eq 0) {
        $lines += $row
        $row = '    '
    }
}
if ($row.Length -gt 4) {
    $lines += $row
}
$lines += '};'
$lines += '#define DAEMON_RAW_SIZE ' + $bytes.Length
[System.IO.File]::WriteAllLines((Join-Path $ROOT 'daemon_raw.h'), $lines)
Write-Host 'daemon_raw.h created, size=' $bytes.Length
