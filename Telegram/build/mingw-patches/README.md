# MinGW cross-compile patches for vendored submodules

These diffs sit on top of the pinned submodule checkouts so LanLink /
Telegram Desktop can be cross-compiled with MinGW-w64. They are not
pushed to the upstream desktop-app or third-party remotes.

Apply from the repository root after `git submodule update --init`:

```
git -C cmake apply "$(pwd)/Telegram/build/mingw-patches/cmake.patch"
git -C Telegram/lib_base apply "$(pwd)/Telegram/build/mingw-patches/Telegram_lib_base.patch"
git -C Telegram/lib_webrtc apply "$(pwd)/Telegram/build/mingw-patches/Telegram_lib_webrtc.patch"
git -C Telegram/lib_ui apply "$(pwd)/Telegram/build/mingw-patches/Telegram_lib_ui.patch"
git -C Telegram/lib_webview apply "$(pwd)/Telegram/build/mingw-patches/Telegram_lib_webview.patch"
git -C Telegram/codegen apply "$(pwd)/Telegram/build/mingw-patches/Telegram_codegen.patch"
git -C Telegram/ThirdParty/libfido2 apply "$(pwd)/Telegram/build/mingw-patches/Telegram_ThirdParty_libfido2.patch"
git -C Telegram/ThirdParty/range-v3 apply "$(pwd)/Telegram/build/mingw-patches/Telegram_ThirdParty_range-v3.patch"
```

The first Windows GUI package was produced with these patches already
applied in the build worktree.
