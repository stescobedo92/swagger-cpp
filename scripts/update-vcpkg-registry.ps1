$ErrorActionPreference = "Stop"

$portTree = git rev-parse HEAD:packaging/vcpkg/registry/ports/swaggercpp
$portManifest = Get-Content "$PSScriptRoot\..\packaging\vcpkg\registry\ports\swaggercpp\vcpkg.json" | ConvertFrom-Json
$version = $portManifest.'version-semver'

$json = @"
{
  "versions": [
    {
      "version-semver": "$version",
      "git-tree": "$portTree"
    }
  ]
}
"@

Set-Content -Path "$PSScriptRoot\..\packaging\vcpkg\registry\versions\s-\swaggercpp.json" -Value $json -NoNewline
