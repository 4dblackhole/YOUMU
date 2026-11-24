param(
    [string]$Folder = (Join-Path $PSScriptRoot '..\Assets\Skins\Default Skin\InGame\Judgements'),
    [string]$Preview = ''
)
# Builds the BAD-recovery judgement words (Recovery.MAX.png .. Recovery.GOOD.png) from the plain ones:
# the same art with the lower part of the letters' fill turning purple. The dark outline keeps its colour.
# -Preview <png> also writes a 6x side-by-side sheet (plain above, recovery below) for review.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public static class RecoveryTint
{
    // Fill pixels (light enough to be the letter face) blend towards purple below `start` of the height.
    public static Bitmap Make(Bitmap source, Color purple, double start)
    {
        var result = new Bitmap(source.Width, source.Height, PixelFormat.Format32bppArgb);
        using (var g = Graphics.FromImage(result)) g.DrawImage(source, 0, 0, source.Width, source.Height);
        var rect = new Rectangle(0, 0, result.Width, result.Height);
        var data = result.LockBits(rect, ImageLockMode.ReadWrite, PixelFormat.Format32bppArgb);
        var bytes = new byte[data.Stride * data.Height];
        Marshal.Copy(data.Scan0, bytes, 0, bytes.Length);
        int top = result.Height, bottom = -1;
        for (int y = 0; y < result.Height; ++y)
            for (int x = 0; x < result.Width; ++x)
                if (bytes[y * data.Stride + x * 4 + 3] > 0) { top = Math.Min(top, y); bottom = Math.Max(bottom, y); }
        for (int y = 0; y < result.Height; ++y)
        {
            double t = bottom > top ? (double)(y - top) / (bottom - top) : 0;
            double amount = t <= start ? 0 : Math.Min(1.0, (t - start) / (1.0 - start));
            for (int x = 0; x < result.Width; ++x)
            {
                int i = y * data.Stride + x * 4;
                double b = bytes[i], gr = bytes[i + 1], r = bytes[i + 2];
                double luminance = 0.299 * r + 0.587 * gr + 0.114 * b;
                if (bytes[i + 3] == 0 || luminance < 80 || amount <= 0) continue;
                bytes[i] = (byte)Math.Round(b + (purple.B - b) * amount);
                bytes[i + 1] = (byte)Math.Round(gr + (purple.G - gr) * amount);
                bytes[i + 2] = (byte)Math.Round(r + (purple.R - r) * amount);
            }
        }
        Marshal.Copy(bytes, 0, data.Scan0, bytes.Length);
        result.UnlockBits(data);
        return result;
    }
}
'@
$purple = [System.Drawing.Color]::FromArgb(178, 74, 255)
$names = 'MAX', 'PERFECT', 'GREAT', 'GOOD'
foreach ($name in $names) {
    $source = [System.Drawing.Bitmap]::new((Join-Path $Folder "$name.png"))
    try {
        $made = [RecoveryTint]::Make($source, $purple, 0.35)
        $made.Save((Join-Path $Folder "Recovery.$name.png"), [System.Drawing.Imaging.ImageFormat]::Png)
        $made.Dispose()
    }
    finally { $source.Dispose() }
    "wrote Recovery.$name.png"
}
if ($Preview) {
    $scale = 6; $gap = 12
    $plain = @(); $tinted = @()
    foreach ($name in $names) {
        $plain += [System.Drawing.Bitmap]::new((Join-Path $Folder "$name.png"))
        $tinted += [System.Drawing.Bitmap]::new((Join-Path $Folder "Recovery.$name.png"))
    }
    [int]$width = $gap; [int]$tallest = 0
    foreach ($image in $plain) { $width += $image.Width * $scale + $gap; $tallest = [Math]::Max($tallest, $image.Height) }
    $sheet = [System.Drawing.Bitmap]::new($width, [int]($tallest * $scale * 2 + $gap * 3))
    $g = [System.Drawing.Graphics]::FromImage($sheet)
    $g.Clear([System.Drawing.Color]::FromArgb(24, 30, 48))
    $g.InterpolationMode = 'NearestNeighbor'
    $x = $gap
    for ($i = 0; $i -lt $plain.Count; $i++) {
        $w = $plain[$i].Width * $scale; $h = $plain[$i].Height * $scale
        $g.DrawImage($plain[$i], $x, $gap, $w, $h)
        $g.DrawImage($tinted[$i], $x, $gap * 2 + $tallest * $scale, $w, $h)
        $x += $w + $gap
        $plain[$i].Dispose(); $tinted[$i].Dispose()
    }
    $g.Dispose()
    $sheet.Save($Preview, [System.Drawing.Imaging.ImageFormat]::Png)
    $sheet.Dispose()
    "wrote preview $Preview"
}
