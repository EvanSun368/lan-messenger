# Optional VPN policy validation

Development preview, 2026-11-11, Qt 6.11.2 on Linux. No VPN was installed and no system routing, firewall or authentication configuration was changed.

- `test_networkpolicy`: 12 passed, 0 failed, 1 skipped. Exact IPv4 allowlists, CGNAT boundaries, interface observations, fixed ports, unauthorized ingress, chat/group ACKs, file/share/screen paths, stopped connections, policy revocation, diagnostics, queue migration and 125 dormant-mode rows are covered.
- `test_networkdiagnosticsui`: 4 passed, 0 failed. Local refresh, disabled demo dialing, closing/reopening with a live pending request and safe late callback handling are covered.
- CLI checks: 7/7, including missing mode/configuration, empty interface option without mode, public peer, zero VPN port, hostname rejection and help options.
- Affected baseline regression: 10/10 CTest groups passed before the final queue-index hardening. After hardening, core, delivery receipts, request lifecycle and both VPN test targets passed 5/5. A full merged-source suite must be run after concurrent mail work stabilizes.

Transport tests use real loopback TCP and explicitly injected interface-observation fixtures. The only skipped policy test is real OS interface enumeration: the execution sandbox denies Qt's AF_NETLINK socket. Production interface checks remain fail closed. This is not real VPN, two-city, route/firewall, Windows-native or cryptographic identity validation.

Interface/source-address checks do not force OS route selection. VPN/ACL/firewall administration and two-device acceptance remain required. No application-level identity authentication or E2EE is claimed.
