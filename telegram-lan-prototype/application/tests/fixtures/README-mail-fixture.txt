mail-fixture.crt/key are generated disposable self-signed loopback TLS-test fixtures.
The locally generated key is disposable test data, not a user/account credential.
Never deploy or trust it in production. The regression checks rejection by default, and positive TLS only with temporary
process-local test trust that is restored afterwards.

The distributable source kit deliberately omits mail-fixture.key. Before running
mail TLS tests, create a fresh local pair using:
  python3 application/tests/fixtures/generate_mail_fixture.py
The script uses an installed official OpenSSL CLI (pass its executable path as
an optional argument). This is local generation only, with no network access.
Do not commit or distribute the generated private key.
