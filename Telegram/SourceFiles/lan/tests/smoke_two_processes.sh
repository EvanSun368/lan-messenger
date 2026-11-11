#!/usr/bin/env bash
# Two-process text-chat smoke test for lan_node on loopback.
# Uses unicast discovery targets because multicast to 127.0.0.1 is not
# reliable in every environment. Multicast remains the default LAN path.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/lan_node"
if [[ ! -x "$BIN" ]]; then
  echo "lan_node is not built" >&2
  exit 2
fi

WORKDIR=$(mktemp -d)
mkfifo "$WORKDIR/alice.in" "$WORKDIR/bob.in"

ALICE_PID=""
BOB_PID=""
cleanup() {
  exec 3>&- 4>&- 2>/dev/null || true
  kill $ALICE_PID $BOB_PID 2>/dev/null || true
  wait $ALICE_PID $BOB_PID 2>/dev/null || true
  rm -rf "$WORKDIR"
}
trap cleanup EXIT

"$BIN" --name Alice --dir "$WORKDIR/alice" --bind 127.0.0.1 --no-multicast \
  >"$WORKDIR/alice.out" 2>"$WORKDIR/alice.err" <"$WORKDIR/alice.in" &
ALICE_PID=$!
"$BIN" --name Bob --dir "$WORKDIR/bob" --bind 127.0.0.1 --no-multicast \
  >"$WORKDIR/bob.out" 2>"$WORKDIR/bob.err" <"$WORKDIR/bob.in" &
BOB_PID=$!

exec 3>"$WORKDIR/alice.in"
exec 4>"$WORKDIR/bob.in"

read_self_udp() {
  local file=$1
  local deadline=$((SECONDS + 5))
  while (( SECONDS < deadline )); do
    if grep -q '^self ' "$file" 2>/dev/null; then
      sed -n 's/.* udp=\([0-9]*\).*/\1/p' "$file" | head -1
      return 0
    fi
    sleep 0.05
  done
  return 1
}

ALICE_UDP=$(read_self_udp "$WORKDIR/alice.out")
BOB_UDP=$(read_self_udp "$WORKDIR/bob.out")
if [[ -z "${ALICE_UDP:-}" || -z "${BOB_UDP:-}" ]]; then
  echo "nodes did not start" >&2
  cat "$WORKDIR/alice.out" "$WORKDIR/alice.err" "$WORKDIR/bob.out" "$WORKDIR/bob.err" >&2 || true
  exit 1
fi

echo "target 127.0.0.1:${BOB_UDP}" >&3
echo "target 127.0.0.1:${ALICE_UDP}" >&4

deadline=$((SECONDS + 8))
while (( SECONDS < deadline )); do
  if grep -q "peer-up .* Bob " "$WORKDIR/alice.out" 2>/dev/null \
    && grep -q "peer-up .* Alice " "$WORKDIR/bob.out" 2>/dev/null; then
    break
  fi
  sleep 0.1
done

if ! grep -q "peer-up .* Bob " "$WORKDIR/alice.out" \
  || ! grep -q "peer-up .* Alice " "$WORKDIR/bob.out"; then
  echo "discovery failed" >&2
  echo "---- alice ----" >&2
  cat "$WORKDIR/alice.out" "$WORKDIR/alice.err" >&2 || true
  echo "---- bob ----" >&2
  cat "$WORKDIR/bob.out" "$WORKDIR/bob.err" >&2 || true
  exit 1
fi

echo "send Bob hello-from-alice" >&3
deadline=$((SECONDS + 5))
while (( SECONDS < deadline )); do
  if grep -q "recv .* Alice .* hello-from-alice" "$WORKDIR/bob.out" 2>/dev/null \
    && grep -q "^ack " "$WORKDIR/alice.out" 2>/dev/null; then
    echo "SMOKE_OK two-process loopback text chat (unicast UDP ${ALICE_UDP}<->${BOB_UDP})"
    exit 0
  fi
  sleep 0.1
done

echo "message exchange failed" >&2
echo "---- alice ----" >&2
cat "$WORKDIR/alice.out" "$WORKDIR/alice.err" >&2 || true
echo "---- bob ----" >&2
cat "$WORKDIR/bob.out" "$WORKDIR/bob.err" >&2 || true
exit 1
