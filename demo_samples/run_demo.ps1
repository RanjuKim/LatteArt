# Runs every demo case from the repository root and prints the final score or the error.
# Usage (from the repository root): powershell -ExecutionPolicy Bypass -File demo_samples\run_demo.ps1
$ErrorActionPreference = 'Continue'
$root = Resolve-Path (Join-Path $PSScriptRoot '..')
Set-Location $root
$exe = 'bin\Release\LatteArt.exe'
if (-not (Test-Path $exe)) { Write-Output 'Build Release x64 first.'; exit 1 }

$cases = @(
    @('1 high', 'reference\Pro_Tulip_3.png', '1_high_score\Pro_Tulip_3__scale90.png', 'tulip'),
    @('1 high', 'reference\Pro_Heart_3.png', '1_high_score\Pro_Heart_3__rot10.png', 'heart'),
    @('1 high', 'reference\Pro_Tulip_4.png', '1_high_score\Pro_Tulip_4__scale90.png', 'tulip'),
    @('1 high', 'reference\Pro_Heart_3.png', '1_high_score\Pro_Heart_3__dark.png', 'heart'),
    @('1 high', 'reference\Pro_Tulip_3.png', '1_high_score\Pro_Tulip_3__rot10.png', 'tulip'),
    @('2 merged', 'reference\Pro_Tulip_4.png', '2_layers_merged\Pro_Tulip_4__thick.png', 'tulip'),
    @('3 good', 'reference\Pro_Tulip_3.png', '3_practice_good\Tulip_Practice_11.png', 'tulip'),
    @('3 good', 'reference\Pro_Tulip_3.png', '3_practice_good\Tulip_Practice_18.png', 'tulip'),
    @('3 good', 'reference\Pro_Heart_3.png', '3_practice_good\Heart_Practice_18.png', 'heart'),
    @('3 good', 'reference\Pro_Tulip_3.png', '3_practice_good\Tulip_Practice_9.png', 'tulip'),
    @('3 good', 'reference\Pro_Heart_3.png', '3_practice_good\Heart_Practice_14.png', 'heart'),
    @('4 bad', 'reference\Pro_Tulip_1.png', '4_practice_bad\Tulip_Practice_6.png', 'tulip'),
    @('4 bad', 'reference\Pro_Tulip_1.png', '4_practice_bad\Tulip_Practice_8.png', 'tulip'),
    @('4 bad', 'reference\Pro_Heart_3.png', '4_practice_bad\Heart_Practice_6.png', 'heart'),
    @('5 fail', 'reference\Pro_Heart_3.png', '5_should_fail\no_cup_gray.png', 'heart'),
    @('5 fail', 'reference\Pro_Tulip_3.png', '5_should_fail\Tulip_Practice_1.png', 'tulip'),
    @('5 fail', 'reference\Pro_Heart_3.png', '5_should_fail\Heart_Practice_21.png', 'heart'),
    @('5 fail', 'reference\Pro_Heart_3.png', '5_should_fail\Heart_Practice_5.png', 'heart')
)

foreach ($c in $cases) {
    $ref = Join-Path 'demo_samples' $c[1]
    $user = Join-Path 'demo_samples' $c[2]
    $out = $user -replace '\.png$', '_result.png'
    $lines = & $exe $ref $user $c[3] $out --no-show 2>&1 | ForEach-Object { "$_" }
    $final = $lines | Where-Object { $_ -match 'FINAL SCORE' } | Select-Object -First 1
    $err = $lines | Where-Object { $_ -match '\[error\]' } | Select-Object -First 1
    $result = if ($final) { ($final -replace '.*FINAL SCORE\s*:\s*', '').Trim() } else { "$err".Trim() }
    Write-Output ('{0,-9} {1,-45} {2}' -f $c[0], (Split-Path $user -Leaf), $result)
}
