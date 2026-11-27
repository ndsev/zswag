# CI/CD and Release Process

Maintainer-facing notes on how zswag is built, tested, and released. End users do not need to read this.

## CI/CD

The project uses GitHub Actions for automated build, test, and deploy:

- **Wheel builds:** One `cp310-abi3` wheel per target: Linux (x86_64 and aarch64), macOS (Intel x86_64 and Apple Silicon arm64), and Windows (x64). CI builds these five wheels with CPython 3.11.
- **Python compatibility:** CI installs the same repaired Linux x86_64 wheel on CPython 3.10 through 3.14 and runs the binding smoke test. Pip supplies the interpreter-specific `nanobind-backend` dependency.
- **Java toolchain:** Temurin 17 (auto-provisioned by Gradle via the Foojay resolver — see `build.gradle` and `settings.gradle`).
- **Wheel workflow triggers:** pull requests, pushes to `main` or `master`, version tags, and manual snapshot builds.

Three top-level workflows:

| Workflow | What |
|---|---|
| `build-deploy.yml` | Builds and tests the wheels. PRs do not publish. Version tags and default-branch pushes publish to PyPI; manual snapshots attach wheels to a GitHub prerelease instead. |
| `coverage.yml` | C++ code coverage (lcov / SonarCloud / Codecov). |
| `jzswag.yml` | Java build, JaCoCo coverage, Codecov upload (Linux + macOS). |

## Release process

1. Update the version in `CMakeLists.txt` (and the matching version in root `build.gradle`).
2. Tag the commit: `git tag v{version}` (e.g. `v1.14.0`), then `git push origin v{version}`.
3. CI validates that the tag version matches the CMake version and deploys wheels to PyPI.

## Development snapshots

Pushes to `main` can publish a development version such as `1.14.0.dev3` to PyPI without a tag.
To test a particular branch or commit without publishing to PyPI, manually run
`Build and Deploy` with that ref. The resulting GitHub prerelease lists an
install command for each wheel; choose the one for your OS and CPU architecture.
