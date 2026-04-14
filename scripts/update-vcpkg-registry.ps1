$ErrorActionPreference = "Stop"

$portTree = git rev-parse HEAD:packaging/vcpkg/registry/ports/swaggercpp

$json = @"
{
  "versions": [
    {
      "version-semver": "0.1.0",
      "git-tree": "$portTree"
    }
  ]
}
"@

Set-Content -Path "$PSScriptRoot\..\packaging\vcpkg\registry\versions\s-\swaggercpp.json" -Value $json -NoNewline
