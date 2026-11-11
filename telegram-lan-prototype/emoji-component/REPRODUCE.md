# Original Telegram emoji component reproduction

Upstream lib_ui 42c6f1e88db81e6d9e8006f3e7b1c3ba39f9cac4:
engine/emoji_suggestions.cpp,.h,_helper.h; data/emoji.txt, data/emoji_autocomplete.json
Upstream codegen dbd1d53137cd581cfdbeebe93bcaf54abf55ec74:
codegen/common/*, codegen/emoji/*
Upstream lib_base 42a0e5dd634e638de7115e5a4cfcdd72786853a3:
base/qt/qt_string_view.h
All retain original headers; GPL3+ terms in LEGAL/LICENSE.

Build generator using Qt6 Core/Gui/Test, C++20:
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build
build/codegen_emoji -o./generated data/emoji.txt data/emoji_autocomplete.json
cmake --build build
build/emoji_probe

Original generator emits additional emoji.cpp/.h for full glyph pipeline;
the integrated application compiles only emoji_suggestions_data.cpp/.h and
the original suggestion engine. Unicode rendering remains system fonts.
No entire Telegram emoji UI is claimed.
