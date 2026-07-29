# Fixture Script

`verify-reference.mjs` compiles the trusted supervisor through the same POSIX
redirection boundary as a candidate, links the deterministic mock and public
tests, executes the test binary, and removes its temporary build directory.
