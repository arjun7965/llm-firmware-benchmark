# Fixture Scripts

`verify-reference.mjs` compiles and runs the trusted implementation in a
temporary directory without invoking a shell. Use the repository-level
`test:mutations` command to verify `mutations.json`; use `fixture:extract` and
`fixture:validate` for model answers.
