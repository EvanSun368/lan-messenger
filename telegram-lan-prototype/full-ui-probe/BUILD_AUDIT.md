# Build and distribution audit (stage evidence)

- Original tdesktop worktree is unmodified (`git status --short` empty at review).
- The full-library probe imports original upstream targets. Its host integration,
  dependency selection and application tests are new code, not upstream code.
- Qt 6.8.3 is insufficient for this upstream lib_ui revision. Qt 6.11.2 is the
  upstream-selected baseline. The separate old application build remains 6.8.3.
- Original InputField compiled and ran, with genuine style/emoji resource setup.
  Isolated success is not proof of actual LanLink integration or Windows support.
- Debian development archive manifest records exact filenames, versions, URLs and
  SHA256. 30 hashes verified against available official Packages index; three GL
  archive digests recorded from original official Debian downloads (explicitly
  marked). Downloader checks hashes before extracting, never installs systemwide.
- Manifest covers extracted packages, not the complete host OS. Existing glibc,
  GCC, runtime libraries, Python/setuptools and Qt/aqt are additional dependencies.
  A fresh-machine hermetic build has NOT been proven.
- Convenience shell scripts contain local prefixes and Linux x86_64 assumptions.
  Windows/macOS are not supported merely by copying these scripts.
- Telegram/lib_ui GPL-3.0-or-later with upstream OpenSSL exception applies.
  Preserve GPL/LEGAL and corresponding source with derivatives. Third-party
  libraries/assets retain their individual terms; see upstream README/headers.
  Emoji alpha-code MIT reference is preserved separately in emoji-component.
- No Telegram login/API keys/MTProto services were included in the probe.
  Full library size/initialization does not mean full Telegram application parity.
- A public executable release still needs dependency license collection, bundled
  runtime/resource verification, Windows packaging and actual device testing.

`fetch-upstream.sh` records a pinned, non-overwriting source bootstrap path.
It has been syntax-reviewed but not run on a new machine; existing successful
checkout was populated incrementally. Upstream network access remains required.

## Integrated verification update

The full mode is now wired into LanLink through a thin compatibility facade over
original Ui::InputField. Entire app built; seven CTest groups passed (22.26s),
and normal screenshot run exited 0. IME host submission guard and qApp teardown
null protection fixed integration bugs found by runtime testing. The raw editor
and model remain original upstream code. Tests include real loopback sends,
UTF-8 128KiB boundary, rejected draft preservation and persisted font formatting.
This supersedes the earlier isolated-only status, not the stated platform limits.

Parallel simultaneous FULL/default recompiles exhausted this environment's memory
once (cc1plus killed). The final FULL build was rerun serially and all 14 CTest groups passed (34.34s).
BUILD_JOBS defaults to 2 in build-local.sh and can be set to 1 on small machines.
Do not treat a previous test log as evidence for a failed newer build; final
commands use fail-fast execution before refreshing test logs and screenshots.
