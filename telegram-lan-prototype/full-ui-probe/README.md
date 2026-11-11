# Original full lib_ui feasibility probe (Linux)

Unlike `input-component`, this directory does NOT extract or rewrite InputField.
It imports original upstream CMake targets for **all lib_ui, lib_base, lib_crl,
lib_rpl and codegen** from the pinned checkout in ../upstream. No dependency
stubs or upstream source edits are used. Only unrelated external targets are
excluded. `input_field_probe.cpp` is new application host glue.

## Provenance and licensing

Telegram Desktop checkout: c4bd064bcc84a359179e191fa6dfd216acda504f (7.4.1).
lib_ui gitlink: 42c6f1e88db81e6d9e8006f3e7b1c3ba39f9cac4.
The original files retain their upstream headers and legal notices. See
../input-component/DESKTOP_APP_LEGAL and ../input-component/LICENSE.
This integration must be distributed consistently with upstream GPL terms;
this is not a permissively licensed replacement implementation.

Original upstream repositories: https://github.com/telegramdesktop/tdesktop,
https://github.com/desktop-app/lib_ui, https://github.com/desktop-app/legal.

## Real dependency chain

C++20 compiler, CMake; Qt Core/Gui/Widgets/Svg/Network/DBus/Test and Qt private
headers; GSL, expected, range-v3, xxHash, LZ4, kcoreaddons, xdg-desktop-portal,
GLib/GIO and gobject-introspection, cppgir/expected-lite; OpenSSL, JPEG, Zlib,
XKB, Boost headers. Original codegen produces palette, styles and emoji tables.
Python g-ir-scanner needs distutils; isolated setuptools supplies the real
vendored distutils. All system dependencies were extracted from official Debian
packages into /tmp/telegram-sysroot, without changing system installations.

## Build attempts

1. Qt 6.8.3: configuration succeeded. lib_crl, lib_base, cppgir, D-Bus bindings,
codegen_style and codegen_emoji compiled. Full lib_ui reached 62%, failing in
ui/accessible/ui_accessible_widget.cpp lines 377/383 because Qt 6.8 lacks
QAccessible::Attribute::Orientation. No source edits to bypass it.
2. Upstream `Telegram/build/qt_version.py` and Linux Dockerfile explicitly
select Qt 6.11.2. Official qtbase and qtsvg 6.11.2 have been downloaded into a
separate prefix; fresh configure and complete compilation succeeded (see final result).

Local convenience scripts reference this environment's dependency prefixes;
they are not a portable bootstrap installer. Build logs preserve exact evidence.

The executable probe initializes real base/Ui integrations, main queue, style
manager and emoji system, then instantiates original Ui::InputField. The Linux
leave subscription no-op matches upstream Application's macOS-only workaround.
`--self-test` exercises newline vs submit, 20,000-character content, and screenshot.
These checks subsequently passed; see the final verified result below.

The isolated probe alone does not test LanLink networking. The later optional
application integration below adds real loopback TCP send tests. No Windows build,
physical cross-device validation or complete Telegram UI parity is claimed.

## Final verified result

Full original `lib_ui` static library and `input_field_probe` executable built
successfully with Qt 6.11.2. Upstream git status remains clean.
The standalone executable ran offscreen and exited **0**:

```
ORIGINAL_INPUT_FIELD_TEST newline true submit true longText true emoji true screenshot true
```

`run.log` is the final run; `input-field-probe.png` is a real widget screenshot,
not a design mockup. This tests Enter submission, Shift+Enter newline,
20,000-character set/get, actual emoji rendering/roundtrip and image capture.
There is no network send in this standalone test.

Additional real dependencies discovered and resolved:
- Qt 6.11.2 Vulkan headers (official Debian libvulkan-dev).
- JPEG's Debian multiarch include directory and xxHash link directory.
- Original Telegram Resources/qrc/emoji_*.qrc and official Qt imageformats WebP plugin.
- Application-provided crl::on_main_update_requests event bridge following
  upstream core/sandbox.cpp UpdateRequest behavior.
- Real Ui::Animations::Manager initialization before widgets.

The host integration is deliberately small and Linux-only; it does not recreate
Telegram's complete sandbox event-nesting machinery. Production integration
must validate modal/nested event loops and lifecycle callbacks separately.

Run locally:
```
./configure-local.sh
./build-local.sh
HOME=/tmp XDG_CACHE_HOME=/tmp/telegram-probe-cache QT_QPA_PLATFORM=offscreen \
 /tmp/telegram-full-ui-611-build/input_field_probe --self-test
```

The earlier failure logs are retained as evidence, followed by successful builds.
The Qt 6.8 attempt is not the final build. No Windows package is claimed.

## Actual LanLink application integration

The next stage is now implemented, not just proposed. Configure with
`-DLANLINK_BUILD_APPLICATION=ON` then build `BUILD_TARGET=all` using build-local.sh.
This enables `LANLINK_FULL_TELEGRAM_UI=ON` in ../application and replaces its
production editor with original Ui::InputField through a compatibility class.
The real application, not only the probe, is linked with original lib_ui and its
style/emoji resources. See application/README.md for explicit scope.

All 7 application CTest suites pass (`ctest-full.log`). The UI suite exercises
real TCP sends, IME composition protection, UTF-8 exactly 128 KiB, rejection with
retained draft, shortcuts, font changes and real screenshots. The app's chat
screenshot process exits 0 (`run-application.log`). Old reports above of no
application integration describe the earlier standalone-probe stage only.

Runtime fixes include an application-host preedit guard and qApp-null-safe
teardown. These modify host glue, not original InputField sources. Font styles
are owned per editor and kept alive through the original base destructor.
