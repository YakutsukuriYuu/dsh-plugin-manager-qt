# dsh-plugin-manager-qt

基于 **Qt 6 / QML** 的 [DSH](https://www.npmjs.com/package/@deepseek-ai/dsh)（DeepSeek Harness）插件管理桌面应用。

为 DSH 提供一个图形化的插件与运行环境管理工具：浏览/安装/启停插件、管理 tmux 中的 DSH 会话，无需记忆命令行操作。

## ⬇️ 下载安装

在 [Releases](https://github.com/YakutsukuriYuu/dsh-plugin-manager-qt/releases) 页面下载最新的 `dsh-plugin-manager-macos-arm64.dmg`（macOS Apple Silicon）。

> **⚠️ 首次打开必看**：应用未购买 Apple 开发者签名（$99/年），从浏览器下载后 macOS 会报「已损坏，无法打开」。**这不是真的损坏**，是 Gatekeeper 对网络下载应用的隔离拦截。把应用拖进「应用程序」文件夹后执行：
>
> ```bash
> xattr -cr /Applications/dsh-plugin-manager.app
> ```
>
> 之后即可正常双击打开。

## 🚀 发版（维护者）

推送 tag 即可触发 GitHub Actions 自动编译并发布：

```bash
git tag v0.1.2
git push origin v0.1.2
```

## ✨ 功能

### 📦 插件管理

- **插件浏览**：扫描当前 Profile 下所有 DSH 插件（支持 scoped 包 `@scope/name`、符号链接）
- **子依赖识别**：被聚合包（如 `dsh-web-all`）的 `dependencies` 带入的包自动标记为「子依赖」，默认折叠、半透明弱化显示
- **启用/禁用**：Switch 开关切换，本质是编辑 Profile `package.json` 的 `dsh.profile.bundles` 数组
- **安装/卸载**：封装 `dsh plugin --profile <name> add/remove <pkg>` 命令
- **Profile 切换**：自动发现 `~/.dsh/profiles/` 下所有 Profile
- **搜索过滤**：按名称/描述实时过滤
- **快捷入口**：一键打开插件目录 / Profile 目录

### 🖥️ 终端会话管理（tmux）

适合用 tmux 托管 `dsh web` 进程的场景：

- **会话列表**：名称、窗口数、创建时间、附着状态、运行命令、工作目录
- **DSH 智能识别**：窗格命令为 `node` 且工作目录含 `.dsh` → 自动标记 DSH 徽章
- **查看输出**：抓取会话最近 100 行日志（`tmux capture-pane`）
- **附加终端**：一键打开 Terminal.app 并 `tmux attach` 到对应会话
- **重启 DSH**：发送 Ctrl-C 后自动重新执行 `dsh web`
- **一键启动**：未检测到 DSH 会话时显示启动横幅，一键拉起
- **自动刷新**：页面可见时每 4 秒刷新，切走自动停止

### 🌐 SSH 远程服务器管理

远程管理服务器上的 DSH 插件：

- **服务器列表**：备注名 + SSH 地址 + 端口，QSettings 持久化
- **地址解析**：支持 `user@host -p 6005`、`ssh -p 6005 user@host` 等格式，粘贴完整命令自动拆分端口
- **认证方式**：密钥认证（需 `ssh-copy-id` 免密）或密码认证（OpenSSH `SSH_ASKPASS` 机制，无需 sshpass；可选"记住密码"）
- **异步连接**：连接 + 首次扫描在工作线程执行，不阻塞 UI，期间显示等待动画
- **完整的插件操作**：浏览/启用/禁用/安装/卸载、Profile 切换，全部走远程
- **打开目录**：远程模式复制路径到剪贴板

### 🔄 插件同步（本地 → 远程）

- **同步清单**：默认只显示直接安装的插件（子依赖自动随父插件带上），「子依赖」开关可展开
- **选择性同步**：全选 / 仅选需更新 / 手动勾选
- **版本策略**：本地为主（远程较新也被本地覆盖）
- **依赖闭包**：选中聚合包时自动带上其 DSH 子依赖，保证远程可运行
- **实时进度**：进度条 + 逐插件日志，全程工作线程执行
- **网络容错**：上传失败自动重试 2 次

## 🔍 DSH 插件模型

| 状态 | 判定方式 |
|------|---------|
| Profile | `~/.dsh/profiles/<name>/` 下含 `package.json` 的目录 |
| 已安装插件 | Profile 的 `node_modules/` 下 `package.json` 含 `"dsh"` 字段的包 |
| 已启用 | 包名出现在 Profile `package.json` 的 `dsh.profile.bundles` 数组中 |
| 子依赖 | 被其他已安装包的 `dependencies` 引用，且不在 Profile 自身 `dependencies` 中 |

## 🛠️ 构建

### 依赖

- Qt 6.5+（开发环境为 Qt 6.11.2，通过官方安装器安装）
- CMake 3.16+
- C++17 编译器（macOS 为 Xcode 命令行工具）

### 构建步骤

```bash
git clone git@github.com:YakutsukuriYuu/dsh-plugin-manager-qt.git
cd dsh-plugin-manager-qt

mkdir build && cd build

# CMAKE_PREFIX_PATH 指向你的 Qt 安装目录
cmake .. -DCMAKE_PREFIX_PATH=~/Qt/6.11.2/macos
cmake --build .
```

### 运行

```bash
open build/bin/dsh-plugin-manager.app
# 或查看控制台日志
./build/bin/dsh-plugin-manager.app/Contents/MacOS/dsh-plugin-manager
```

## 📁 项目结构

```
├── CMakeLists.txt              # 构建配置（QML 模块、Theme 单例、图标/Liquid Glass 资源）
├── cmake/Info.plist.in         # 自定义 Info.plist（含 CFBundleIconName 声明 LG 图标）
├── resources/                  # 应用图标（.icon 源文件 + 预编译 Assets.car）+ Logo
├── resources/icons/            # Lucide SVG 图标及 tools/make_icons.py 烘焙的主题色变体
├── tools/make_icons.py         # 把 Lucide currentColor SVG 批量烘焙成 6 个主题色
├── src/
│   ├── main.cpp                # 入口：注册 5 个上下文属性（plugin/tmux/update/remote/sync）
│   ├── core/
│   │   ├── PluginBackend.h     # 后端抽象接口（本机/远程统一）
│   │   ├── LocalBackend.h      # 本机后端（QFile/QDir）
│   │   ├── SshBackend.h        # SSH 后端（端口/密码/askpass/一次扫描/tar 上传）
│   │   ├── PluginManager.*     # 插件管理核心（对接 backend，异步远程连接）
│   │   ├── RemoteManager.*     # 服务器列表 CRUD + 连接切换
│   │   ├── SyncManager.*       # 本地→远程同步（计划/闭包/上传/配置更新）
│   │   ├── TmuxManager.*       # tmux 会话管理
│   │   └── UpdateChecker.*     # GitHub Releases 更新检查
│   └── ui/qml/
│       ├── Theme.qml           # 单例：颜色/字号主题
│       ├── Main.qml            # 主窗口（侧边栏 + 页面 + 对话框）
│       └── components/
│           ├── AppIcon.qml     # 矢量图标组件（按 iconColor 加载烘焙变体）
│           ├── PluginList.qml  # 插件列表页（搜索/筛选/子依赖折叠/远程横幅）
│           ├── PluginCard.qml  # 插件卡片（Switch 启停 + 幽灵图标按钮）
│           ├── TmuxPage.qml    # tmux 会话页
│           ├── RemotePage.qml  # 远程服务器页
│           └── SyncDialog.qml  # 同步对话框
└── build/                      # 构建输出（不入库）
```

## 🏗️ 技术要点

- **后端抽象**：所有插件文件/命令操作走 `PluginBackend` 接口，本机（`QFile`/`QDir`）与远程（`ssh`）逻辑完全复用，上层 `PluginManager` 无感知
- **异步模型**：远程连接 + 首次扫描用 `QtConcurrent` 跑在工作线程，`QFutureWatcher` 把结果移回主线程，全程不阻塞 UI；扫描逻辑提取为静态函数保证跨线程安全
- **单次往返扫描**：远程扫描把整个 `node_modules` 的所有 `package.json` 合并为一条 shell 脚本抓取（`@@@ENTRY` 分隔符），避免逐文件 SSH 往返
- **密码认证**：OpenSSH `SSH_ASKPASS_REQUIRE=force` 机制（临时 0700 脚本输出密码），不依赖 sshpass；`tar | ssh` 上传用 `QProcess::setStandardOutputProcess`
- **Theme 单例**：`QT_QML_SINGLETON_TYPE` 声明；操作按钮用 Lucide SVG + 预烘焙着色变体（Qt 6.11 无公开 currentColor 支持）
- **图标**：Liquid Glass 新格式 `.icon` 经 `actool` 编译进 `Assets.car`，Dock 中铺满显示（macOS 27）

## 📋 路线图

- [x] 插件浏览/启停/安装/卸载
- [x] tmux 会话管理
- [x] SSH 远程服务器管理（端口/密码/异步连接）
- [x] 本地 → 远程插件同步（选择性 + 依赖闭包）
- [x] GitHub Releases 更新检查
- [ ] 插件详情页（README 预览、依赖树）
- [ ] 插件市场（从 npm registry 搜索 DSH 插件）
- [ ] 多 Profile 批量操作
- [ ] Windows / Linux 适配

## 许可证

MIT
