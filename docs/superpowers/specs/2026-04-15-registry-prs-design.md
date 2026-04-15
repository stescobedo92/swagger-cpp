# Registry PRs Design — swaggercpp v0.2.1

**Date:** 2026-04-15  
**Author:** Sergio Triana Escobedo  
**Scope:** Submit swaggercpp to ConanCenter (conan-io/conan-center-index) and vcpkg official (microsoft/vcpkg) via PRs from existing forks.

---

## Context

- Repo: `https://github.com/stescobedo92/swagger-cpp`
- Version: `0.2.1` (tag `v0.2.1`)
- Tarball SHA256: `2b5f40fbc686673a69fe4b39570fc18a2f37cbee2dbd7a9c5980d67f38ccdaa1`
- Tarball SHA512: `8258929fd1d281b4d44395794fb0db4735e59fa50af085329be96642ac15d76231769363e49ae974001fa3843c6152c144f2598537eeeaa50252bd496ab3c01d`
- Forks: `stescobedo92/conan-center-index`, `stescobedo92/vcpkg`
- git user: Sergio Triana Escobedo <stescobedo.31@gmail.com>

---

## PR 1 — ConanCenter (`conan-io/conan-center-index`)

### Branch
`add-swaggercpp-0.2.1` on fork `stescobedo92/conan-center-index`

### Files to create

#### `recipes/swaggercpp/config.yml`
Maps version `0.2.1` to folder `all/`.

#### `recipes/swaggercpp/all/conandata.yml`
Declares tarball URL and SHA256 for version `0.2.1`.

#### `recipes/swaggercpp/all/conanfile.py`
Adapted from project's root `conanfile.py` with the following changes vs. the original:
- Remove `exports_sources` (CCI downloads sources, never bundles them)
- Remove `author` field (not allowed in CCI)
- Set `url = "https://github.com/conan-io/conan-center-index"` (CCI policy)
- Add `homepage = "https://github.com/stescobedo92/swagger-cpp"`
- Remove `default_options` for transitive deps (`yaml-cpp/*:shared`) — CCI policy
- Add `source()` method using `conan.tools.files.get()` reading URL/SHA256 from `conandata.yml`
- Use `conan.tools.files.apply_conandata_patches()` stub (future proofing)

### PR target
`conan-io/conan-center-index` master ← `stescobedo92/conan-center-index` `add-swaggercpp-0.2.1`

---

## PR 2 — vcpkg official (`microsoft/vcpkg`)

### Branch
`add-swaggercpp-0.2.1` on fork `stescobedo92/vcpkg`

### Files to create/modify

#### `ports/swaggercpp/portfile.cmake`
Rewritten to use `vcpkg_from_github` with:
- `REPO stescobedo92/swagger-cpp`
- `REF v0.2.1`
- `SHA512` of tarball
- `HEAD_REF master`
- Keep existing cmake configure/install/fixup steps
- Replace manual `file(INSTALL ... LICENSE)` with `vcpkg_install_copyright()`

#### `ports/swaggercpp/vcpkg.json`
Clean version for official registry:
- `"version": "0.2.1"` (semver)
- Add `"homepage"` field
- Dependencies: `cpp-httplib`, `nlohmann-json`, `yaml-cpp`, `vcpkg-cmake` (host), `vcpkg-cmake-config` (host)

#### `ports/swaggercpp/usage`
Short CMake usage instructions shown after `vcpkg install swaggercpp`.

#### `versions/s-/swaggercpp.json` (new)
Version database for this port. The `git-tree` field is computed after the port commit with `git rev-parse HEAD:ports/swaggercpp`.

#### `versions/baseline.json` (modify)
Insert `swaggercpp` entry alphabetically between existing `swagger-ui` and `swiftshader` entries.

### PR target
`microsoft/vcpkg` master ← `stescobedo92/vcpkg` `add-swaggercpp-0.2.1`

---

## Execution sequence

1. Clone fork `stescobedo92/conan-center-index` (shallow `--depth=1`)
2. Create branch, write 3 files, commit, push, open PR to `conan-io/conan-center-index`
3. Clone fork `stescobedo92/vcpkg` (shallow `--depth=1`)
4. Create branch, write port files, commit
5. Capture `git rev-parse HEAD:ports/swaggercpp` → tree SHA
6. Write `versions/s-/swaggercpp.json` with that tree SHA
7. Patch `versions/baseline.json` alphabetically
8. Commit, push, open PR to `microsoft/vcpkg`
