# LanLink transport

This directory is the serverless LAN layer for this Telegram Desktop fork.
It does not talk to Telegram's servers.

## What this module does

- Creates a local identity (random 16-byte peer id + a display name) and stores it in a directory.
- Discovers other peers with UDP multicast (`239.255.87.42:48710` by default, TTL 1).
- Exchanges text over direct TCP, with a length-prefixed binary frame and an acknowledgement.

The official intro/session hook lives alongside this transport (`lan_mode.h`,
`lan_ids.*`, `bridge.*`). A desktop build of this fork starts with a local
display name, lists discovered peers in the session, and sends text through
`lan::Node`. Files, voice or video calls, groups, and channels are still
out of scope.

`lan_ids.cpp` maps a 32-character `peer_id` onto a synthetic tdesktop
`UserId` (bit 46 set). That mapping is covered by `tests/test_ids.cpp`.

## Build and test

The module is plain C++17 and only needs a compiler (no Qt, no tdesktop submodules):

```bash
cd Telegram/SourceFiles/lan
c++ -std=c++17 -O2 -I. -Itests -o lan_tests \
  tests/test_main.cpp tests/test_identity.cpp tests/test_protocol.cpp tests/test_node.cpp \
  tests/test_ids.cpp identity.cpp protocol.cpp node.cpp lan_ids.cpp
./lan_tests

c++ -std=c++17 -O2 -I. -o lan_node lan_cli.cpp identity.cpp protocol.cpp node.cpp
./tests/smoke_two_processes.sh
```

If you have GNU make: `make test` and `make smoke`.

## Two processes on one machine

```bash
./lan_node --name Alice --dir /tmp/lan-alice --bind 127.0.0.1 --no-multicast
# note the printed udp= port, then in another terminal:
./lan_node --name Bob --dir /tmp/lan-bob --bind 127.0.0.1 --no-multicast
# then on each stdin:
target 127.0.0.1:<other-udp-port>
send Alice hello
```

On a real LAN, omit `--no-multicast` and `--bind 127.0.0.1`. Peers should appear via multicast. If multicast is filtered, use `--target` or the `target` command.

Stdin commands: `list`, `target <ipv4:udp-port>`, `send <peer-or-name> <text>`, `quit`.
