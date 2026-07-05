# Publishing Benchmark Results Safely

Raw provider output is untrusted. It can contain credentials, local paths,
session identifiers, provider metadata, or tool transcripts. Files under
`results/` must remain private and Git-ignored.

## Export

Generate an allowlisted public projection:

```bash
npm run export:public -- --input results --output public-results
```

The exporter:

- extracts answer text from supported provider envelopes;
- omits raw model paths, provider options, stderr, errors, usage, cost, UUIDs,
  session IDs, and tool metadata;
- retains the task's suite, validation profile, and public target-profile
  identifier when available;
- redacts recognized credentials, private keys, home-directory usernames, and
  identifiers inside answer text;
- validates each output against the public result contract; and
- records redaction types and counts without recording sensitive values.

Exports containing redactions exit with status 2 and set
`publication.reviewRequired` to `true`. Inspect those files and the private
source, rotate any potentially real credential, then rerun with
`--allow-redactions` only to acknowledge that review.

Raw records created before target profiles were introduced export with
`task.targetProfile: null`. Do not infer a current profile for historical
records; rerun the task if profile-bearing provenance is required.

Raw records without `validationProfile` cannot be exported under the v1.3
contract. Rerun them to establish hosted-environment provenance.

Raw records created before suite metadata was introduced infer `firmware` only
when their recorded target profile is non-null; all others infer `auxiliary`.
New raw records store the suite explicitly.

The canonical public-result schema and all new exports use v1.3.

## Review Checklist

1. Run `npm run security:scan`.
2. Confirm only files matching `schemas/public-result.schema.json` are present.
3. Inspect every export with `reviewRequired: true`.
4. Confirm model IDs are public labels, not private paths or endpoint URLs.
5. Verify prompts, provider settings, tool access, and run counts are disclosed
   separately without credentials.
6. Confirm the model/service terms permit publication of generated output.
7. Publish from a separate reviewed location; never force-add `results/`.

Sanitization reduces accidental disclosure risk but does not determine
copyright, model-provider terms, or whether generated code is safe to execute.
The repository scan covers tracked files and non-ignored untracked files, so a
force-added result file is scanned in CI.
