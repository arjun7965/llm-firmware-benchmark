import { execFileSync } from "node:child_process";
import {
  existsSync,
  lstatSync,
  readFileSync,
  readdirSync,
  statSync,
} from "node:fs";
import { basename, join, resolve } from "node:path";
import { findSensitiveData } from "../src/public-results.mjs";

const excludedDirectories = new Set([
  ".git",
  "gpt55-results",
  "node_modules",
  "results",
]);
const excludedFiles = new Set([
  "models.local.json",
  "repeat-scores.json",
  "REPORT.md",
  "REPEAT_REPORT.md",
]);

function walk(path, files = []) {
  const metadata = statSync(path);
  if (metadata.isFile()) {
    files.push(path);
    return files;
  }
  for (const entry of readdirSync(path, { withFileTypes: true })) {
    if (entry.isSymbolicLink()) continue;
    if (entry.isDirectory() && excludedDirectories.has(entry.name)) continue;
    if (entry.isFile() &&
        (excludedFiles.has(entry.name) || entry.name.startsWith(".env"))) {
      continue;
    }
    walk(join(path, entry.name), files);
  }
  return files;
}

function gitCandidateFiles() {
  const output = execFileSync(
    "git",
    ["ls-files", "-z", "--cached", "--others", "--exclude-standard"],
    { encoding: "utf8" },
  );
  return output
    .split("\0")
    .filter(Boolean)
    .map((path) => resolve(path))
    .filter((path) =>
      existsSync(path) && !lstatSync(path).isSymbolicLink() &&
      lstatSync(path).isFile());
}

const arguments_ = process.argv.slice(2);
const useGitCandidates = arguments_.includes("--git");
const roots = arguments_.filter((argument) => argument !== "--git");
if (useGitCandidates && roots.length > 0) {
  throw new TypeError("--git cannot be combined with paths");
}
if (!useGitCandidates && roots.length === 0) roots.push(".");

const files = useGitCandidates
  ? gitCandidateFiles()
  : roots.flatMap((root) => walk(resolve(root)));

let findingCount = 0;
for (const file of files) {
  const contents = readFileSync(file);
  if (contents.includes(0)) continue;
  const findings = findSensitiveData(contents.toString("utf8"), {
    includeMetadata: false,
  });
  for (const finding of findings) {
    findingCount++;
    console.error(
      `${file}:${finding.line}: potential ${finding.type}`,
    );
  }
}

if (findingCount > 0) {
  console.error(`Secret scan failed with ${findingCount} finding(s).`);
  process.exitCode = 1;
} else {
  const scope = useGitCandidates
    ? "Git candidate files"
    : roots.map((root) => basename(root)).join(", ");
  console.log(
    `Secret scan passed for ${scope}.`,
  );
}
