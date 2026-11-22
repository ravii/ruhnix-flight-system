# scripts

构建、烧录、部署脚本。

| 脚本 | 在哪里执行 | 作用 |
|------|------------|------|
| `build.sh` | WSL / 容器 | 构建模块。不加选项时用主机 gcc 以 Debug 构建所有模块，供开发调试用，不调用芯片工具链；`-t` 测试，`--arm64` / `--fw <board>` 交叉编译，`--sitl` 把飞控固件编译成 Linux 程序，`-h` 查看全部选项 |
| `build_all.sh` | WSL / 容器 / CI | 等同于 `build.sh --host --sitl --test`：构建并测试所有模块（或指定模块），再做 SITL 锁步检查 |
| `sitl.sh` | WSL / 容器 | SITL：启动 FreeRTOS 固件（Linux 进程），用 `rfs_sim --udp` 和它锁步飞一段，轨迹写到 `build/sitl/trace.csv`；`--check` 要求和进程内算法闭环的轨迹逐字节一致 |
| `format.sh` | 任意 | clang-format 格式化 / `--check` 检查 |
| `wsl.sh` | Windows（Git Bash） | 备选方案：在 WSL 里对 Windows 盘上的这份代码执行命令，例如 `scripts/wsl.sh scripts/build_all.sh simulator`；不带参数时进入 WSL shell。日常开发直接在 WSL 的 `~/ruhnix-flight-system` 里进行，用不到它 |
| `docker.sh` | WSL / Windows | 构建 CI 镜像、在容器里执行命令 |
| `confluence_sync.py` | 任意（需要 Python + `markdown` 包） | 把设计文档同步到 Confluence（`--dry-run` 只转换、不联网）。凭据放在本机的 `~/.confluence/confluence.env`，**不要放进仓库**，格式见脚本开头的注释 |
| `claude-format-hook.sh` | Claude Code hook | Claude 改完 C/C++ 文件后自动格式化 |
| `git-hooks/pre-commit` | git | 提交前检查格式（`git config core.hooksPath scripts/git-hooks`） |
