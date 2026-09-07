# Fixture Script

`verify-reference.mjs` compiles the trusted supervisor through the same POSIX
redirection boundary as a candidate, links the deterministic mock and public
tests, executes the test binary, and removes its temporary build directory.
It also requires three equivalent reference variants to pass: reversed pipe
close order, reversed handler restoration order, and both changes together.
These positive controls prevent the tests from imposing cleanup ordering that
the prompt does not specify. The mutation catalog separately rejects missing
and duplicate descriptor cleanup and failure to restore SIGTERM.
