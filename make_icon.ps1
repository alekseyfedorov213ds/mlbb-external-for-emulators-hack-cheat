#
# Phantom — генератор иконки .ico
# Создаёт многоразмерный ICO с логотипом (буква P со свечением на тёмном градиенте).
#
Add-Type -AssemblyName System.Drawing

function New-PhantomBitmap([int]$size) {
    $bmp = New-Object System.Drawing.Bitmap $size, $size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g   = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode     = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit

    # --- Скруглённый rect path ---
    $r = [Math]::Round($size * 0.18)
    $rect = New-Object System.Drawing.Rectangle 0, 0, $size, $size
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddArc($rect.X,                     $rect.Y,                       $r*2, $r*2, 180, 90)
    $path.AddArc($rect.Right  - $r*2 - 1,     $rect.Y,                       $r*2, $r*2, 270, 90)
    $path.AddArc($rect.Right  - $r*2 - 1,     $rect.Bottom - $r*2 - 1,       $r*2, $r*2,   0, 90)
    $path.AddArc($rect.X,                     $rect.Bottom - $r*2 - 1,       $r*2, $r*2,  90, 90)
    $path.CloseFigure()

    # --- Градиент фон (тёмно-синий → фиолетовый) ---
    $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        (New-Object System.Drawing.Point 0, 0),
        (New-Object System.Drawing.Point $size, $size),
        ([System.Drawing.Color]::FromArgb(255, 18, 24, 42)),
        ([System.Drawing.Color]::FromArgb(255, 52, 28, 90)))
    $g.FillPath($brush, $path)
    $brush.Dispose()

    # --- Внутреннее свечение (radial-ish через несколько полупрозрачных кругов) ---
    $cx = $size / 2.0
    $cy = $size * 0.42
    $glowMax = $size * 0.55
    for ($i = 0; $i -lt 16; $i++) {
        $t = $i / 16.0
        $rad = $glowMax * (1.0 - $t)
        $a = [int](70 * $t)
        $col = [System.Drawing.Color]::FromArgb($a, 70, 150, 255)
        $b = New-Object System.Drawing.SolidBrush $col
        $g.FillEllipse($b, [single]($cx - $rad), [single]($cy - $rad), [single]($rad * 2), [single]($rad * 2))
        $b.Dispose()
    }

    # --- Буква P (внутри clip-path рамки) ---
    $g.SetClip($path)
    $fontSize = [single]($size * 0.65)
    $font = New-Object System.Drawing.Font "Segoe UI", $fontSize, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)

    $fmt = New-Object System.Drawing.StringFormat
    $fmt.Alignment     = [System.Drawing.StringAlignment]::Center
    $fmt.LineAlignment = [System.Drawing.StringAlignment]::Center

    # тень буквы (несколько слоёв для glow)
    for ($i = 6; $i -gt 0; $i--) {
        $a = [int](40 - $i * 4)
        if ($a -lt 0) { $a = 0 }
        $glowBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb($a, 100, 180, 255))
        $rectF = New-Object System.Drawing.RectangleF -ArgumentList ([single]$i), ([single]$i), ([single]$size), ([single]$size)
        $g.DrawString("P", $font, $glowBrush, $rectF, $fmt)
        $glowBrush.Dispose()
    }
    # сама буква — почти белая с лёгким голубым оттенком
    $textBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 235, 245, 255))
    $textRect = New-Object System.Drawing.RectangleF -ArgumentList 0, 0, ([single]$size), ([single]$size)
    $g.DrawString("P", $font, $textBrush, $textRect, $fmt)
    $textBrush.Dispose()
    $font.Dispose()

    $g.ResetClip()

    # --- Тонкая глянцевая внутренняя обводка ---
    $pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(80, 255, 255, 255)), 2
    $g.DrawPath($pen, $path)
    $pen.Dispose()

    $g.Dispose()
    $path.Dispose()
    return $bmp
}

function Get-BmpDibBytes($bmp) {
    $w = $bmp.Width
    $h = $bmp.Height
    $rect = New-Object System.Drawing.Rectangle 0, 0, $w, $h
    $data = $bmp.LockBits($rect,
        [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $stride    = $data.Stride
    $byteCount = $stride * $h
    $pixels    = New-Object byte[] $byteCount
    [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $pixels, 0, $byteCount)
    $bmp.UnlockBits($data)

    # ICO/BMP wants bottom-up rows; LockBits gave top-down -> flip.
    $flipped = New-Object byte[] $byteCount
    for ($y = 0; $y -lt $h; $y++) {
        [Array]::Copy($pixels, $y * $stride, $flipped, ($h - 1 - $y) * $stride, $stride)
    }

    # AND mask: 1 bit per pixel, rows padded to 4-byte boundary, bottom-up.
    $andRowSize = [int]([Math]::Ceiling($w / 32.0)) * 4
    $andSize    = $andRowSize * $h
    $andMask    = New-Object byte[] $andSize

    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter $ms
    # BITMAPINFOHEADER (40 bytes)
    $bw.Write([uint32]40)
    $bw.Write([int32]$w)
    $bw.Write([int32]($h * 2))   # height = 2x (XOR + AND mask)
    $bw.Write([uint16]1)
    $bw.Write([uint16]32)
    $bw.Write([uint32]0)         # BI_RGB
    $bw.Write([uint32]0)         # biSizeImage (0 is legal for BI_RGB)
    $bw.Write([int32]0)
    $bw.Write([int32]0)
    $bw.Write([uint32]0)
    $bw.Write([uint32]0)
    $bw.Write($flipped)
    $bw.Write($andMask)
    $result = $ms.ToArray()
    $bw.Dispose()
    $ms.Dispose()
    # Comma forces PowerShell to return byte[] as a single object (no auto-unrolling).
    return ,$result
}

function New-IcoFile([string]$path, [int[]]$sizes) {
    $entries = @()
    foreach ($s in $sizes) {
        $bmp = New-PhantomBitmap $s
        $entries += ,(Get-BmpDibBytes $bmp)
        $bmp.Dispose()
    }

    $fs = [System.IO.File]::Create($path)
    $bw = New-Object System.IO.BinaryWriter $fs
    try {
        # ICONDIR header
        $bw.Write([uint16]0)              # reserved
        $bw.Write([uint16]1)              # type = ICO
        $bw.Write([uint16]$sizes.Count)   # count

        $offset = 6 + 16 * $sizes.Count
        for ($i = 0; $i -lt $sizes.Count; $i++) {
            $s    = $sizes[$i]
            $data = $entries[$i]
            $w = if ($s -ge 256) { 0 } else { $s }   # 0 means 256 in spec
            $h = if ($s -ge 256) { 0 } else { $s }
            $bw.Write([byte]$w)
            $bw.Write([byte]$h)
            $bw.Write([byte]0)             # colour palette
            $bw.Write([byte]0)             # reserved
            $bw.Write([uint16]1)           # color planes
            $bw.Write([uint16]32)          # bits/pixel
            $bw.Write([uint32]$data.Length)
            $bw.Write([uint32]$offset)
            $offset += $data.Length
        }
        foreach ($data in $entries) { $bw.Write($data) }
    } finally {
        $bw.Dispose()
        $fs.Dispose()
    }
}

$outPath = Join-Path $PSScriptRoot "phantom.ico"
New-IcoFile $outPath @(16, 24, 32, 48, 64, 128)
Write-Host "Created: $outPath"
