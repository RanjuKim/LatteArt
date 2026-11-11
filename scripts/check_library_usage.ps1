# Lists every cv:: usage that is not on the allowed list.
# Assignment rule: only OpenCV input/output functions may be used.
# Usage (from repo root):  powershell -ExecutionPolicy Bypass -File scripts\check_library_usage.ps1

$allowedEverywhere = @(
    'Mat', 'Point', 'Point2f', 'Point2i', 'Point2d', 'Size', 'Vec3b', 'Vec3f',
    'Exception'
)
$allowedInIo = @('imread', 'imwrite', 'imshow', 'waitKey', 'IMREAD_COLOR', 'IMREAD_GRAYSCALE')

$root = Join-Path $PSScriptRoot '..\src'
$violations = 0

Get-ChildItem $root -Recurse -Include *.cpp, *.h | ForEach-Object {
    $file = $_
    $inIo = $file.FullName -match '[\\/]src[\\/]io[\\/]'
    $lineNo = 0
    foreach ($line in Get-Content $file.FullName) {
        $lineNo++
        $code = ($line -split '//')[0]
        foreach ($m in [regex]::Matches($code, 'cv::(\w+)')) {
            $name = $m.Groups[1].Value
            if ($allowedEverywhere -contains $name) { continue }
            if ($inIo -and $allowedInIo -contains $name) { continue }
            $rel = $file.FullName.Substring((Resolve-Path $root).Path.Length + 1)
            Write-Host ("{0}:{1}  cv::{2}" -f $rel, $lineNo, $name) -ForegroundColor Yellow
            $violations++
        }
    }
}

if ($violations -eq 0) {
    Write-Host 'OK: no forbidden OpenCV calls found.' -ForegroundColor Green
    exit 0
}
Write-Host ("{0} forbidden cv:: usage(s). Replace them or document them in the report." -f $violations) -ForegroundColor Red
exit 1
