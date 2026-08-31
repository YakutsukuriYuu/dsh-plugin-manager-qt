# dsh-plugin-manager

[DSH](https://www.npmjs.com/package/@deepseek-ai/dsh)（DeepSeek Harness）插件的图形化管理工具。

管理本机与远程服务器上的 DSH 插件、tmux 会话，支持一键同步插件到服务器，无需记忆命令行。

## 下载安装

在 [Releases](https://github.com/YakutsukuriYuu/dsh-plugin-manager-qt/releases/latest) 下载 `dsh-plugin-manager-macos-arm64-<版本号>.dmg`（macOS Apple Silicon）。

> macOS 会报「已损坏，无法打开」——这是未签名应用的正常拦截，执行一条命令即可：
>
> ```bash
> xattr -cr "/Applications/DSH Plugin Manager.app"
> ```

## 功能

### 📦 插件管理

- 浏览当前 Profile 下所有 DSH 插件
- 一键启用/禁用（Switch 开关）
- 安装新插件（输入 npm 包名）
- 卸载插件（自动识别安装方式，符号链接插件也能正常卸载）
- 搜索过滤、Profile 切换
- 聚合包的子依赖默认折叠，界面干净

### 🌐 远程服务器（SSH）

通过 SSH 管理远程服务器上的 DSH 插件：

1. **添加服务器**：侧边栏点「远程服务器」→「添加服务器」
   - 输入备注名 + SSH 地址（如 `user@host` 或 `~/.ssh/config` 别名）
   - 支持自定义端口（可直接粘贴 `user@host -p 6005`，端口自动识别）
   - 认证方式：**密钥认证**（需 `ssh-copy-id` 免密）或**密码认证**（可选记住密码）
2. **连接**：点「连接」→ 等待动画 → 自动切换到远程模式
3. **操作**：插件列表自动切为远程数据，所有操作（启停/安装/卸载）对远程生效
4. **断开**：点「断开，回本机」切换回本地

### 🛠️ 服务器 DSH 服务管理

连接远程服务器后，自动出现「DSH 服务」卡片：

- **版本检查**：自动显示服务器已装版本与 npm 最新版本，有更新时高亮提示
- **一键升级**：执行 `npm install -g` 升级到最新版（自动定位 npm，支持 nvm 安装）
- **一键重启**：自动识别运行方式——tmux 会话（原地重启）、systemd 用户服务、
  后台裸进程（kill 后按原命令重新拉起），重启后自动验证进程存活
- 所有操作实时显示日志，不卡界面

### 🔄 同步插件到服务器

远程连接后，插件管理页出现「同步到服务器」按钮：

1. 打开同步对话框，显示本地插件清单（本地 vs 远程版本对照）
2. 全选或手动勾选要同步的插件
3. 点「开始同步」→ 进度条 + 实时日志
4. 版本以本地为准；聚合包（如 `dsh-web-all`）同步时自动带上子依赖

### 🖥️ 终端会话（tmux）

管理 tmux 中运行的 DSH 进程：

- 查看所有 tmux 会话状态（DSH 会话自动识别并标记徽章）
- 查看输出（最近 100 行日志）
- 附加到终端（一键打开 Terminal.app）
- 重启 DSH（Ctrl-C → 重新执行 `dsh web`）
- 未检测到 DSH 时显示「启动 DSH」横幅

### ⬆️ 自动更新检查

启动后自动检查 GitHub Releases，有新版本时侧边栏「设置」项出现红点提醒。

## 许可证

MIT
