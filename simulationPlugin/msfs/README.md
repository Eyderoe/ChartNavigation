# MSFS 原生 SimConnect 直连

按最终选定的方案，ChartNavigation 本身作为进程外 SimConnect 客户端。这里是共享采集模块与独立诊断程序，不是放入 Community 目录的 WASM 插件，也不提供 UDP/WebSocket 桥。

## 使用

1. Windows x64，启动 MSFS 2020/2024 并进入飞行。
2. 原生 **`SimConnect.dll`** 随 Windows 程序一起提供，构建时自动从 `simulationPlugin/msfs/SimConnect.dll` 复制到 `ChartNavigation.exe` 和独立诊断程序旁。用户无需安装 SDK、配置 SDK 路径或设置环境变量。
3. 航图程序选择「数据源 → MSFS」。无需运行下面的诊断程序。

开发诊断时的 DLL 查找顺序：`CHARTNAVIGATION_SIMCONNECT_DLL` 环境变量、应用目录、编译时的 `simulationPlugin/msfs` 源目录、`MSFS2024_SDK` / `MSFS_SDK` / `MSFS2020_SDK` 下的 `SimConnect SDK/lib/SimConnect.dll`、默认 `C:/MSFS 2024 SDK` / `C:/MSFS SDK`。正常使用直接加载程序旁的原生 DLL，不依赖 SDK。构建时以 `copy_if_different` 同步 DLL，替换源目录中的 DLL 后重新构建即可同步，无需重新链接可执行文件。

默认 `SimConnect_Open` 配置用于本机连接。远程 Windows 连接由 SimConnect 自己的 `SimConnect.xml` / `SimConnect.cfg` 配置管理，不是自建传输协议。macOS/Android 可继续构建，但原生直连菜单隐藏；当前方案不提供它们的 MSFS 网络接收端。

## 数据与生命周期

| 适配字段 | SimConnect 数据 | 单位 / 语义 |
|---|---|---|
| `id` | 当前帧紧凑槽位 + 1 | 非零表示可用；不是 Mode-S 或永久对象 ID |
| `lat`, `lon` | `PLANE LATITUDE`, `PLANE LONGITUDE` | 度，保留浮点精度 |
| `alt` | `PLANE ALTITUDE` | 米，海拔高度 |
| `trk` | `PLANE HEADING DEGREES TRUE` | 真航向，规范到 `[0, 360)`，与 XP 的 psi 字段对应 |
| `vs` | `VERTICAL SPEED` | ft/min |
| `flightId` | `ATC ID` | 前八字节、零填充；可能是注册号，不保证是航班呼号 |
| `icao` | `ATC MODEL` 的短字面值 | 仅接受 2..4 位大写字母/数字；是尽力提供的机型提示，不保证为真实 ICAO designator |

完整 ATC 模型文本保留为 `Aircraft::atcModel`，供诊断程序显示；`TT:ATCCOM...` 本地化键、长机型名称会令 `icao` 留空，不进行猜测或截断映射。尾流查询只在该提示恰好匹配现有机型表时生效。

用户机通过固定 `SIMCONNECT_OBJECT_ID_USER` 按秒订阅，始终占槽 0。每轮周边查询最多等待 1.5 秒，半径 200 km（官方上限），以 `IS USER SIM` 排除用户机。收齐一轮响应才替换周边列表；按球面距离排序，保留最近 63 架 `AIRCRAFT` 查询返回的周边航空器。通过该 API 只能获得模拟器向客户端暴露的对象和字段，不保证覆盖所有在线玩家或交通插件的私有数据。

Qt 事件循环每 25 ms 排空最多 256 条消息；所有 API 调用、缓存读写和连接回调都在同一线程，避免 SimConnect 的线程安全问题。订阅产生有效用户机数据才报告在线；用户机三秒无数据、QUIT 或 API 异常会清空缓存并关闭句柄，三秒后重连。打开后十五秒仍无有效用户机数据也会重新连接。无周边响应或目标消失会清空旧目标。`close()` 停止定时器，注销连接并清空数据；析构不再调用上层回调。

`addDatarefArray()` 返回固定字段索引，`freq` 为接口兼容参数；实际采集为 1 Hz，与当前 DataProvider 刷新频率一致。不支持任意 SimVar 或自定义频率订阅。

## 独立构建与诊断

只依赖 C++23、CMake ≥ 3.28、Qt6 Core。支持现有 MinGW 13.x，通过动态符号调用原生 Windows x64 C ABI，不要求 MSVC 导入库。

