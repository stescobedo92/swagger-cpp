[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string]$RegistryDirectory,
    [Parameter(Mandatory)] [string]$VcpkgRoot,
    [string]$Triplet = "x64-windows",
    [switch]$Tools,
    [string]$ConsumerDirectory = "$PSScriptRoot/../build/registry-consumer"
)

$ErrorActionPreference = "Stop"
$repoRoot = [IO.Path]::GetFullPath("$PSScriptRoot/..")
$registryPath = (Resolve-Path -LiteralPath $RegistryDirectory).Path
$consumerPath = [IO.Path]::GetFullPath($ConsumerDirectory)
if ($consumerPath -eq $repoRoot -or $repoRoot.StartsWith($consumerPath.TrimEnd('/', '\') + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw "ConsumerDirectory must not be the repository root or its ancestor."
}
$vcpkgPath = Join-Path $VcpkgRoot "vcpkg.exe"
if (-not (Test-Path -LiteralPath $vcpkgPath)) { $vcpkgPath = Join-Path $VcpkgRoot "vcpkg" }
$projectManifest = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "vcpkg.json") | ConvertFrom-Json
$portManifest = Get-Content -Raw -LiteralPath (Join-Path $registryPath "ports/swaggercpp/vcpkg.json") | ConvertFrom-Json
$dependency = @{ name = "swaggercpp"; 'version>=' = $portManifest.version }
if ($Tools) { $dependency.features = @("tools") }
$manifest = @{ name = "swaggercpp-registry-consumer"; version = "1.0.0"; dependencies = @($dependency) }
$configuration = @{
    'default-registry' = @{ kind = "builtin"; baseline = $projectManifest.'builtin-baseline' }
    registries = @(@{ kind = "filesystem"; path = $registryPath; baseline = "default"; packages = @("swaggercpp") })
}
$utf8 = [Text.UTF8Encoding]::new($false)
New-Item -ItemType Directory -Force -Path $consumerPath | Out-Null
[IO.File]::WriteAllText((Join-Path $consumerPath "vcpkg.json"), ($manifest | ConvertTo-Json -Depth 5) + "`n", $utf8)
[IO.File]::WriteAllText((Join-Path $consumerPath "vcpkg-configuration.json"), ($configuration | ConvertTo-Json -Depth 5) + "`n", $utf8)
$cmake = @'
cmake_minimum_required(VERSION 3.26)
project(swaggercpp_registry_consumer LANGUAGES CXX)
find_package(swaggercpp CONFIG REQUIRED)
add_executable(registry-consumer example.cpp)
target_link_libraries(registry-consumer PRIVATE swaggercpp::swaggercpp)
target_compile_features(registry-consumer PRIVATE cxx_std_23)
enable_testing()
add_test(NAME registry-consumer COMMAND registry-consumer)
'@
[IO.File]::WriteAllText((Join-Path $consumerPath "CMakeLists.txt"), $cmake + "`n", $utf8)
Copy-Item -LiteralPath (Join-Path $repoRoot "packaging/conan/test_package/src/example.cpp") -Destination (Join-Path $consumerPath "example.cpp")

$installPath = Join-Path $consumerPath "vcpkg_installed"
& $vcpkgPath install "--triplet=$Triplet" "--x-manifest-root=$consumerPath" "--x-install-root=$installPath"
if ($LASTEXITCODE -ne 0) { throw "Registry package installation failed ($LASTEXITCODE)." }

$buildPath = Join-Path $consumerPath "build"
& cmake -S $consumerPath -B $buildPath -G Ninja "-DCMAKE_BUILD_TYPE=Release" "-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot/scripts/buildsystems/vcpkg.cmake" "-DVCPKG_TARGET_TRIPLET=$Triplet" "-DVCPKG_INSTALLED_DIR=$installPath"
if ($LASTEXITCODE -ne 0) { throw "Registry consumer configuration failed ($LASTEXITCODE)." }
& cmake --build $buildPath --parallel
if ($LASTEXITCODE -ne 0) { throw "Registry consumer build failed ($LASTEXITCODE)." }
& ctest --test-dir $buildPath --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Registry consumer test failed ($LASTEXITCODE)." }

if ($Tools) {
    $cliPath = Join-Path $installPath "$Triplet/tools/swaggercpp/swaggercpp-cli.exe"
    if (-not (Test-Path -LiteralPath $cliPath)) { $cliPath = Join-Path $installPath "$Triplet/tools/swaggercpp/swaggercpp-cli" }
    $cliVersion = & $cliPath --version
    if ($LASTEXITCODE -ne 0 -or ($cliVersion -join "`n").Trim() -ne $portManifest.version) {
        throw "Installed CLI version check failed: $cliVersion"
    }
    Write-Host $cliVersion
}
