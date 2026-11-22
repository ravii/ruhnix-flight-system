# Ruhnix Flight System (RFS)

Ruhnix Flight System (RFS). Monorepo of independently built modules: `common/` (C++17 protocol library), `flight-controller/` (C++17 on FreeRTOS), `perception/` and `localization/` (C++17 on Linux), `simulator/` (C++17, host). Each module has its own CMakeLists.txt; there is no top-level build. ESCs are off-the-shelf (BLHeli_32 / AM32 …) driven by DShot from the flight controller; there is no ESC firmware in this repo.

## Namespaces (required)

- All C++ code lives in exactly one namespace per module: `rfs::common`, `rfs::fc` (flight-controller, including app/, bsp/, tests helpers), `rfs::sim`, `rfs::perception`, `rfs::localization`. No deeper namespaces (no `rfs::fc::esc`); disambiguate with name prefixes instead (`EscProtocol`, `DshotCommand`, `dshot_encode`). Anonymous namespaces for file-local code are fine.
- Public headers live under `include/rfs/<module>/` and are included as `"rfs/<module>/..."`.
- Inside a module's namespace, refer to the shared protocol as `common::` (e.g. `common::SimSensorsMsg`).

## Formatting (required)

- Every C/C++ change must be clang-formatted with the repo `.clang-format` (devkits style, clang-format 20.1.8).
- A PostToolUse hook formats files edited with Edit/Write automatically. Files created or changed through Bash (sed, python, heredocs) bypass it: run `scripts/format.sh <files>` afterwards.
- Before committing, `scripts/format.sh --check` must pass (CI enforces it).

## Build and test (WSL2 / Docker, Ubuntu 26.04)

Development happens in WSL2: the primary clone is `~/ruhnix-flight-system` on the WSL filesystem, opened with VS Code Remote-WSL. Editing, building, testing, debugging and git all happen there. The Docker image is the CI / clean-room environment. Both get their toolchain from `docker/setup_ubuntu.sh` — add new tools there, never only in one place. See `docs/dev-environment.md`. (`scripts/wsl.sh` is a fallback that builds a Windows-drive checkout from Git Bash; it is not the normal workflow.)

```bash
scripts/build.sh                              # host Debug build of all modules (no chip toolchains); add module names to limit
scripts/build.sh -t                           # ... and run the unit tests
scripts/build.sh --arm64 -t                   # perception/localization for ARM64, tests under qemu
scripts/build.sh --fw <board>                 # flight-controller firmware (arm-none-eabi, bsp/<board>)
scripts/build.sh --sitl                       # flight-controller firmware as a Linux process (FreeRTOS POSIX port, bsp/sitl)
scripts/sitl.sh --check                       # SITL firmware vs simulator over UDP; trace must equal the in-process loop
scripts/build_all.sh                          # = build.sh --host --sitl --test (incl. sitl.sh --check); what CI runs
scripts/format.sh --check
scripts/docker.sh run scripts/build_all.sh    # same check in the CI image
scripts/docker.sh build                       # if Docker Hub is blocked:
                                              # RFS_BASE_IMAGE=public.ecr.aws/docker/library/ubuntu:26.04
```

Build directories are `build/<host|arm64|sitl|fw-BOARD>/<module>`. FreeRTOS-Kernel is a submodule (`git submodule update --init`). `build.sh` uses GCC 15 with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wdouble-promotion -Wundef -Werror`, the same as CI. Do not validate with MSVC on the Windows side.

## Docs

- Confluence pages draw architecture diagrams with the PlantUML plugin, never as ASCII art. ASCII diagrams are only for the Markdown docs in the repo.
- PlantUML diagrams: no `:` in element names or labels (the Confluence app fails to render them; write `Board`, not `rfs::fc::Board`). Name the project's own modules in Chinese (`感知`, `飞控算法`, `消息总线`); keep established industry terms as they are (IMU, GNSS, UART, DShot, FreeRTOS, MCAP). A second line may carry the code name (`飞控算法\nFlightControl`).
- Diagrams in repo Markdown are pure-ASCII flowcharts (`+--+`, `|`, `-->`, `v`, `^`). Non-ASCII text (Chinese) only at the end of a line, never left of a box edge or arrow; CJK and box-drawing glyphs have font-dependent widths and break alignment. Directory trees may keep `├──`.
- Confluence is the master for the design docs listed in `scripts/confluence_sync.py` and `docs/confluence_pages.json`, and for new design docs. Write and change them in Confluence first; the user reviews and edits the page there. Only after the review is done and the code is implemented, store the idempotent Markdown copy in the repo with `python scripts/confluence_sync.py pull` (diagrams as ASCII). Never edit the repo copy first and push it up.
- Credentials live only in `~/.confluence/confluence.env` — never put tokens in the repo.

## Firmware rules

- `core/` must not include FreeRTOS or hardware headers; it is driven by the simulator and unit tests.
- Motor output stays protocol-agnostic: control code produces `ActuatorOutputs` in [0, 1]; ESC protocol, wiring order and encoding live in `config/esc_config.hpp` + `core/.../esc/` (`EscOutput`), and boards only implement the waveform (`rfs::fc::EscBus`).
- Hardware access only through `rfs::fc::Board`. The control loop is paced by `Board::wait_imu()`, and time only comes from `Board::time_us()` (simulation time in SITL).
- No exceptions, no RTTI, no heap after the scheduler starts; no `std::vector`, `std::string`, `std::function`, `iostream` in firmware.
- No side effects inside `configASSERT()`.
- The RTOS tick is 50 Hz (20 ms). Convert timeouts with `rfs::fc::timeout_ticks()` (`rfs/fc/bsp/rtos_time.hpp`), never `pdMS_TO_TICKS()` (rounds 10 ms down to 0). Task periods must be whole ticks.
- SITL (`bsp/sitl`): tasks must not block in syscalls (a blocked pthread still "runs" for FreeRTOS); I/O waits go through the idle hook + task notifications, see `udp_sim_backend.hpp`.
- Protocol structs are packed: copy element-wise, never bind references or pointers to their members.
