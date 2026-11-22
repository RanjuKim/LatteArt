# End-to-end tests against supplied photos. All artifacts stay in ignored data/raw/.
# Build first, then run with -Reference <photo> -UserPhoto <rotated/edited photo>.
param(
    [Parameter(Mandatory=$true)][string]$Reference,
    [Parameter(Mandatory=$true)][string]$UserPhoto,
    [string]$OutputDirectory = 'data/raw/jaeku-private/integration-validation/run'
)
$ErrorActionPreference = 'Stop'
$repoPath = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$exePath = Join-Path $repoPath 'bin/Release/LatteArt.exe'
$referencePath = (Resolve-Path -LiteralPath $Reference).Path
$userPath = (Resolve-Path -LiteralPath $UserPhoto).Path
$outPath = [IO.Path]::GetFullPath((Join-Path $repoPath $OutputDirectory))
$privateRoot = [IO.Path]::GetFullPath((Join-Path $repoPath 'data/raw')) + [IO.Path]::DirectorySeparatorChar
if (-not $outPath.StartsWith($privateRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Test output must be under the ignored data/raw directory.'
}
if (-not (Test-Path -LiteralPath $exePath)) { throw 'Build the Release executable first.' }
New-Item -ItemType Directory -Force -Path $outPath | Out-Null
$script:checks = 0
function Check([bool]$Condition, [string]$Name) {
    if (-not $Condition) { throw ('FAIL: ' + $Name) }
    $script:checks++
    Write-Output ('PASS: ' + $Name)
}
function Run-Latte([string]$Log, [string[]]$Arguments, [int]$Expected = 0) {
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    & $exePath @Arguments *> (Join-Path $outPath $Log)
    $code = $LASTEXITCODE
    $ErrorActionPreference = $savedPreference
    if ($code -ne $Expected) { throw "Exit $code, expected $Expected. See $Log" }
}
function Near([double]$A, [double]$B) { return [Math]::Abs($A - $B) -lt 0.001 }
Push-Location $outPath
try {
    # Copies preserve the source bytes; a comma tests CSV path quoting.
    Copy-Item -LiteralPath $referencePath -Destination 'reference.png' -Force
    Copy-Item -LiteralPath $userPath -Destination 'user,rotated.png' -Force
    Run-Latte 'layers-self-test.log' @('--debug','layers','--self-test')
    Add-Type -AssemblyName System.Drawing
    $blank = New-Object Drawing.Bitmap(512,512)
    try { $blank.Save((Join-Path $outPath 'no-cup.png'), [Drawing.Imaging.ImageFormat]::Png) }
    finally { $blank.Dispose() }
    Run-Latte 'single.log' @('reference.png','user,rotated.png','test','single.png','--no-show')
    Run-Latte 'legacy-cli.log' @('reference.png','user,rotated.png','test:99','legacy.png','--no-show')
    Check ((Get-FileHash single.png).Hash -eq (Get-FileHash legacy.png).Hash) 'catalog layer metadata does not change rendering or score'
    Run-Latte 'debug.log' @('--debug','layers','reference.png','user,rotated.png','--prefix=debug')
    $quotedNote = "comma, quote `"hello`"`nsecond line"
    $labels = @(
        [pscustomobject]@{file='reference.png';pattern='test';reference='reference.png';notes='self'},
        [pscustomobject]@{file='user,rotated.png';pattern='test';reference='reference.png';notes=$quotedNote},
        [pscustomobject]@{file='user,rotated.png';pattern='test';reference='reference.png';notes='duplicate file'},
        [pscustomobject]@{file='missing.png';pattern='test';reference='reference.png';notes='missing file'},
        [pscustomobject]@{file='no-cup.png';pattern='test';reference='reference.png';notes='no cup'},
        [pscustomobject]@{file='debug_layers_synthetic_wrong_size.png';pattern='test';reference='debug_layers_synthetic_reference.png';notes='size reject'},
        [pscustomobject]@{file='reference.png';pattern='test';reference='reference.png';notes='continues after errors'}
    )
    $labels | Export-Csv labels.csv -NoTypeInformation -Encoding UTF8
    Run-Latte 'batch.log' @('--batch','labels.csv','.','scores.csv')
    $summary = @(Import-Csv scores.csv)
    $lines = @(Import-Csv scores_lines.csv)
    $matches = @(Import-Csv scores_matches.csv)
    Check ($summary.Count -eq 7) 'batch retains one summary per input row'
    Check ($summary[0].status -eq 'scored' -and (Near ([double]$summary[0].final_score) 100)) 'identical real photo scores 100'
    Check ($summary[1].notes -ceq $quotedNote) 'CSV commas, quotes, newlines and UTF-8 BOM round-trip'
    Check ($summary[1].row_id -ne $summary[2].row_id -and $summary[1].final_score -eq $summary[2].final_score) 'duplicate filenames retain distinct row IDs'
    Check ($summary[3].status -eq 'error' -and $summary[3].error_stage -eq 'load images') 'missing image is recorded as a row error'
    Check ($summary[4].status -eq 'error' -and $summary[4].error_stage -eq 'normalize user') 'no-cup image is rejected during normalization'
    Check ($summary[5].status -eq 'rejected_size' -and $summary[5].error_stage -eq 'pattern size validation') 'wrong-size pattern stops before quality scoring'
    Check ($summary[6].status -eq 'scored') 'batch continues after failed and rejected rows'
    Check (@($summary | Where-Object { $_.status -ne 'scored' -and ($_.final_score -ne '' -or $_.Q -ne '') }).Count -eq 0) 'failed rows have blank scores'
    Check (@($lines | Where-Object { $_.row_id -in @('4','5','6') }).Count -eq 0) 'failed rows produce no misleading line details'
    Check ($summary[0].PSObject.Properties.Name -notcontains 'detected_layers') 'summary contains no global detected layer count'
    Check ($summary[0].PSObject.Properties.Name -notcontains 'layer_correct') 'summary contains no obsolete layer accuracy metric'
    $validLineCounts = $true
    $validMatches = $true
    $validMeans = $true
    foreach ($row in $summary | Where-Object status -eq 'scored') {
        $rowLines = @($lines | Where-Object row_id -eq $row.row_id)
        $validMeans = $validMeans -and $rowLines.Count -eq [int]$row.line_count
        $verticalMean = (@($rowLines | Where-Object direction -eq 'vertical') | Measure-Object -Property final_score -Average).Average
        $horizontalMean = (@($rowLines | Where-Object direction -eq 'horizontal') | Measure-Object -Property final_score -Average).Average
        $mean = ($verticalMean + $horizontalMean) / 2
        $validMeans = $validMeans -and (Near $verticalMean ([double]$row.vertical_score)) -and (Near $horizontalMean ([double]$row.horizontal_score))
        $validMeans = $validMeans -and (Near $mean ([double]$row.final_score))
        foreach ($line in $rowLines) {
            $pairs = @($matches | Where-Object { $_.row_id -eq $row.row_id -and $_.line_index -eq $line.line_index })
            $matched = @($pairs | Where-Object status -eq 'matched')
            $missing = @($pairs | Where-Object status -eq 'missing')
            $extra = @($pairs | Where-Object status -eq 'extra')
            $validLineCounts = $validLineCounts -and ([int]$line.reference_count -eq $matched.Count + $missing.Count) -and ([int]$line.user_count -eq $matched.Count + $extra.Count)
            $validMatches = $validMatches -and ([int]$line.missing -eq $missing.Count) -and ([int]$line.extra -eq $extra.Count)
            $lastRef = -1; $lastUser = -1
            foreach ($pair in $matched) {
                $validMatches = $validMatches -and [int]$pair.reference_index -gt $lastRef -and [int]$pair.user_index -gt $lastUser
                $lastRef = [int]$pair.reference_index; $lastUser = [int]$pair.user_index
            }
            foreach ($pair in $missing) { $validMatches = $validMatches -and $pair.user_index -eq '' -and $pair.user_position -eq '' }
            foreach ($pair in $extra) { $validMatches = $validMatches -and $pair.reference_index -eq '' -and $pair.reference_position -eq '' }
        }
    }
    Check $validLineCounts 'detail rows account for every peak on every line'
    Check $validMatches 'correspondence indices remain ordered and unmatched sides stay blank'
    Check $validMeans 'direction means have equal weight in each image final score'
    $debugLines = @(Import-Csv debug_lines.csv)
    $batchLines = @($lines | Where-Object row_id -eq '2')
    $sameLines = $debugLines.Count -eq $batchLines.Count
    for ($i = 0; $i -lt $debugLines.Count -and $sameLines; $i++) {
        foreach ($field in @('coordinate','reference_count','user_count','missing','extra','spacing','thickness','U','final_score')) {
            $sameLines = $sameLines -and $debugLines[$i].direction -eq $batchLines[$i].direction -and (Near ([double]$debugLines[$i].$field) ([double]$batchLines[$i].$field))
        }
    }
    Check $sameLines 'debug and batch export the same per-line results'
    $console = Get-Content single.log -Raw
    $scoreText = [regex]::Match($console, 'FINAL SCORE\s*:\s*([0-9.]+)').Groups[1].Value
    Check ($scoreText -ne '' -and (Near ([double]$scoreText) ([double]$summary[1].final_score))) 'console and batch final scores agree'
    Check ($console -notmatch 'Your layers|Layer error|Target layers') 'console reports lines instead of obsolete global counts'
    $image = [Drawing.Image]::FromFile((Join-Path $outPath 'single.png'))
    try { Check ($image.Width -eq 1040 -and $image.Height -eq 1232) 'normal CLI saves the complete comparison image' }
    finally { $image.Dispose() }
    # Legacy input still resolves the reference by catalog name; true_layers is ignored.
    New-Item -ItemType Directory -Force -Path data/reference | Out-Null
    Copy-Item reference.png data/reference/test3.jpg -Force
    'file,pattern,target_layers,true_layers,notes', 'reference.png,test,3,999,legacy' | Set-Content legacy-labels.csv -Encoding UTF8
    Run-Latte 'legacy-batch.log' @('--batch','legacy-labels.csv','.','legacy-scores.csv')
    $legacy = @(Import-Csv legacy-scores.csv)
    Check ($legacy[0].status -eq 'scored' -and (Near ([double]$legacy[0].final_score) 100)) 'legacy CSV still works without using true_layers as a score target'
    $hashBefore = (Get-FileHash labels.csv).Hash
    Run-Latte 'overlap.log' @('--batch','labels.csv','.','labels.csv') 2
    Check ((Get-FileHash labels.csv).Hash -eq $hashBefore) 'batch rejects overwriting its label input'
    'file,pattern,reference', 'reference.png,test,"unterminated' | Set-Content malformed.csv -Encoding UTF8
    Run-Latte 'malformed.log' @('--batch','malformed.csv','.','malformed-out.csv') 2
    Check (-not (Test-Path malformed-out.csv)) 'malformed CSV fails before output'
    Run-Latte 'missing-output.log' @('--batch','labels.csv','.','nonexistent-directory/scores.csv') 2
    Check (-not (Test-Path nonexistent-directory/scores.csv)) 'output failure returns a nonzero exit code'
    Write-Output ("Integration checks passed: " + $script:checks)
    Write-Output ("Artifacts: " + $outPath)
} finally { Pop-Location }
