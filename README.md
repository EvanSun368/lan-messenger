# lan-messenger
局域网即时通讯：聊天、文件共享（参考飞鸽传书 / QQ）

## 最新集成开发分支

当前 Telegram UI / 邮箱 OAuth、轮询和回复 / 受管 VPN 模式的源码位于
[`telegram-lan-prototype/`](telegram-lan-prototype/README.md)，详细验证结果见
[整合验收记录](telegram-lan-prototype/application/INTEGRATION_VALIDATION.md)。
这仍是源码实验阶段；FULL UI 目前仅 Linux，正式凭据后端仅 Windows，
Windows、真实邮箱和 VPN 双机端到端验收尚未完成。旧 `cpp/` 与 Python 实现保留。

## 先前主线：原生 C++ / Qt

当前主要开发版本位于 [`cpp/`](cpp/)，C++17 + Qt 6.8.3，现代气泡聊天界面。构建步骤、功能范围与验证记录请看 [C++ README](cpp/README.md)、[验证记录](cpp/VALIDATION.md) 和 [构建状态](cpp/BUILD_STATUS.md)。

Windows 构建：使用 Qt 6.8.3 MSVC2022 x64，执行 `cmake -S cpp -B build-cpp -A x64`、`cmake --build build-cpp --config Release`，然后 `ctest --test-dir build-cpp -C Release --output-on-failure`。Qt 路径须在 CMAKE_PREFIX_PATH 中或使用 Qt 开发环境命令行。完整打包流程见 [.github/workflows/windows-cpp.yml](.github/workflows/windows-cpp.yml)。

- 主分支合入不代表 Windows 安装包已产出。此前 Windows Actions 因账户支付/额度问题未启动；尚无当前版本 Windows EXE，不能使用旧包冒充。
- C++ 使用独立 `.lanlink-cpp` 数据目录，不自动导入 Python 的历史与身份。
- 根目录 Python 实现保留用于旧协议兼容与互通测试，不是当前原生程序入口；Python 0.4.1 接收端防护在独立 `build/windows-portable-0.4.1` 分支。
- 旧 Python 包的 Windows 接收端崩溃尚未复现确认，不能据此宣称根治；真实双机验收仍待完成。

