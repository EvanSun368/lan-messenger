LanLink slim Windows x64 portable package
=========================================

This is a slim LAN messenger with a Telegram Desktop look.
It does not log into Telegram and does not use Telegram's servers.

How to run
1. Unzip the whole LanLink folder.
2. Double-click LanLink.exe. Keep the DLLs and plugin folders next to it.
3. The program uses this folder as its working directory when it is writable
   (identity, downloads/, state/, history/, settings.ini). Copying the folder
   to another machine rotates the peer id.

Optional flags:
  LanLink.exe --dir D:\path\to\data --name Alice --lang zh-CN

The interface language follows the Windows locale. Use --lang zh, zh-CN,
zh-Hans, zh-TW, zh-Hant, or en to force it, or change it later in Settings.
CJK text uses Microsoft YaHei UI / Microsoft YaHei when those fonts are installed.

What is in this build
- Official Telegram-style emoji in chat bubbles and in the picker, plus a
  bundled static sticker pack (artwork from official Desktop sprites).
  The picker closes after you insert an emoji or sticker.
- Photo, File, and Folder send live under the paperclip button, the same
  as official Telegram Desktop. You can also drag a file or folder onto
  the current chat.
- File-transfer encryption is a setting and is off by default (faster on
  a trusted LAN). Text stays end-to-end encrypted. Turn file encryption
  on in Settings if you want file payloads sealed with tde2e.
- Cancel an in-progress transfer from the bubble or the context menu.
  Cancel is final and does not resume. Resume after a drop still works.
- Recall (unsend) a sent text, sticker, or file within 48 hours. The peer
  sees "Message deleted" / "消息已删除".
- Edit Profile: photo, first name, last name, and bio. Name and bio are
  announced to LAN peers; the photo is only shown on this computer.
- The chat header and the peer profile show the other side's IP address.
- Night / day mode applies to the main window, menus, compose bar, and
  later-opened dialogs. The window uses the native Windows title bar
  (no extra in-window min/max/close buttons, which used to duplicate
  the system chrome).
- The compose box starts at one line and grows with wrapped text and
  newlines, up to the official Telegram Desktop max (224 px), then
  scrolls. Attach, emoji, and send stay at the bottom of the bar.
  Continuous wrapping input keeps the official compose background and
  buttons visible; the bar no longer collapses to a two-line white
  strip with the wallpaper showing through.
  Long text pastes are delivered in full (up to 1 MiB per
  frame; larger pastes are split into successive messages). A very
  long bubble is laid out like official Telegram Desktop: height is
  cached, only the visible slice is painted, and text taller than
  about 50 lines shows "Show more" / "显示更多". The full text stays
  stored and can be copied or expanded.
- Incoming files and folders wait for you to Receive or Decline, and
  you can pick a save location. A default folder is in Settings.
  Completed transfers show "Verified" / "已校验" after the SHA-256
  check (including the default unencrypted path, hashed while streaming).
- Right-click a bubble for a Telegram-style menu (Copy Text, Recall / 撤回,
  and so on), not a raw Windows menu.
- The Windows exe, taskbar, and title bar use the LanLink icon (not the
  official Telegram logo).
- The three-line menu next to the search box closes the same way as
  official Telegram Desktop: click the dimmed area outside the panel,
  or press Escape. It does not close just because the pointer left
  the panel.
- Chat history renders official-style typed markdown: **bold**, __italic__,
  ~~strike~~, `code`, ```pre```, ||spoiler||, [text](url), and bare URLs.
  The text is still stored as the markdown source. Underline and line-start
  blockquotes are not on this path.
- The three-line menu matches official Telegram Desktop visuals (274 px,
  134 px cover in dialogsBgActive, 48 px avatar, icons, night toggle,
  Version / About footer, 200 ms slide). It still closes only on a click
  outside the panel or Escape, not when the pointer leaves.
- Settings, Edit Profile, New Group, Members, and About use official-style
  boxes (8 px corners, dimmed layer, text buttons with confirm on the right).
  System file pickers stay native.
- Recalling a file or folder also deletes the already-saved payload on the
  receiver's computer when it is safe to do so. The sender's original file
  is never deleted. If the file is in use or cannot be removed, the bubble
  says so. Recall is still allowed for 48 hours after send.
- While a file or folder offer is waiting, the sender shows "Waiting to
  accept" / "等待接收". If the receiver declines, the sender shows
  "Declined to receive" / "拒绝接收" / "拒絕接收", according to the
  current language (OS locale, --lang, or Settings).
- Devices that go offline stay in the dialog list when local chat
  history exists, marked "offline" / "不在线" / "離線". Opening the
  chat still shows history. Sending while the peer is offline is
  blocked and shows a clear notice; there is no store-and-forward
  server. The device is shown as on LAN again when it comes back.
- If the same computer comes back with a new peer id (a copied data
  folder, or a fresh unpack that rotated the id), leftover offline
  history for that machine is merged into the live contact. The list
  keeps one row with the current name. Two computers that are both
  online are never merged just because they share a hostname. Saved
  display names are restored so history rows are not raw hex ids
  when the name is known. History is not discarded on merge.
- An incoming file offer lays out the icon, filename, size, Receive,
  Decline, and "Choose save location" / "选择保存位置" / "選擇儲存位置"
  without overlap, in day and night mode. Long labels wrap onto the
  next row; the time stays on its own reserved line.
- The sender now keeps "Waiting to accept" / "等待接收" until the
  receiver actually accepts (the offer used to be reported as
  started, which showed "0 B / size"). After accept, the sender
  shows live progress. A decline is still "Declined to receive" /
  "拒绝接收" / "拒絕接收".
- The idle loop no longer rewrites peer history or rebuilds the
  session list on every tick. File progress in the bubble updates
  about every 100 ms (and immediately on start, finish, fail, or
  decline). Sending a large file still uses CPU and LAN bandwidth
  while bytes are moving; when idle, the app rests at a 40 ms tick.


Caveats
- x64 only; MinGW build, not MSVC.
- Use on a trusted LAN. With file encryption off, file bytes travel in
  the clear. Encryption protects the payload, not membership of the LAN.
- Unsigned development preview; SmartScreen or antivirus may warn.
- Animated stickers (tgs / rlottie) are not played; the bundled pack is
  static. Official cloud sticker packs are not downloaded.
- No Telegram cloud login, wallet, Premium, or calls.
- File transfers are capped at 16 GiB. Resume still works after a drop;
  an aborted transfer does not resume.
- Recall is allowed for 48 hours after send.
- The avatar is not sent to peers. System title-bar chrome and system
  file dialogs follow the Windows theme, not the in-app night/day switch.
