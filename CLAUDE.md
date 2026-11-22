# uav-flight

Monorepo of independently built modules: `common/` (C protocol headers), `flight-controller/` and `esc/` (C++17 on FreeRTOS), `perception/` and `localization/` (C++17 on Linux), `simulator/` (C++17, host). Each module has its own CMakeLists.txt; there is no top-level build.

## Formatting (required)

- Every C/C++ change must be clang-formatted with the repo `.clang-format` (devkits style, clang-format 20.1.8).
- A PostToolUse hook formats files edited with Edit/Write automatically. Files created or changed through Bash (sed, python, heredocs) bypass it: run `scripts/format.sh <files>` afterwards.
- Before committing, `scripts/format.sh --check` must pass (CI enforces it).

## Build and test (WSL2 / Docker, Ubuntu 26.04)

Code is edited on Windows (this checkout, VS Code navigation only); building, testing and debugging happen in WSL2 on the **same checkout** via `/mnt/<drive>/...` — there is no second clone to sync. From Git Bash, prefix any command with `scripts/wsl.sh` (build output goes to `~/uav-build` inside WSL). The Docker image is the CI / clean-room environment. Both get their toolchain from `docker/setup_ubuntu.sh` — add new tools there, never only in one place. See `docs/dev-environment.md`.

```bash
scripts/wsl.sh scripts/build_all.sh           # from Windows: build + test in WSL on this checkout
scripts/build_all.sh                          # inside WSL: all modules, or: scripts/build_all.sh esc simulator
scripts/format.sh --check
scripts/docker.sh run scripts/build_all.sh    # same check in the CI image
scripts/docker.sh build                       # if Docker Hub is blocked:
                                              # UAV_BASE_IMAGE=public.ecr.aws/docker/library/ubuntu:26.04
```

`build_all.sh` uses GCC 15 with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wdouble-promotion -Wundef -Werror`, the same as CI. Do not validate with MSVC on the Windows side.

## Firmware rules

- `core/` must not include FreeRTOS or hardware headers; it is driven by the simulator and unit tests.
- Hardware access only through `fc::Board` / `esc::Board`. The control loop is paced by `Board::wait_imu()`, and time only comes from `Board::time_us()` (simulation time in SITL).
- No exceptions, no RTTI, no heap after the scheduler starts; no `std::vector`, `std::string`, `std::function`, `iostream` in firmware.
- No side effects inside `configASSERT()`.
- Protocol structs are packed: copy element-wise, never bind references or pointers to their members.
