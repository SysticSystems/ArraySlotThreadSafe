# Systic C++ Skeleton

A starting point for C++23 projects where build reproducibility and honest benchmark numbers matter. Clone it, rename things, write code.

---

## What this is ?

A template repository that wires up the build toolchain, test framework, and benchmark harness so you don't have to. It gives you a working project structure, a containerised build pipeline, and a CI workflow that publishes real test and benchmark results — nothing more.

It does not impose an architecture, a logging strategy, a memory allocator, or any application-level opinions. Those are your problem.

---

## Toolchain constraints

Using this skeleton ties you to the following:

| Concern | Choice |
|---|---|
| Language standard | C++23 (enforced by Conan) |
| Compiler | Clang 21 |
| Dependency manager | Conan 2.0 |
| Build system | CMake + Ninja (system-provided via Alpine) |
| Unit / integration tests | GoogleTest 1.14.0 |
| Benchmarks | Google Benchmark 1.8.3 |
| Build container base | Alpine Edge (musl libc) |

Changing the compiler or standard means updating both the Conan profile under `.conan/profiles/` and the `configure()` block in `conanfile.py`. Everything else is replaceable.

---

## Project layout

```
include/    public headers
src/        implementation
tests/      GTest suites
bench/      Google Benchmark cases
.conan/     Conan hardware profiles
.github/    CI workflow definitions
```

---

## Building locally

Docker is required. The `run-build.sh` script manages the container lifecycle and maps build artifacts back to the host.

```sh
./run-build.sh Dev        # build + compile-db for IDE indexing
./run-build.sh Testing    # build, run tests, run benchmarks
./run-build.sh Release    # optimised build
```

If you want to build directly on the host, install Conan 2, CMake, Ninja, and Clang 21, then run `build.sh`.

---

## CI: test and benchmark results

The GitHub Actions workflow runs on a **bare-metal self-hosted runner**. This is intentional — shared cloud runners introduce scheduling jitter and variable CPU states that make benchmark numbers meaningless. Results from a real machine are surfaced directly in the Actions summary of each run: test pass/fail counts and benchmark timings are posted there so regressions are visible without digging through logs.

> If you fork this and use GitHub-hosted runners, benchmark numbers will not be comparable across runs. Use them only as smoke tests until you wire up a bare-metal runner of your own.

---
# Getting Started

## System Requirements

- **Docker** (any recent version with BuildKit support)
- **~2 GB free disk space** for the Alpine Edge image and Conan/build cache

## Setup

1. Copy the environment file and fill in your values:
   ```sh
   cp .example.env .env
   ```

2. Edit `conanfile.py` — at minimum update the `name` and `version` fields to match your project.

3. Review or replace the Conan profile under `.conan/profiles/`. The default targets a specific arch and compiler path — adjust it to match your machine. If you add a new profile file, update `build.sh` accordingly so it picks up the right profile name.

## Run

```sh
./run-build.sh Dev        # build + compile-db for IDE indexing
./run-build.sh Testing    # build, run tests and benchmarks
./run-build.sh Release    # optimised build
```

Artifacts are mapped back to your host by the script on completion.