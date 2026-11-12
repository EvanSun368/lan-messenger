# Third-party sources and notices

- Telegram Desktop artwork (wallpaper, icons, emoji sprites and derived static
  stickers): https://github.com/telegramdesktop/tdesktop . Preserve the root
  `LICENSE` (GPL-3.0-or-later) and `LEGAL` notice inherited from that tree.
- Open Sans font files: digitized data copyright 2010–2011 Google Corporation;
  Ascender Corporation. These bundled font files declare Apache License 2.0
  in their name metadata. Full text: `OpenSans-LICENSE.txt`.
- Official tde2e/tdutils: https://github.com/tdlib/td , tested dependency revision
  `51743dfd`, built with `TD_E2E_ONLY=ON`. Install headers and static libraries
  under `TDE2E_PREFIX`; dependency source is not vendored. Its Boost license is
  reproduced in `TDLib-LICENSE.txt`. LanLink calls the official
  `encrypt_message_for_one` / `decrypt_message_for_one` APIs.
- Qt 6.8: https://www.qt.io/licensing/ ; OpenSSL: https://www.openssl.org/ ;
  zlib: https://zlib.net/ . These are external build/link dependencies, not
  bundled binary distributions in this repository. Observe their license
  requirements when distributing executables and libraries.
