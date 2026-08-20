import { createHash } from "node:crypto";
import {
  cpSync,
  existsSync,
  mkdirSync,
  readFileSync,
  readdirSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { parseArgs } from "node:util";

const repositoryRoot = fileURLToPath(new URL("../", import.meta.url));
const siteSource = resolve(repositoryRoot, "site");

function readJson(path) {
  return JSON.parse(readFileSync(path, "utf8"));
}

function fileVersion(path) {
  return createHash("sha256")
    .update(readFileSync(path))
    .digest("hex")
    .slice(0, 12);
}

function plainText(markdown) {
  return markdown
    .replace(/\[([^\]]+)\]\([^\)]+\)/gu, "$1")
    .replace(/[`*]/gu, "")
    .replace(/\s+/gu, " ")
    .trim();
}

function section(markdown, heading) {
  const lines = markdown.split(/\r?\n/u);
  const start = lines.findIndex((line) => line === `## ${heading}`);
  if (start === -1) throw new TypeError(`missing ${heading} section`);
  const relativeEnd = lines.slice(start + 1)
    .findIndex((line) => line.startsWith("## "));
  const end = relativeEnd === -1 ? lines.length : start + 1 + relativeEnd;
  return lines.slice(start + 1, end).join("\n").trim();
}

function taskTitle(markdown, taskId) {
  const match = markdown.match(/^# (.+)$/mu);
  if (!match) throw new TypeError(`missing title in rubric for ${taskId}`);
  return match[1].trim().replace(/ Rubric$/u, "");
}

function scoringCriteria(markdown, taskId) {
  const scoring = section(markdown, "Scoring");
  const matches = [...scoring.matchAll(
    /^- (\d+) points? — (?:\*\*([^:]+):\*\*\s+)?([\s\S]*?)(?=\n\s*\n|^- \d+ points? —|(?![\s\S]))/gmu,
  )];
  const criteria = matches.map((match, index) => ({
    points: Number(match[1]),
    dimension: match[2]?.trim() ?? `Criterion ${index + 1}`,
    evidence: plainText(match[3]),
  }));
  const total = criteria.reduce((sum, criterion) => sum + criterion.points, 0);
  if (criteria.length === 0 || total !== 10) {
    throw new TypeError(`rubric for ${taskId} must expose 10 scoring points`);
  }
  const lastMatch = matches[matches.length - 1];
  const trailing = scoring.slice(lastMatch.index + lastMatch[0].length);
  const note = trailing.trim() ? plainText(trailing) : null;
  return { criteria, note };
}

function repositoryUrl(packageDocument) {
  const server = process.env.GITHUB_SERVER_URL;
  const repository = process.env.GITHUB_REPOSITORY;
  if (server && repository) return `${server}/${repository}`;
  const configured = packageDocument.repository?.url;
  if (typeof configured === "string") {
    return configured.replace(/^git\+/u, "").replace(/\.git$/u, "");
  }
  throw new TypeError("package.json must define repository.url");
}

function buildData() {
  const packageDocument = readJson(resolve(repositoryRoot, "package.json"));
  const tasks = readJson(resolve(repositoryRoot, "tasks.json"));
  const profiles = readJson(
    resolve(repositoryRoot, "validation-profiles.json"),
  );
  const baseUrl = repositoryUrl(packageDocument);
  const siteTasks = tasks.map((task) => {
    const rubricPath = resolve(
      repositoryRoot,
      "docs",
      "benchmarks",
      `${task.id}.md`,
    );
    if (!existsSync(rubricPath)) {
      throw new TypeError(`missing rubric for ${task.id}`);
    }
    const rubric = readFileSync(rubricPath, "utf8");
    const { criteria, note } = scoringCriteria(rubric, task.id);
    return {
      id: task.id,
      title: taskTitle(rubric, task.id),
      category: task.category,
      suite: task.suite,
      scoringMode: task.scoringMode,
      validationProfile: task.validationProfile,
      targetProfile: task.targetProfile ?? null,
      objective: plainText(section(rubric, "Objective")),
      scoring: criteria,
      note,
      rubricUrl: `${baseUrl}/blob/main/docs/benchmarks/${task.id}.md`,
      fixtureUrl: `${baseUrl}/tree/main/fixtures/${task.id}`,
    };
  });
  const activeFixtures = readdirSync(resolve(repositoryRoot, "fixtures"), {
    withFileTypes: true,
  }).filter((entry) => {
    if (!entry.isDirectory()) return false;
    const manifestPath = resolve(
      repositoryRoot,
      "fixtures",
      entry.name,
      "manifest.json",
    );
    return existsSync(manifestPath) && readJson(manifestPath).status === "active";
  }).length;

  return {
    repositoryUrl: baseUrl,
    docsUrl: `${baseUrl}/tree/main/docs`,
    tasksUrl: `${baseUrl}/blob/main/tasks.json`,
    stats: {
      tasks: siteTasks.length,
      firmwareTasks: siteTasks.filter((task) => task.suite === "firmware")
        .length,
      auxiliaryTasks: siteTasks.filter((task) => task.suite === "auxiliary")
        .length,
      activeFixtures,
      validationProfiles: new Set(profiles.profiles.map((profile) => profile.id))
        .size,
    },
    tasks: siteTasks,
  };
}

function build(outputPath) {
  const output = resolve(repositoryRoot, outputPath);
  const containsRepository = !relative(output, repositoryRoot).startsWith("..");
  if (containsRepository) {
    throw new TypeError("site output must not contain the repository");
  }
  const defaultOutput = resolve(repositoryRoot, "site-dist");
  const markerPath = resolve(output, ".site-build-marker");
  if (existsSync(output) && output !== defaultOutput && !existsSync(markerPath)) {
    throw new TypeError("refusing to replace an unrecognized site output");
  }
  rmSync(output, { recursive: true, force: true });
  mkdirSync(output, { recursive: true });
  cpSync(siteSource, output, { recursive: true });

  const indexPath = resolve(output, "index.html");
  const template = readFileSync(indexPath, "utf8");
  const marker = "__BENCHMARK_DATA__";
  if (!template.includes(marker)) {
    throw new TypeError("site template is missing its benchmark data marker");
  }
  const serialized = JSON.stringify(buildData()).replace(/</gu, "\\u003c");
  const versionedTemplate = template
    .replace(
      'href="styles.css"',
      `href="styles.css?v=${fileVersion(resolve(output, "styles.css"))}"`,
    )
    .replace(
      'src="app.js"',
      `src="app.js?v=${fileVersion(resolve(output, "app.js"))}"`,
    );
  writeFileSync(indexPath, versionedTemplate.replace(marker, serialized));
  writeFileSync(resolve(output, ".nojekyll"), "");
  writeFileSync(markerPath, "Generated by scripts/build-pages.mjs.\n");
  console.log(`Built GitHub Pages site at ${output}`);
}

const { values } = parseArgs({
  options: {
    output: { type: "string", short: "o", default: "site-dist" },
  },
});

build(values.output);
