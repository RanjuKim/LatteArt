# Build Release first. Exercises the real Tulip_Practice_10 failure and verifies
# both recovery branches, determinism, and diagnostics on unsuccessful inputs.
param([string]$OutputDirectory='data/raw/jaeku-private/cup-recovery/tests')
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$exe=Join-Path $repo 'bin/Release/LatteArt.exe'
$dest=[IO.Path]::GetFullPath((Join-Path $repo $OutputDirectory))
$privateRoot=[IO.Path]::GetFullPath((Join-Path $repo 'data/raw'))+[IO.Path]::DirectorySeparatorChar
if(-not $dest.StartsWith($privateRoot,[StringComparison]::OrdinalIgnoreCase)){throw 'Output must be under data/raw'}
New-Item -ItemType Directory -Force $dest|Out-Null
Copy-Item -LiteralPath (Join-Path $repo 'pictures/practice/Tulip_Practice_10.png') -Destination (Join-Path $dest 'input.png') -Force
$script:checks=0
function Check([bool]$condition,[string]$name){if(-not $condition){throw ('FAIL: '+$name)};$script:checks++;Write-Output ('PASS: '+$name)}
function Run([string]$prefix,[string[]]$options,[int]$expected=0,[string]$image='input.png'){
 $saved=$ErrorActionPreference;$ErrorActionPreference='Continue'
 & $exe --debug normalize $image "--prefix=$prefix" @options *> ($prefix+'.log')
 $code=$LASTEXITCODE;$ErrorActionPreference=$saved
 if($code -ne $expected){throw ('Unexpected exit for '+$prefix+': '+$code)}
}
Push-Location $dest
try {
 Run 'merged' @('--ransac-iterations=0')
 $merged=@(Import-Csv merged_candidates.csv|Where-Object {$_.method -eq 'merged arcs' -and $_.accepted -eq '1'})
 Check ($merged.Count -gt 0) 'separate real cup arcs yield a supported merged ellipse'
 Check ((Get-Content merged.log -Raw) -match 'selected rim: merged arcs') 'arc recovery works without RANSAC'
 Check ([double]$merged[0].coverage -ge .6 -and [double]$merged[0].error -lt .05) 'merged arc fit obeys original coverage and error limits'
 Run 'robust' @()
 $robust=@(Import-Csv robust_candidates.csv|Where-Object {$_.method -eq 'RANSAC' -and $_.accepted -eq '1'})
 Check ($robust.Count -gt 0) 'RANSAC recovers the rim connected to the handle'
 Check ([double]$robust[0].inlier_fraction -ge .6 -and [double]$robust[0].inlier_fraction -lt 1) 'handle recovery requires majority support and excludes outliers'
 Check ((Get-Content robust.log -Raw) -match 'selected rim: RANSAC') 'supported robust fit participates in final cup selection'
 $log=Get-Content robust.log -Raw
 $geometry=[regex]::Match($log,'liquid center ([0-9.]+) ([0-9.]+), axes ([0-9.]+) ([0-9.]+)')
 Check ($geometry.Success -and [Math]::Abs([double]$geometry.Groups[1].Value-225) -lt 8 -and [Math]::Abs([double]$geometry.Groups[2].Value-278) -lt 8 -and [Math]::Abs([double]$geometry.Groups[3].Value-182) -lt 8) 'recovered liquid ellipse follows the actual surface'
 Run 'repeat' @()
 Check ((Get-FileHash robust_cup.png).Hash -eq (Get-FileHash repeat_cup.png).Hash) 'RANSAC recovery is reproducible'
 Add-Type -AssemblyName System.Drawing
 $cup=[Drawing.Image]::FromFile((Join-Path $dest 'robust_cup.png'))
 try{Check ($cup.Width -eq 512 -and $cup.Height -eq 512) 'recovery preserves the canonical image contract'}finally{$cup.Dispose()}
 $blank=[Drawing.Bitmap]::new(128,128)
 try{$blank.Save((Join-Path $dest 'blank.png'),[Drawing.Imaging.ImageFormat]::Png)}finally{$blank.Dispose()}
 Run 'blank' @() 2 'blank.png'
 Check ((Test-Path blank_edges.png) -and (Test-Path blank_candidates.csv) -and (Test-Path blank_candidates.png)) 'failed detection still saves edges and rejection diagnostics'
 Check ((Get-Content blank.log -Raw) -match '\[error\]') 'no-cup input still fails explicitly'
 Run 'bad-config' @('--ransac-iterations=-1') 2
 Check ((Get-Content bad-config.log -Raw) -match 'invalid NormalizeParams') 'invalid recovery configuration is rejected'
 Write-Output ('Cup recovery checks passed: '+$script:checks)
}finally{Pop-Location}
