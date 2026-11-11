"""Replay the pinned Debian development packages; extraction only, no system install."""
import argparse, hashlib, json, pathlib, subprocess, urllib.request
parser = argparse.ArgumentParser()
parser.add_argument("--cache", type=pathlib.Path, default=pathlib.Path("/tmp/telegram-debs"))
parser.add_argument("--sysroot", type=pathlib.Path, default=pathlib.Path("/tmp/telegram-sysroot"))
parser.add_argument("--verify-only", action="store_true")
args = parser.parse_args()
manifest = pathlib.Path(__file__).with_name("DEBIAN_PACKAGES.json")
args.cache.mkdir(parents=True, exist_ok=True)
for entry in json.loads(manifest.read_text()):
    target = args.cache / entry["filename"]
    if not target.exists():
        if args.verify_only:
            raise SystemExit("Missing archive: " + str(target))
        if not entry["url"].startswith("https://deb.debian.org/debian/"):
            raise SystemExit("Unexpected archive origin")
        with urllib.request.urlopen(entry["url"]) as response:
            data = response.read()
        if hashlib.sha256(data).hexdigest() != entry["sha256"]:
            raise SystemExit("Downloaded hash mismatch: " + str(target))
        target.write_bytes(data)
    if hashlib.sha256(target.read_bytes()).hexdigest() != entry["sha256"]:
        raise SystemExit("Cached hash mismatch: " + str(target))
    print("Verified", entry["package"], entry["version"], flush=True)
    if not args.verify_only:
        args.sysroot.mkdir(parents=True, exist_ok=True)
        subprocess.run(["dpkg-deb", "-x", str(target), str(args.sysroot)], check=True)
