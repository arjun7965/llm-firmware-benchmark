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
