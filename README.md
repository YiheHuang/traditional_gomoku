# 五子棋 AI — Alpha-Beta 引擎

一个 C++ 实现的五子棋人工智能引擎，通过 pybind11 提供 Python 接口，支持图形化对弈和棋谱分析。

## 特性

- **高性能 Alpha-Beta 搜索**：PVS 空窗搜索 + 迭代加深 + 置换表 + 杀手/历史启发
- **15×15 标准棋盘**：经典五子棋规则
- **四种模式**：人机对战、本地双人对弈、棋谱分析、线上对战
- **线上对战**：通过中转服务器匹配对手，远程实时对弈
- **SGF 棋谱**：导入/导出标准 Smart Game Format
- **棋谱分析**：← → 键浏览，实时胜率热力图
- **tkinter 图形界面**：原生 Python GUI，支持鼠标落子、悬停预览
- **静态链接**：C++ 运行时内嵌，无需额外运行库

## 项目结构

```
├── src/                    # C++ 核心引擎
│   ├── board.h/cpp         #   棋盘表示 + Zobrist 哈希
│   ├── pattern.h/cpp       #   棋型识别与评分
│   ├── evaluator.h/cpp     #   局面评估函数
│   ├── movegen.h/cpp       #   着法生成与启发式排序
│   ├── transposition.h/cpp #   置换表缓存
│   ├── threat.h/cpp        #   威胁空间搜索 (VCT/VCF)
│   ├── search.h/cpp        #   Alpha-Beta 搜索引擎
│   ├── game.h/cpp          #   对局控制器
│   └── bindings.cpp        #   pybind11 Python 绑定
├── gomoku/                 # Python 应用层
│   ├── __init__.py         #   自动加载 C++ 扩展
│   ├── app.py              #   tkinter 主程序图形界面
│   ├── board_ui.py         #   共享棋盘绘制模块
│   ├── client_online.py    #   线上对战客户端（独立窗口）
│   └── server.py           #   中转匹配服务器（部署到 CentOS）
├── main.py                 # 程序入口
├── build_extension.py      # 一键编译脚本
├── CMakeLists.txt          # C++ 构建配置
├── pyproject.toml          # Python 项目元数据
├── installer/              # Inno Setup 安装包
│   └── setup.iss
└── guidebook.md            # 详细教学文档
```

## 环境要求

- **Python** ≥ 3.8
- **CMake** ≥ 3.16
- **编译器**：MinGW-w64 (Windows) / GCC (Linux) / Clang (macOS)
- **pybind11** ≥ 3.0 (`pip install pybind11`)

## 快速开始

```bash
# 1. 安装依赖
pip install pybind11

# 2. 编译 C++ 扩展
python build_extension.py

# 3. 运行
python main.py
```

### 手动编译

```bash
# Windows (MinGW)
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release \
    -Dpybind11_DIR="<Python路径>/Lib/site-packages/pybind11/share/cmake/pybind11"
cmake --build build --config Release

# Linux / macOS
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

编译产物 `_gomoku_core.cp3xx-*.pyd`（Windows）或 `_gomoku_core.cpython-*.so`（Linux/macOS）存放在 `build/` 目录，`gomoku/__init__.py` 会自动加载。

## 使用说明

### 人机对战

默认模式。玩家执黑先手，点击棋盘交叉点落子。AI 在后台线程计算，UI 不卡顿。

- 「游戏」→「选择阵营」→ 切换先手/后手
- 「设置」→ 调节 AI 思考时间（1/3/5/10 秒）

### 线上对战

「模式」→「线上对战」，启动独立客户端窗口。

客户端通过 TCP 连接到中转服务器（默认 `192.144.228.237:9999`），服务器负责：
- **匹配对手**：将等待中的两名玩家配对
- **转发棋步**：将一方着法实时转发给另一方
- **胜负判定**：服务端验证五连、棋盘满、认输、断线等终局条件

客户端可独立运行，无需主程序：
```bash
python -m gomoku.client_online --host <服务器IP> --port 9999
```

### 部署中转服务器

服务器为纯 Python 实现，无第三方依赖，部署到 CentOS：

```bash
# 上传服务器文件
scp gomoku/server.py root@<服务器IP>:/opt/gomoku_server/

# SSH 登录服务器
ssh root@<服务器IP>

# 开放防火墙端口
firewall-cmd --zone=public --add-port=9999/tcp --permanent
firewall-cmd --reload

# 安装 systemd 服务（开机自启）
cp installer/gomoku-server.service /etc/systemd/system/
systemctl daemon-reload
systemctl enable --now gomoku-server

# 查看运行状态
systemctl status gomoku-server
```

### 本地对战

「模式」→「本地对战」，两人共用一台电脑交替落子。

### 棋谱分析

「模式」→「棋谱分析」：

| 操作 | 说明 |
|------|------|
| 点击空位 | 自由摆棋 |
| ← → 键 | 前进/后退一步 |
| 导入 SGF | 载入棋谱分析 |
| 导出 SGF | 保存当前棋谱 |

棋盘上的彩色圆圈为 AI 评估的候选着法：
- 🟢 绿色 = 对当前方有利
- 🔴 红色 = 对当前方不利
- 数字 = 该着法的预估胜率 (%)
- ★ = AI 首选着法

## SGF 棋谱格式

```
(;GM[11]SZ[15]PB[黑棋]PW[白棋]
;B[hh]      ← 黑棋 (7,7) = 天元
;W[gg]      ← 白棋 (6,6)
;...)
```

坐标编码：`a=0, b=1, …, o=14`，列在前行在后。

## 打包安装包

本项目使用 **PyInstaller** 打包为独立 exe，再通过 **Inno Setup** 生成安装程序。

### 依赖

```bash
pip install pyinstaller pybind11
```

需安装 [Inno Setup](https://jrsoftware.org/isinfo.php)（免费）。

### 一键打包

```bash
# 1. 编译 C++ pybind11 扩展
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release \
    -Dpybind11_DIR="<Python路径>/Lib/site-packages/pybind11/share/cmake/pybind11"
cmake --build build --config Release

# 2. 复制 .pyd 到包目录
cp build/_gomoku_core*.pyd gomoku/

# 3. PyInstaller 打包为独立 exe（使用 spec 文件，已含隐藏导入配置）
pyinstaller GomokuAI.spec
# → dist/GomokuAI.exe  (~11 MB)

# 4. Inno Setup 生成安装包
"C:/InnoSetup/ISCC.exe" installer/setup.iss
# → installer/GomokuAI-Setup-2.0.exe  (~12 MB)
```

> **v1.2 更新**：线上对战客户端（`client_online.py`）已内嵌至 exe。点击"线上对战"时，exe 以 `--online` 参数启动自身来运行客户端窗口。

### 安装包特性

- 中文安装向导
- 自定义安装路径（默认 `%ProgramFiles%\GomokuAI`）
- 开始菜单快捷方式 + 可选桌面快捷方式
- 控制面板 → 添加/删除程序中可卸载
- Windows 7+ 版本检测
- 单文件 exe，自包含 Python 运行时和 C++ 引擎，无需额外安装
- **线上对战**：点击菜单即启动独立客户端窗口，无需额外安装

## 技术参数

| 参数 | 值 |
|------|-----|
| 棋盘大小 | 15×15 |
| 搜索算法 | Alpha-Beta + PVS + 迭代加深 |
| 搜索深度 | 7-9 层 (5 秒时限) |
| 有效分支因子 | ≈ 3-5 |
| 置换表 | 64 MiB, 双路组相联 |
| 候选着法上限 | 18 |
| 静止期搜索深度 | ≤ 4 |

## 许可证

MIT
