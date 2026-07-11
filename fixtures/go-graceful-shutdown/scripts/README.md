# Scripts

Run `verify-reference.mjs` through
`npm run fixture:go-shutdown:self-test` with Go 1.24.4 available on `PATH`.
The trusted reference and every controlled mutation must compile; the baseline
must pass and every mutation must fail the public suite.
