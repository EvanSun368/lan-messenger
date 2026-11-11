# Checking for updates

LanLink 0.6.1 can check GitHub Releases for a newer *telegram-mail-vpn* portable package and stage it for the next launch.

## What is matched

The running version is compared with release tags such as `v0.6.1-telegram-mail-vpn-windows-x64`. Only assets whose names contain both `telegram-mail-vpn` and the current platform (`windows-x64` or `linux-x64`) are considered. Older `*-cpp-windows-x64` 0.5.x packages are ignored.

Draft releases are skipped. Prereleases are included, because the existing Windows preview packages are prereleases.

## Private repository token

The repository is private. Without a GitHub token the app does not call the Releases API; it shows an error and the releases page:

https://github.com/EvanSun368/lan-messenger/releases

On Windows, an optional token can be saved in Windows Credential Manager under `LanLink/Updates/github` (the same service used for mail secrets, a different target). The token is never written to settings files, logs, or the screen after it is saved. Linux and macOS fail closed and do not store a token.

The token needs permission to read releases and assets of `EvanSun368/lan-messenger`.

## Manual and background checks

- The main menu and Settings provide **检查更新**.
- Settings also has **后台自动检查并下载更新** (on by default). After a 30 second delay, and then every 24 hours while the app is running, a background check runs if a token is present. The running app is never force-restarted.
- When a newer matching package is found and auto-update is on (or the user confirms a manual check), the zip is downloaded, SHA-256 is verified when GitHub provides a digest or a `.sha256` file, and the payload is staged. Incomplete or failed downloads do not replace the installed app.
- The staged copy is applied after the user quits, or on the next launch. Windows cannot overwrite a running executable, so a small helper waits for the process to exit. A crash mid-apply keeps the previous files in a backup and retries from the still-complete staged copy.

This updater does not use GitHub Actions.