```powershell
cmake -S simulationPlugin/msfs -B build/msfs -G "MinGW Makefiles" `
  -DCMAKE_PREFIX_PATH=D:/Qt/Qt6/6.9.3/mingw_64 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/msfs -j 4
$env:PATH = 'D:\Qt\Qt6\6.9.3\mingw_64\bin;' + $env:PATH
ctest --test-dir build/msfs --output-on-failure
& .\build\msfs\msfs_monitor.exe --seconds 20
```

诊断输出包含 ONLINE/OFFLINE、本机与周边机对象编号、位置、高度、真航向、垂直速度和原始 ATC 模型。有限时长模式只有读到有效帧才以 0 退出；没有读到有效帧返回 1。发布诊断程序时，可用对应 Qt 的 `windeployqt` 部署 Qt 运行库，并配置原生 SimConnect DLL。

`SimConnectApi.hpp` 是从公开 API 文档定义的最小 ABI 子集，使用编译期大小检查及对齐安全的 `memcpy` 解码。构建和运行均不需要用户安装官方 SDK 或指定头文件路径。

## 验证记录（2026-10-09）

- 根项目 `ChartNavigation` 目标、独立采集库、诊断程序、适配器及 MSFS 专项测试已编译。
- Windows 假 SimConnect DLL 通过独立定义的 ABI 驱动实际动态加载/分发路径；七个测试用例验证订阅名/单位、用户机固定请求、乱序周边目标排序/去重/64 架上限、短 span 和零填充、错误索引、目标消失、超时、QUIT、异常、重连、本地化机型键过滤、非有限坐标、截断消息、错误字段计数和 DLL 缺失。
- 最终专项测试结果：7/7 用例、52/52 断言通过（约 12.34 秒）；记录位于 `build/msfs/Testing/Temporary/LastTest.log`。
- 假服务测试验证的是客户端逻辑，不是真实 MSFS 联调。当前机器未检测到运行中的 MSFS；仍需在真实飞行中验证 ONLINE、移动位置、周边机显示，以及退出/重启后的状态变化。
- 初次使用旧 Little Navmap 源码目录中的 DLL 时，Windows 报告“不是有效的 Win32 应用程序”。后续用户提供 `simulationPlugin/msfs/SimConnect.dll`，已加入自动查找和构建复制；源 DLL、主程序目录 DLL 与诊断程序目录 DLL 的 SHA256 一致。诊断程序不指定 `--dll` 即可加载该 DLL，已进入 `SimConnect_Open` 调用；由于没有运行中的 MSFS，返回 `0x80004005`，未建立真实飞行连接。
- 原有根项目 `unit_tests` 目标链接失败：地图测试依赖的 `airportSymbol` / `fixSymbol` / `vorSymbol` 等实现未加入测试目标。该问题不属于 SimConnect 代码，未修改原测试构建。

## 官方资料

- [SimConnect SDK 与进程外客户端建议](https://docs.flightsimulator.com/html/Programming_Tools/SimConnect/SimConnect_SDK.htm)
- [按对象类型查询与 200 km 上限](https://docs.flightsimulator.com/html/Programming_Tools/SimConnect/API_Reference/Events_And_Data/SimConnect_RequestDataOnSimObjectType.htm)
- [用户机固定对象 ID 与多人模式注意事项](https://docs.flightsimulator.com/html/Programming_Tools/SimConnect/API_Reference/Events_And_Data/SimConnect_RequestDataOnSimObject.htm)
- [单位和数据类型定义](https://docs.flightsimulator.com/html/Programming_Tools/SimConnect/API_Reference/Events_And_Data/SimConnect_AddToDataDefinition.htm)
- [飞行器位置与速度变量](https://docs.flightsimulator.com/html/Programming_Tools/SimVars/Aircraft_SimVars/Aircraft_Misc_Variables.htm)
- [ATC 字段](https://docs.flightsimulator.com/msfs2024/html/6_Programming_APIs/SimVars/Aircraft_SimVars/Aircraft_RadioNavigation_Variables.htm)
- [接收结构](https://docs.flightsimulator.com/html/Programming_Tools/SimConnect/API_Reference/Structures_And_Enumerations/SIMCONNECT_RECV_SIMOBJECT_DATA.htm)、[数据类型枚举](https://docs.flightsimulator.com/html/Programming_Tools/SimConnect/API_Reference/Structures_And_Enumerations/SIMCONNECT_DATATYPE.htm)、[消息枚举](https://docs.flightsimulator.com/html/Programming_Tools/SimConnect/API_Reference/Structures_And_Enumerations/SIMCONNECT_RECV_ID.htm)
