# LanLink source integration lab

Current source lives in [application](application/README.md). This directory is a
separate, reviewable development line and does not replace the historical `cpp/`
or Python implementations at the repository root.

Implemented changes include the partial original Telegram UI integration,
independent mail OAuth/polling/reply workflows, explicit selected-chat mail drafts,
and opt-in managed-VPN peer policy/diagnostics. Read the exact limits in the
application documentation before using real accounts or remote networks.

## Build the default Qt implementation

Requirements: CMake 3.21+, C++17, Qt 6.5+ Core/Gui/Widgets/Network/Sql/Test/Concurrent,
Python 3 and OpenSSL CLI for disposable local TLS test fixtures. The final local
integration run uses Linux and Qt 6.11.2.

```sh
python3 application/tests/fixtures/generate_mail_fixture.py
cmake -S application -B build-default -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build-default --parallel 1
QT_QPA_PLATFORM=offscreen ctest --test-dir build-default --output-on-failure
```

The script creates a new local test certificate and private key. Do not deploy
or upload that key or trust its certificate system-wide. No real mail accounts
are needed for the fake-server regression suite.

## Full original-control build

See [full-ui-probe/README.md](full-ui-probe/README.md) and
[BUILD_AUDIT.md](full-ui-probe/BUILD_AUDIT.md). `fetch-upstream.sh` checks out the
pinned Telegram Desktop revision and submodules without replacing an existing
checkout. Qt 6.11.2 and the real upstream dependencies are required. The provided
local-prefix scripts describe this Linux test environment, not a hermetic installer.
FULL mode currently rejects Windows/macOS configuration explicitly.

## Licensing and verification

The combined derivative is GPL-3.0-or-later with upstream notices and OpenSSL
exception preserved in `application/LICENSE`, `application/LEGAL` and
`application/telegram/DESKTOP_APP_LEGAL`. Source-scope ledgers identify the reused
modules; `emoji-component/` includes the original generator, inputs, engine and
manifest. The full upstream tree is obtained through the pinned bootstrap instead
of vendored build products. Before distributing binaries, separately complete the
corresponding-source and third-party runtime/asset notice obligations.

[Final integration verification](application/INTEGRATION_VALIDATION.md) distinguishes
local automated checks from untested Windows, real mail, notifications and two-device
VPN operation. Older stage documents are historical evidence. This is not a release,
not full Telegram parity, and not a fix claim for the reported Windows receive crash.
