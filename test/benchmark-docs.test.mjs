import { existsSync, readFileSync } from "node:fs";
import test from "node:test";
import assert from "node:assert/strict";
import { loadTasks } from "../src/harness.mjs";

test("every benchmark task has a ten-point scoring document", () => {
  const tasks = loadTasks(new URL("../tasks.json", import.meta.url));

  for (const task of tasks) {
    const url = new URL(
      `../docs/benchmarks/${task.id}.md`,
      import.meta.url,
    );
    assert.equal(existsSync(url), true, `missing rubric for ${task.id}`);
    const contents = readFileSync(url, "utf8");
    assert.match(contents, /^# /);
    assert.match(contents, /^## Objective$/m);
    assert.match(contents, /^## Scoring$/m);
    const points = [...contents.matchAll(/^- (\d+) points? —/gm)]
      .map((match) => Number(match[1]));
    assert.equal(
      points.reduce((total, value) => total + value, 0),
      10,
      `rubric for ${task.id} must total 10 points`,
    );
  }
});

test("every benchmark task has documented validation dependencies", () => {
  const tasks = loadTasks(new URL("../tasks.json", import.meta.url));
  const dependencies = readFileSync(
    new URL("../docs/dependencies.md", import.meta.url),
    "utf8",
  );

  for (const task of tasks) {
    assert.ok(
      dependencies.includes(`| \`${task.id}\` |`),
      `missing dependencies for ${task.id}`,
    );
  }
});

test("every task has a documented answer-contract decision", () => {
  const tasks = loadTasks(new URL("../tasks.json", import.meta.url));
  const contracts = readFileSync(
    new URL("../docs/answer-contracts.md", import.meta.url),
    "utf8",
  );

  assert.match(contracts, /^## Decision$/mu);
  assert.match(contracts, /^## Multi-File Contract$/mu);
  for (const task of tasks) {
    const marker = `| \`${task.id}\` |`;
    assert.equal(
      contracts.split(marker).length - 1,
      1,
      `answer-contract decision for ${task.id} must appear exactly once`,
    );
  }
  assert.match(
    contracts,
    /\| `go-graceful-shutdown` \| Multi-file \| Multi-file \|/u,
    "Go server and *_test.go outputs must retain a multi-file contract",
  );
});

test("fixture-backed prompts match their declared answer contract", () => {
  const tasks = loadTasks(new URL("../tasks.json", import.meta.url));

  for (const task of tasks) {
    const manifestUrl = new URL(
      `../fixtures/${task.id}/manifest.json`,
      import.meta.url,
    );
    if (!existsSync(manifestUrl)) continue;
    const manifest = JSON.parse(readFileSync(manifestUrl, "utf8"));
    if (manifest.status !== "active") continue;
    if (manifest.answer.format === "markdown-file-bundle") {
      assert.equal(task.id, "go-graceful-shutdown");
      for (const file of manifest.answer.files) {
        assert.ok(
          task.prompt.includes("### `" + file.path + "`"),
          `fixture ${task.id} prompt must request ${file.path}`,
        );
      }
    } else {
      assert.equal(manifest.answer.format, "markdown-fenced-code");
      assert.match(
        task.prompt,
        /Return one fenced\b/u,
        `fixture ${task.id} prompt must request one fenced implementation`,
      );
    }
  }
});
