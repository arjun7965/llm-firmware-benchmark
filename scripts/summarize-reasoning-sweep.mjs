import { readFileSync, writeFileSync } from "node:fs";
import { join, resolve, relative, sep } from "node:path";
import { parseArgs } from "node:util";
import { loadCalibrationCohort, resolvePrivateResultsPath } from "../src/calibration-scoring.mjs";
import { sha256 } from "../src/fixture-answer-digests.mjs";
import { summarizeReasoningSweep } from "../src/reasoning-sweep.mjs";
import { extractAnswer } from "../src/answers.mjs";
import { extractFencedCode } from "../src/fixture-answers.mjs";

const { values } = parseArgs({
  strict: true,
  allowPositionals: false,
  options: { directory: { type: "string" }, output: { type: "string" } },
});
const root = resolvePrivateResultsPath(values.directory, { name: "--directory" });
const output = resolvePrivateResultsPath(values.output, { name: "--output", mustExist: false });
const read = (path) => JSON.parse(readFileSync(path, "utf8"));
function privateFile(path) {
  const resolved = resolve(root, path);
  const child = relative(root, resolved);
  if (child === "" || child === ".." || child.startsWith(`..${sep}`)) {
    throw new TypeError("evidence path must remain inside the cohort directory");
  }
  return resolvePrivateResultsPath(resolved);
}
function verify(path, expected) {
  if (!/^[a-f0-9]{64}$/u.test(expected) || sha256(readFileSync(path)) !== expected) {
    throw new TypeError(`frozen evidence changed: ${path}`);
  }
}
const provenance = read(join(root, "provenance.json"));
for (const required of ["cohort.json", "tasks.json", "plan.json", "fixture-hashes.json"]) {
  if (!provenance.files?.[required]) throw new TypeError(`missing frozen input: ${required}`);
}
for (const [path, digest] of Object.entries(provenance.files)) {
  verify(privateFile(path), digest);
}
const plan = read(join(root, "cohort.json"));
verify(join(root, "cohort.json"), readFileSync(join(root, "cohort.sha256"), "utf8").trim().split(/\s/u)[0]);
const task = read(join(root, "tasks.json")).find((entry) => entry.id === plan.taskId);
if (!task) throw new TypeError("frozen task is missing");
const fixtureRoot = join(root, "fixture-snapshot", task.id);
const fixtureHashes = read(join(root, "fixture-hashes.json"));
if (!fixtureHashes[`fixtures/${task.id}/manifest.json`]) {
  throw new TypeError("frozen fixture manifest digest is missing");
}
for (const [path, digest] of Object.entries(fixtureHashes)) {
  if (!path.startsWith(`fixtures/${task.id}/`)) throw new TypeError("unexpected fixture path");
  verify(privateFile(join("fixture-snapshot", path.slice("fixtures/".length))), digest);
}
const manifest = read(join(fixtureRoot, "manifest.json"));
if (manifest.answer.format !== "markdown-fenced-code") {
  throw new TypeError("reasoning sweep audit currently requires single-file fenced answers");
}
const { cohort } = loadCalibrationCohort(join(root, "raw"), task, plan);
const audits = read(join(root, "validation-audit.json"));
const evidence = audits.map((entry) => {
  const attempt = cohort.attempts.find((row) => row.modelName === entry.modelName && row.run === entry.run);
  if (!attempt?.source) throw new TypeError("audit has no recorded attempt");
  const rawPath = privateFile(join("raw", attempt.source));
  verify(rawPath, entry.resultSha256);
  const raw = read(rawPath);
  if (Date.parse(raw.finishedAt) - Date.parse(raw.startedAt) !== entry.durationMs) {
    throw new TypeError("audit duration does not match raw timestamps");
  }
  let report = null;
  let reportSha256 = null;
  if (attempt.outcome === "answer") {
    let code = null;
    try {
      code = extractFencedCode(extractAnswer(raw.stdout), { language: manifest.answer.language });
    } catch {
      if (entry.extraction?.success !== false) throw new TypeError("raw answer extraction disagrees with audit");
    }
    if (code !== null && (!entry.extraction?.success || sha256(code) !== entry.extraction.sha256)) {
      throw new TypeError("extraction audit does not match raw answer");
    }
  }
  if (entry.extraction?.success) {
    verify(privateFile(entry.extraction.outputPath), entry.extraction.sha256);
  }
  if (entry.validation) {
    const path = privateFile(entry.validation.reportPath);
    report = read(path);
    reportSha256 = sha256(readFileSync(path));
    if (report.success !== entry.validation.success) throw new TypeError("audit validation mismatch");
    if (report.validationProfile !== task.validationProfile ||
        report.phases?.some((phase, index) => phase.phase !== manifest.commands[index]?.phase)) {
      throw new TypeError("report does not match the declared validation contract");
    }
  }
  return { ...entry, report, reportSha256 };
});
const summary = summarizeReasoningSweep({
  cohort, evidence, phaseIds: manifest.commands.map((command) => command.id),
});
summary.cohortSha256 = sha256(readFileSync(join(root, "cohort.json")));
summary.planSha256 = sha256(readFileSync(join(root, "plan.json")));
summary.fixtureHashesSha256 = sha256(readFileSync(join(root, "fixture-hashes.json")));
writeFileSync(output, `${JSON.stringify(summary, null, 2)}\n`, { flag: "wx", mode: 0o600 });
console.log(`Wrote ${summary.attempts.length} verified attempts to ${output}`);
