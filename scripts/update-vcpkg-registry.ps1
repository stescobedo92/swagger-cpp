[CmdletBinding()]
param(
    [string]$SourceRef = "HEAD",
    [ValidatePattern('^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$')]
    [string]$SourceRepository = "stescobedo92/swagger-cpp",
    [string]$OutputDirectory = "$PSScriptRoot/../build/vcpkg-registry"
)

$ErrorActionPreference = "Stop"
$repoRoot = [IO.Path]::GetFullPath("$PSScriptRoot/..")
$sourceCommit = git -C $repoRoot rev-parse --verify "$SourceRef^{commit}"
if ($LASTEXITCODE -ne 0 -or $sourceCommit -notmatch '^[0-9a-f]{40}$') {
    throw "SourceRef must resolve to a commit in this repository."
}

$manifestPath = Join-Path $repoRoot "packaging/vcpkg/ports/swaggercpp/vcpkg.json"
$manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
$version = $manifest.version
$sourceManifestText = git -C $repoRoot show "${sourceCommit}:vcpkg.json"
if ($LASTEXITCODE -ne 0) { throw "Cannot read the source manifest at $sourceCommit." }
$sourceManifest = ($sourceManifestText -join "`n") | ConvertFrom-Json
if ($version -ne $sourceManifest.'version-string') {
    throw "Port version $version does not match source version $($sourceManifest.'version-string')."
}

$outputPath = [IO.Path]::GetFullPath($OutputDirectory)
if ($outputPath -eq $repoRoot -or $repoRoot.StartsWith($outputPath.TrimEnd('/', '\') + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw "OutputDirectory must not be the repository root or its ancestor."
}
$portPath = Join-Path $outputPath "ports/swaggercpp"
$versionsPath = Join-Path $outputPath "versions/s-"
New-Item -ItemType Directory -Force -Path $portPath, $versionsPath | Out-Null

$archivePath = Join-Path ([IO.Path]::GetTempPath()) ("swaggercpp-" + [guid]::NewGuid().ToString("N") + ".tar.gz")
try {
    $archiveUrl = "https://github.com/$SourceRepository/archive/$sourceCommit.tar.gz"
    Invoke-WebRequest -Uri $archiveUrl -OutFile $archivePath
    $sha512 = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA512).Hash.ToLowerInvariant()
} finally {
    if (Test-Path -LiteralPath $archivePath) {
        Remove-Item -LiteralPath $archivePath -Force
    }
}

$template = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "packaging/vcpkg/registry/portfile.cmake.in")
$portfile = $template.Replace('@SOURCE_REPOSITORY@', $SourceRepository).Replace('@SOURCE_REF@', $sourceCommit).Replace('@SOURCE_SHA512@', $sha512)
$utf8 = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText((Join-Path $portPath "portfile.cmake"), $portfile, $utf8)
Copy-Item -LiteralPath $manifestPath -Destination (Join-Path $portPath "vcpkg.json")

$baseline = @{ default = @{ swaggercpp = @{ baseline = $version; 'port-version' = 0 } } }
$versions = @{ versions = @(@{ version = $version; 'port-version' = 0; path = '$/ports/swaggercpp' }) }
[IO.File]::WriteAllText((Join-Path $outputPath "versions/baseline.json"), ($baseline | ConvertTo-Json -Depth 5) + "`n", $utf8)
[IO.File]::WriteAllText((Join-Path $versionsPath "swaggercpp.json"), ($versions | ConvertTo-Json -Depth 5) + "`n", $utf8)
Write-Host "Generated filesystem registry for swaggercpp $version ($sourceCommit) at $outputPath"
