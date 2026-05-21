# CI/CD and Release Process

Maintainer-facing notes on how zswag is built, tested, and released. End users do not need to read this.

## CI/CD

The project uses GitHub Actions for automated build, test, and deploy:

- **Platforms:** Linux (x86_64), macOS (Intel x86_64 and Apple Silicon arm64), Windows (x64).
- **Python versions:** 3.10, 3.11, 3.12, 3.13.
- **Java toolchain:** Temurin 17 (auto-provisioned by Gradle via the Foojay resolver — see `build.gradle` and `settings.gradle`).
- **Triggers:** pull requests, pushes to `main`, version tags.

Three top-level workflows:

| Workflow | What |
|---|---|
| `build-deploy.yml` | C++ + Python wheels; PyPI deploy on tagged releases. |
| `coverage.yml` | C++ code coverage (lcov / SonarCloud / Codecov). |
| `jzswag.yml` | Java build, JaCoCo coverage, Codecov upload (Linux + macOS). |

## Release process

1. Update the version in `CMakeLists.txt` (and the matching version in root `build.gradle`).
2. Tag the commit: `git tag v{version}` (e.g. `v1.11.1`), then `git push origin v{version}`.
3. CI validates that the tag version matches the CMake version and deploys wheels to PyPI.

## Development snapshots

Pushes to `main` create development releases — version format `{base_version}.dev{commit_count}` (e.g. `1.11.1.dev3`) — automatically deployed to PyPI for testing. No tag required.
