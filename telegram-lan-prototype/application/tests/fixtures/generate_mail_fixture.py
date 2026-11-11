#!/usr/bin/env python3
"""Generate disposable TLS fixtures locally. Never use these for a real service."""
import pathlib
import subprocess
import sys
here = pathlib.Path(__file__).resolve().parent
openssl = sys.argv[1] if len(sys.argv) > 1 else "openssl"
subprocess.run([openssl, "req", "-x509", "-newkey", "rsa:2048", "-nodes",
    "-keyout", str(here / "mail-fixture.key"), "-out", str(here / "mail-fixture.crt"),
    "-days", "30", "-subj", "/CN=localhost",
    "-addext", "subjectAltName=DNS:localhost,IP:127.0.0.1,IP:::1",
    "-addext", "basicConstraints=critical,CA:TRUE"], check=True)
try:
    (here / "mail-fixture.key").chmod(0o600)
except OSError:
    pass
print("Created local test-only TLS fixture; do not deploy or share the private key.")
