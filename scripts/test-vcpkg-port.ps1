$ErrorActionPreference = "Stop"

$vcpkg = "C:\Users\stesc\cpp-packages\vcpkg\vcpkg.exe"
& $vcpkg install swaggercpp --overlay-ports="$PSScriptRoot\..\packaging\vcpkg\ports" --classic
