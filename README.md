# JiMei Campus NetLinker

集美大学校园网认证客户端，支持 Eportal 登录 / 退出、断网自动重连、静态 IP 管理。

基于 Qt 6.10.2，C++17，MinGW 64-bit。

## 截图

| 校园网认证 | 高级设置 |
|:---:|:---:|
| ![认证](screenshot-auth.png) | ![高级](screenshot-advanced.png) |

## 功能

### 校园网认证

- 校园网 Eportal 登录 / 退出
- 支持教育网、电信、联通、移动四种运营商
- 记住密码（Windows DPAPI 加密存储）
- 在线状态检测（60s 间隔轮询）
- 断网自动重连（最多 3 次）
- 系统托盘最小化、开机自启

### 高级设置

- 网卡选择与静态 IP 分配（172.19.0.0/16 网段）
- 随机 IP 防冲突（SQLite 记录已分配 IP）
- 一键还原 DHCP
- IP 分配历史记录查看与删除
- 适配真实物理网卡（优先以太网 / WLAN）

## 构建

### 依赖

- Qt 6.10.2（Widgets / Network / Sql）
- MinGW 64-bit
- CMake ≥ 3.16
- Windows SDK（netsh.exe 需要管理员权限）

### 编译

```bash
cmake -B build -DCMAKE_PREFIX_PATH="<Qt6>/mingw_64" -G "MinGW Makefiles"
cmake --build build
```

### 打包

```bash
build_release.bat
```

需要 `windeployqt` 在 PATH 中。

## 架构

```
JMCampusNetLinker
├── main.cpp              # 入口，字体/图标/主题加载
├── MainWindow.h/cpp      # 主窗口，认证流程调度，托盘，自启
├── EportalAuth.h/cpp     # Eportal HTTP 认证（登录/登出）
├── NetworkChecker.h/cpp  # 在线状态探测（HTTP 204）
├── IpManager.h/cpp       # netsh 静态 IP 操作（QThread worker）
├── IpManagerWidget.h/cpp # 高级设置页 UI
├── IpRecord.h/cpp        # SQLite IP 分配记录
├── ThemeManager.h/cpp    # 主题色管理，QSS 变量替换
├── fluent.qss            # Fluent Design 样式表
├── mainwindow.ui         # 主窗口 UI 布局
└── resources.qrc         # 图标资源
```

### 数据流

```
用户点击登录 → MainWindow → EportalAuth::login()
  → GET 探测页获取重定向 → POST 凭证 → 解析结果
  → NetworkChecker 持续监控 → 断网触发 AutoRelogin
```

```
用户分配 IP → IpManagerWidget → IpManager::assignIp()
  → QMetaObject::invokeMethod → IpWorker (QThread)
  → netsh interface ip set address ...
  → IpRecord 写入 SQLite
```

### 主题

`ThemeManager` 单例管理主题色，`fluent.qss` 使用 `{{ThemeColorPrimary}}` 等占位符，加载时自动替换。调用 `ThemeManager::instance().setThemeColor()` 即可运行时换肤。

## 许可

MIT
