import { execFileSync } from "node:child_process";
import {
  existsSync,
  mkdtempSync,
  readFileSync,
  rmSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import test from "node:test";
import assert from "node:assert/strict";

const repositoryRoot = fileURLToPath(new URL("../", import.meta.url));

test("GitHub Pages build publishes the complete task registry", () => {
  const temporaryRoot = mkdtempSync(join(tmpdir(), "firmware-pages-"));
  const output = join(temporaryRoot, "site");
  try {
    execFileSync(
      process.execPath,
      ["scripts/build-pages.mjs", "--output", output],
      { cwd: repositoryRoot },
    );

    for (const asset of [
      ".nojekyll",
      "app.js",
      "index.html",
      "logo.svg",
      "styles.css",
    ]) {
      assert.equal(existsSync(join(output, asset)), true, `missing ${asset}`);
    }

    const html = readFileSync(join(output, "index.html"), "utf8");
    const css = readFileSync(join(output, "styles.css"), "utf8");
    const app = readFileSync(join(output, "app.js"), "utf8");
    assert.doesNotMatch(html, /__BENCHMARK_DATA__/u);
    assert.match(html, /href="styles\.css\?v=[0-9a-f]{12}"/u);
    assert.match(html, /src="app\.js\?v=[0-9a-f]{12}"/u);
    assert.match(html, /id="tasks"/u);
    assert.match(html, /id="grading"/u);
    assert.match(html, /id="run"/u);
    assert.match(
      html,
      /npm run fixture:validate -- --task embedded-ring-buffer/u,
    );
    assert.doesNotMatch(html, /<code>validate embedded-ring-buffer<\/code>/u);
    assert.match(html, /4 checks captured/u);
    assert.match(html, /RUBRIC SCORE/u);
    assert.match(html, /data-suite="all" aria-pressed="true"/u);
    assert.match(html, /data-suite="firmware" aria-pressed="false"/u);
    assert.match(html, /data-suite="auxiliary" aria-pressed="false"/u);
    assert.match(
      app,
      /aria-label="View scoring for \$\{escapeHtml\(task\.title\)\}"/u,
    );
    assert.match(
      app,
      /setAttribute\("aria-pressed", String\(isActive\)\)/u,
    );
    assert.match(app, /event\.key === "Escape"/u);
    assert.match(app, /toggle\.focus\(\)/u);
    assert.match(html, /--runs 1 --concurrency 2/u);
    assert.doesNotMatch(html, /--runs 1,2,3/u);
    const repeatCommand = html.match(
      /<pre><code>(npm run benchmark:repeats -- \\[^<]+)<\/code><\/pre>/u,
    );
    assert.ok(repeatCommand, "site must publish a repeat command");
    assert.match(repeatCommand[1], /--models gpt-5\.6-luna/u);
    assert.match(repeatCommand[1], /--suites firmware/u);
    assert.match(
      repeatCommand[1],
      /--tasks embedded-ring-buffer/u,
    );
    assert.match(repeatCommand[1], /--tasks firmware-state-machine/u);
    assert.match(repeatCommand[1], /--runs 2,3 --concurrency 2/u);
    assert.doesNotMatch(
      html,
      /<code>npm run benchmark:repeats<\/code>/u,
    );
    assert.match(
      html,
      /cp repeat-scores\.example\.json repeat-scores\.json/u,
    );
    assert.match(html, /record each 0–10 total under its task ID/u);
    assert.match(html, /results\/run-2\//u);
    assert.match(html, /reviewRequired/u);
    assert.doesNotMatch(css, /--coral|#f6f3eb|#ebe8df|#fffdf8/u);
    for (const selector of ["method", "task", "run"]) {
      assert.match(
        css,
        new RegExp(`\\.${selector}-section\\s*\\{\\s*background: var\\(--paper\\);`, "u"),
      );
    }
    assert.match(
      css,
      /\.closing-section\s*\{[^}]*background: var\(--night\);/u,
    );
    assert.match(css, /\.button-primary\s*\{\s*background: var\(--amber\);/u);
    assert.match(
      css,
      /@media \(max-width: 900px\) \{[\s\S]*?\.hero-grid \{[\s\S]*?grid-template-columns: 1fr;/u,
    );
    const dataMatch = html.match(
      /<script type="application\/json" id="benchmark-data">([^<]+)<\/script>/u,
    );
    assert.ok(dataMatch, "built site must embed benchmark data");
    const data = JSON.parse(dataMatch[1]);
    const repositoryTasks = JSON.parse(
      readFileSync(join(repositoryRoot, "tasks.json"), "utf8"),
    );

    assert.equal(data.tasks.length, repositoryTasks.length);
    assert.equal(data.stats.tasks, repositoryTasks.length);
    assert.equal(data.stats.activeFixtures, repositoryTasks.length);
    assert.equal(
      data.stats.firmwareTasks,
      repositoryTasks.filter((task) => task.suite === "firmware").length,
    );
    assert.deepEqual(
      data.tasks.map((task) => task.id),
      repositoryTasks.map((task) => task.id),
    );
    for (const task of data.tasks) {
      assert.equal(
        task.scoring.reduce((total, criterion) => total + criterion.points, 0),
        10,
        `${task.id} must expose a ten-point rubric`,
      );
      assert.match(task.rubricUrl, new RegExp(`${task.id}\\.md$`, "u"));
      assert.match(task.fixtureUrl, new RegExp(`${task.id}$`, "u"));
      if (task.note !== null) {
        assert.match(task.note, /\S/u, `${task.id} note must not be blank`);
      }
    }
    const ringBuffer = data.tasks.find(
      (task) => task.id === "embedded-ring-buffer",
    );
    assert.doesNotMatch(
      ringBuffer.scoring.at(-1).evidence,
      /volatile/u,
      "rubric notes must not be absorbed into the last criterion",
    );
    assert.match(ringBuffer.note, /volatile/u);
  } finally {
    rmSync(temporaryRoot, { recursive: true, force: true });
  }
});

test("Pages workflow builds and deploys only the generated site", () => {
  const workflow = readFileSync(
    join(repositoryRoot, ".github", "workflows", "pages.yml"),
    "utf8",
  );
  assert.match(workflow, /run: npm run site:build/u);
  assert.match(workflow, /path: site-dist/u);
  assert.match(workflow, /pages: write/u);
  assert.match(workflow, /id-token: write/u);
  assert.match(workflow, /uses: actions\/deploy-pages@v4/u);
});
