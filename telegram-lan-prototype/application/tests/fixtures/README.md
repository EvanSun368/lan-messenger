# Local test TLS fixture

mail-fixture.crt and locally generated mail-fixture.key are disposable
local fake-server fixtures. The source kit excludes the private key; generate it
locally with generate_mail_fixture.py. It is not a production credential.
Never use it for a deployed server or install this certificate in a user's
trusted certificate store. Tests temporarily trust it only within their own
process and restore the previous Qt SSL configuration afterwards.
