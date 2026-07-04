# Repository Rename

The public repository was renamed on July 4, 2026:

```text
arjun7965/llm-coding-benchmark
  → arjun7965/llm-firmware-benchmark
```

GitHub redirects the old URL, but clones and integrations should use the new
location explicitly:

```bash
git remote set-url origin \
  https://github.com/arjun7965/llm-firmware-benchmark.git
```

The npm package metadata, README badges, and JSON Schema `$id` values now use
`llm-firmware-benchmark`. Schema structure and task IDs did not change. Local
checkout directory names are arbitrary and do not need to match the repository
name.
