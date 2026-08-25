# Vendor Specification and Source Policy

This policy governs vendor documentation, SDKs, source code, binaries, and
other third-party technical material used to design benchmark tasks, prompts,
rubrics, fixtures, mocks, reference implementations, and hardware-in-the-loop
(HIL) guidance. It is a conservative repository acceptance policy, not legal
advice or a claim that an otherwise excluded use would be unlawful.

The benchmark should be understandable, reviewable, and reproducible from the
material committed here. Prefer repository-authored fictional peripherals and
self-contained behavioral contracts over vendor-specific interfaces.

## Source Classes

Classify every external source that materially influences committed content:

- `original-fictional` — repository-authored behavior that does not reproduce
  a vendor peripheral, SDK API, example, register map, or implementation.
- `public-specification-summary` — a concise, independently worded summary of
  technical facts from an official source that is available to the public
  without an NDA or confidential-access obligation.
- `redistributable-third-party` — a file committed under an explicit license
  that permits the repository's redistribution and intended use, with all
  required notices and attributions retained.
- `external-tooling-only` — an SDK, programmer, compiler, simulator, or other
  package obtained directly from its publisher and never committed here.

Public availability alone is not redistribution permission. A downloadable
SDK or example remains `external-tooling-only` unless its applicable license
has been reviewed and expressly permits committing that material.

## When a Specification Is Summarized

Committed content summarizes a vendor specification when a public vendor
source supplies any normative behavior or constraint, even if the wording and
interface names are changed. Examples include register or bit semantics,
required configuration sequences, timing or electrical limits, memory maps,
packet layouts, errata workarounds, and device-specific constants.

A product name, dependency name, version, license link, or link to an official
manual used only as external-tool metadata is not by itself a task
specification summary. The HIL catalog uses this metadata to identify lab
dependencies; it does not make those dependencies part of a benchmark prompt
or the required scoring path.

For a permitted summary:

1. Use the official publisher source and identify its title or document ID,
   revision or publication date when available, and canonical HTTPS URL.
2. Record exactly which facts influenced the task. Write the task contract in
   original language and include only the behavior needed for deterministic
   scoring.
3. Prefer a fictional, fixture-owned interface. Do not recreate the vendor's
   register layout, header surface, naming scheme, or example implementation
   merely to make the task look realistic.
4. Do not copy tables, diagrams, screenshots, substantial prose, code examples,
   headers, linker scripts, startup files, binary blobs, or documentation
   bundles. A necessary short quotation requires explicit review, attribution,
   and confirmation that its use is permitted.
5. Keep the prompt and validator self-contained. Scoring must not depend on a
   reviewer retrieving a mutable web page or accepting vendor terms.

Changing identifiers, deleting copyright notices, translating, or lightly
rewriting source material does not make it repository-authored.

## Prohibited Inputs

Do not use or derive repository content from:

- confidential, NDA-controlled, partner-only, customer-only, leaked, or
  internal vendor material;
- material obtained through an employer, client, support case, private portal,
  or account whose terms do not allow the intended use;
- proprietary or restrictively licensed SDK source, examples, headers,
  generated configuration, documentation, firmware, or binaries without
  explicit redistribution permission;
- decompiled or reverse-engineered vendor implementations when their use or
  distribution is restricted; or
- a third-party contribution whose origin or license cannot be established.

Confidential facts cannot be made acceptable by paraphrasing or fictionalizing
them. If source status is uncertain, omit the material and pause review until
the contributor can establish a public source and applicable permission. Do
not paste questionable content into an issue or pull-request discussion.

## External SDKs and HIL Material

Vendor SDKs and programmer tools may be documented as `external-tooling-only`
dependencies for supplemental HIL checks. They must be installed directly from
the publisher, kept outside the repository, pinned by version or digest where
practical, and accompanied by a source URL and license record. The repository
must not contain their installers, package contents, generated projects, or
prebuilt firmware.

A proprietary vendor dependency cannot become part of the required
deterministic scoring path. If a proposed task cannot be validated without a
public but nonredistributable environment, apply the rubric-only policy and
document the limitation. Undocumented or confidential behavior is not an
acceptable basis for either scoring mode.

## Source Provenance Record

Any new or materially revised task that uses vendor or third-party technical
material must add a `## Source Provenance` section to its rubric. Record:

- one or more source classes from this policy;
- publisher, document title or ID, revision/date, and canonical URL for every
  `public-specification-summary`;
- the limited facts derived from each source and which task files express
  them; and
- the redistribution decision: `not-vendored`, or the license identifier,
  source URL, retained notice, and attribution location for committed files.

An `original-fictional` task may instead state that its interfaces and device
model are original and do not reproduce a vendor peripheral or SDK. When
third-party files are committed, place their license and provenance beside the
files and update the repository `NOTICE` when the applicable license requires
it. HIL source and license metadata may live in `hil-targets.json` plus the HIL
guide rather than in a task rubric.

## Review Checklist

Before accepting a task, fixture, or HIL change that mentions a vendor:

1. Identify the origin of every externally derived technical fact and file.
2. Assign a source class and add the required provenance record.
3. Confirm that summarized specifications are official, public, and not
   subject to a confidentiality obligation or terms that prohibit the use.
4. Compare the change with the source for copied prose, tables, diagrams,
   identifiers, register layouts, and code; reduce it to an original,
   task-specific behavioral contract.
5. Confirm explicit redistribution permission before committing any
   third-party file, and retain all required licenses, notices, and attribution.
6. Keep nonredistributable SDKs, tools, generated projects, binaries, and HIL
   evidence outside Git and outside required scoring.
7. Verify that the prompt, mocks, tests, and trusted reference agree without
   requiring undocumented vendor behavior.
8. Run `npm run security:scan` and the normal fixture checks. These checks can
   catch credentials and structural mistakes, but they do not establish source
   rights; provenance review remains mandatory.

When permission remains ambiguous, exclude the material or obtain qualified
review before publication.
