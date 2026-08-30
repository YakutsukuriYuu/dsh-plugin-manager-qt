# dsh-plugin-manager-qt

基于 **Qt 6 / QML** 的 [DSH](https://www.npmjs.com/package/@deepseek-ai/dsh)（DeepSeek Harness）插件管理桌面应用。

为 DSH 提供一个图形化的插件与运行环境管理工具：浏览/安装/启停插件、管理 tmux 中的 DSH 会话，无需记忆命令行操作。

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
├── CMakeLists.txt              # 构建配置（QML 模块、Theme 单例声明）
├── src/
│   ├── main.cpp                # 入口：注册 pluginManager / tmuxManager 上下文属性
│   ├── core/
│   │   ├── PluginManager.*     # 插件管理（扫描/启停/安装/卸载，主线程同步执行）
│   │   └── TmuxManager.*       # tmux 会话管理（list/capture/send-keys/new/kill）
│   └── ui/qml/
│       ├── Theme.qml           # 单例：颜色/字号主题
│       ├── Main.qml            # 主窗口（侧边栏 + 页面 + 对话框）
│       └── components/
│           ├── PluginList.qml  # 插件列表页（搜索/刷新/子依赖折叠）
│           ├── PluginCard.qml  # 插件卡片（Switch 启停 + 幽灵图标按钮）
│           └── TmuxPage.qml    # tmux 会话页（会话卡片/输出查看/新建会话）
└── build/                      # 构建输出（不入库）
```

## 🏗️ 技术要点

- **线程模型**：所有扫描在主线程同步执行（仅读少量 JSON，毫秒级完成），数据以 `QVariantList` / `QVariantMap` 暴露给 QML，彻底避免 QObject 跨线程问题
- **Theme 单例**：通过 `QT_QML_SINGLETON_TYPE` 声明，各 QML 文件 `import DshPluginManager` 后使用 `Theme.xxx`
- **工具查找**：`dsh` / `tmux` 依次检查 `$PATH`、`/opt/homebrew/bin`、npx 缓存路径；执行子进程时自动补充 PATH 以便找到 node/npm/pnpm
- **UI 组件**：启用/禁用使用带滑动动画的 Switch（行业标准交互）；次要操作使用幽灵图标按钮，悬停才显现背景

## 📋 路线图

- [ ] 插件详情页（README 预览、依赖树）
- [ ] 插件市场（从 npm registry 搜索 DSH 插件）
- [ ] 多 Profile 批量操作
- [ ] DSH 会话日志流式显示（而非定时抓取）
- [ ] Windows / Linux 适配

## 许可证

MIT
