# Telegram emoji search: original engine, not visual imitation

Pinned upstream desktop-app/lib_ui commit:
42c6f1e88db81e6d9e8006f3e7b1c3ba39f9cac4
Source: https://github.com/desktop-app/lib_ui/tree/42c6f1e88db81e6d9e8006f3e7b1c3ba39f9cac4/emoji_suggestions

Unchanged originals: emoji_suggestions.cpp, emoji_suggestions.h,
emoji_suggestions_helper.h. Generated files emoji_suggestions_data.cpp/.h
come from the original codegen_emoji built from desktop-app/codegen
dbd1d53137cd581cfdbeebe93bcaf54abf55ec74, using upstream emoji.txt and
emoji_autocomplete.json. Original codegen, inputs and generated outputs are
included in the companion emoji-component directory for reproducibility.

Build glue supplies cstring precompiled include (upstream normally supplies it).
Qt conversion/search field/selection integration is LanLink glue. The entire
suggestion engine, including query normalization, matching/ranking and compiled
official dictionary is upstream code, not a replacement implementation.

Legal: Desktop App Toolkit original license header retained; see
../DESKTOP_APP_LEGAL. Integration distributed under repository GPL-3.0-or-later.
The engine supports English keyword/shortcode suggestions. The category grid
remains LanLink's existing Unicode categories; this is not the complete Telegram
emoji panel, custom stickers, animated emoji or Telegram glyph renderer.
Emoji appearance still depends on local fonts. No Telegram/cloud connection.
