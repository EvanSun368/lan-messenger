# v0.14 source-publication log

## 2026-11-13 (Asia/Shanghai)

- Read code-changes, source-control, and ub-mode instructions. Publication is
  limited to the already implemented slim v0.14 source; no feature changes.
- Inspected local source commit f0a92a4cf3ac2de17b09afe09199a3c8c973c4b0
  and remote main 362b6188829f18fac1e6e5445fa814460cd5a588.
- Used an isolated checkout based on remote main. The publication commit
  preserves existing remote history, replacing only its current full-desktop
  tree with the slim app, transport, tests, assets, and build definitions.
  The suspended full-desktop feature branch is not merged.
- Copied tracked source inputs only. Excluded all screenshots, generated
  builds, packages, metrics, and runtime/private peer data. Replaced the
  historical local action log with this source-publication log.
- Preserved LICENSE and LEGAL; included Open Sans and TDLib license texts
  and official upstream references. Updated README for the current client,
  dependency setup, recent fixes, and the main-branch source layout.
- Expanded ignore rules, anonymized a test hostname (including both letter
  cases), and removed inherited trailing whitespace. No production behavior
  was changed. A first anonymized-fixture run exposed the missed lowercase
  fixture (134 tests, 945 checks, 1 failure); corrected it, then passed the
  same suite with 134 tests, 945 checks, 0 failures.
- Original source build/tests also passed: 134 tests, 945 checks, 0 failures.
- Ran crapper, dryer (threshold 0.8), and mutator on app/ and lan/. They
  report no analyzable source because these tools do not support C++; no
  complexity, duplication, or mutation-coverage score is claimed.
- Audited the publication index for secrets, generated/private files,
  oversized files, and missing resource references; checked added/modified
  source whitespace. Stopped a whole-tree whitespace scan that needlessly
  fetched deleted historical blobs, then checked current source inputs.
- Final verification uses a Linux Release CMake build and the offscreen
  lan_tests suite. No new Windows execution or packaging is claimed.
- Commit/push is a normal descendant update to main, with a remote-head
  recheck immediately before push. No force, history rewrite, hook bypass,
  branch deletion, release, or Windows-package upload is used.
- Final post-cleanup Release build passed for LanLink, lan_tests, and
  lan_node; lan_tests passed 134 tests, 945 checks, 0 failures.
