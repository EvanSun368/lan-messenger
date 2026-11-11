# LanLink

LanLink 是一个**面向局域网 / 内网的即时通讯工具**，从 [Telegram Desktop (tdesktop)](https://github.com/telegramdesktop/tdesktop) 派生，目标是尽量保留官方桌面客户端的界面与风格，但**不经过任何中心服务器**：同一局域网内的设备互相发现，直接通信。

This is a **serverless LAN / intranet messenger** forked from [Telegram Desktop](https://github.com/telegramdesktop/tdesktop). Peers discover each other on the local network and talk directly. There is no Telegram cloud login and no self-hosted MTProto server in the intended design.

**本项目与 Telegram 官方无关，也不是官方客户端。**  
**Not affiliated with Telegram. This is not the official Telegram Desktop client.**

当前仓库的 `main` 基于官方 tdesktop 的 `dev` 分支，提交 `2b502b26e1b85b502df70c4c048f44f02657af75`（上游 Version 7.4.2）。原有的 LanLink Qt 代码已从 `main` 替换；如需找回，请使用备份标签 `archive/lanlink-0.6.1-pre-tdesktop`（指向 `26676c78a1204e080b8e196ffe678338c2a37c22`）：

```bash
git fetch origin tag archive/lanlink-0.6.1-pre-tdesktop
git checkout archive/lanlink-0.6.1-pre-tdesktop
```

上游许可证仍是 GPLv3（含 OpenSSL 例外）。请保留 [LICENSE](LICENSE) 与 [LEGAL](LEGAL)。

## 现在实际做到哪一步 / What works today

已经落地并可测试的是无服务器传输层，位于 [`Telegram/SourceFiles/lan/`](Telegram/SourceFiles/lan/)：

- 本地身份：随机 `peer_id` + 本机保存的显示名，不需要手机号或 Telegram 账号
- 局域网发现：默认 UDP 组播 `239.255.87.42:48710`（TTL 1）；组播被过滤时可用单播 `target`
- 点对点 TCP 文本消息，带确认
- 单元测试，以及同一台机器上两个 `lan_node` 进程互发文本的冒烟测试

**尚未完成的部分（请勿误解）：**

- **官方 tdesktop 界面还没有接上这条局域网通道。** 若按上游方式直接编译桌面客户端，得到的仍是会登录 Telegram 云的官方路径，请不要把它当成已经可用的内网工具。
- 文件发送、语音/视频通话、群组、频道等还没有在局域网路径上实现。
- 当前局域网传输是明文，只适合受信任的内网。
- 本环境没有做完整的 tdesktop Linux/Windows 桌面构建（官方桌面构建依赖多、耗时长）。

下一步如何接到会话列表和聊天窗口，见 [docs/lan-serverless.md](docs/lan-serverless.md)。

## 先试用局域网文本通道 / Try the LAN text path

不需要初始化 tdesktop 的子模块。用一台装有 C++17 编译器的机器：

```bash
cd Telegram/SourceFiles/lan
c++ -std=c++17 -O2 -I. -Itests -o lan_tests \
  tests/test_main.cpp tests/test_identity.cpp tests/test_protocol.cpp tests/test_node.cpp \
  identity.cpp protocol.cpp node.cpp
./lan_tests

c++ -std=c++17 -O2 -I. -o lan_node lan_cli.cpp identity.cpp protocol.cpp node.cpp
```

同一台机器上的两个进程（回环、单播发现）：

```bash
./lan_node --name Alice --dir /tmp/lan-alice --bind 127.0.0.1 --no-multicast
./lan_node --name Bob   --dir /tmp/lan-bob   --bind 127.0.0.1 --no-multicast
# 在各自标准输入里执行：
#   target 127.0.0.1:<对方输出的 udp 端口>
#   send Alice hello
```

也可以运行 `./tests/smoke_two_processes.sh`。

在真实局域网中，不要加 `--no-multicast` 和 `--bind 127.0.0.1`，默认组播即可互相发现。若交换机或系统过滤了组播，再用 `--target`。

## 如何编译完整的桌面客户端 / Building the desktop app

完整界面仍按上游文档构建（当前尚未切换到局域网模式）：

- [Windows](docs/building-win.md)
- [macOS](docs/building-mac.md)
- [GNU/Linux（官方推荐 Docker）](docs/building-linux.md)

先初始化子模块（体积较大，需要时再拉取）：

```bash
git submodule update --init --recursive
```

本仓库已关闭 GitHub Actions，避免占用或触发上游自带的大量 CI。

## 第三方与版权

界面与大量底层代码来自 Telegram Desktop 及其第三方依赖，许可证说明见上游 README 历史与各子模块。本仓库新增的局域网模块同样以 GPLv3（含 OpenSSL 例外）发布。
