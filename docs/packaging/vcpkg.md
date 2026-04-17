# vcpkg

`swaggercpp` ships in the official [microsoft/vcpkg](https://github.com/microsoft/vcpkg/tree/master/ports/swaggercpp) registry. Consumers need a working vcpkg installation and a CMake toolchain file wired in.

::: info
`vcpkg.io`'s package browser may take 1–2 weeks to index freshly merged ports. The [GitHub path](https://github.com/microsoft/vcpkg/tree/master/ports/swaggercpp) is always the authoritative source.
:::

## One-time setup

```powershell
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
setx VCPKG_ROOT C:\vcpkg
```

```bash
git clone https://github.com/microsoft/vcpkg ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg
```

Add to `CMakePresets.json` (or `-DCMAKE_TOOLCHAIN_FILE=...`):

```json
"cacheVariables": {
  "CMAKE_TOOLCHAIN_FILE": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
}
```

## Manifest mode (recommended)

Put a `vcpkg.json` next to `CMakeLists.txt`:

```json
{
  "name": "my-app",
  "version": "0.1.0",
  "dependencies": [ "swaggercpp" ],
  "builtin-baseline": "<sha-from-vcpkg>"
}
```

Get a baseline SHA that contains `swaggercpp`:

```powershell
git -C $env:VCPKG_ROOT rev-parse HEAD
```

On first `cmake --configure`, vcpkg installs `swaggercpp` and its transitive deps into `out/build/<preset>/vcpkg_installed/<triplet>/`.

## Classic mode

```powershell
vcpkg install swaggercpp
```

Installs into `$VCPKG_ROOT/installed/<triplet>/`. Works for projects **without** a `vcpkg.json`.

::: warning Do not mix modes
If a `vcpkg.json` exists in your project, manifest mode is active and the classic install is ignored. You must declare `swaggercpp` in `dependencies` or `find_package` will fail silently.
:::

## Consuming in CMake

```cmake
find_package(swaggercpp CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE swaggercpp::swaggercpp)
target_compile_features(my_app PRIVATE cxx_std_23)
```

## Troubleshooting

### `error: swaggercpp does not exist`

Your local vcpkg clone is missing the port files. Check `git status` inside `$VCPKG_ROOT` and restore any deleted files:

```powershell
git -C $env:VCPKG_ROOT restore ports/swaggercpp/
```

Then `git pull` to make sure you're on a commit that contains the port.

### `Could not find package configuration file`

Your project has `vcpkg.json` but does not declare `swaggercpp`. Add it to `dependencies` and reconfigure.

### Version 0.1.0 keeps installing

You have a stale classic install cached. Remove it and reinstall:

```powershell
vcpkg remove swaggercpp
vcpkg install swaggercpp
```

## Overlay ports (for contributors)

If you want to test local changes to `packaging/vcpkg/ports/swaggercpp` before opening a PR to upstream vcpkg:

```powershell
vcpkg install swaggercpp --overlay-ports=packaging\vcpkg\ports --classic
```

## See also

- [Installation](/install)
- [Custom registry](/packaging/cmake-package)
- Official port: https://github.com/microsoft/vcpkg/tree/master/ports/swaggercpp
