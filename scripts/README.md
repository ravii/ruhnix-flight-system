# scripts

构建、烧录、部署脚本。

| 脚本 | 在哪里执行 | 作用 |
|------|------------|------|
| `build_all.sh` | WSL / 容器 | 构建并测试所有模块（或指定模块） |
| `format.sh` | 任意 | clang-format 格式化 / `--check` 检查 |
| `wsl.sh` | Windows（Git Bash） | 在 WSL 里对**当前这份**代码执行命令，例如 `scripts/wsl.sh scripts/build_all.sh simulator`；不带参数时进入 WSL shell |
| `docker.sh` | WSL / Windows | 构建 CI 镜像、在容器里执行命令 |
| `confluence_sync.py` | 任意（需要 Python + `markdown` 包） | 把设计文档同步到 Confluence（`--dry-run` 只转换、不联网）。凭据放在本机的 `~/.config/uav-flight/confluence.env`，**不要放进仓库**，格式见脚本开头的注释 |
| `claude-format-hook.sh` | Claude Code hook | Claude 改完 C/C++ 文件后自动格式化 |
| `git-hooks/pre-commit` | git | 提交前检查格式（`git config core.hooksPath scripts/git-hooks`） |
