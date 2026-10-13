# Release registry template

Run `scripts/update-vcpkg-registry.ps1` from a published source commit to generate
a portable filesystem registry in `build/vcpkg-registry`. The generator verifies
the source version, downloads the immutable commit archive, and records its
SHA512. The release workflow validates a consumer before publishing the ZIP.

Each release bundle contains exactly that release's version. Extract the ZIP and
configure a `filesystem` registry pointing at the extracted directory, with
`baseline: "default"` and `packages: ["swaggercpp"]`.

The previous nested Git registry layout was not consumable by vcpkg. Its files
remain in Git history; they are not migrated into the generated registry.
