import test from "node:test";
import assert from "node:assert/strict";
import {
  mean,
  populationSd,
  summarizeModelScores,
} from "../src/statistics.mjs";

test("mean and population standard deviation are calculated correctly", () => {
  assert.equal(mean([2, 4, 6]), 4);
  assert.equal(populationSd([2, 4, 6]), Math.sqrt(8 / 3));
});

test("model summaries include totals and per-task variation", () => {
  const scores = {
    tasks: ["a", "b"],
    model: {
      run1: [1, 3],
      run2: [3, 5],
      run3: [5, 7],
    },
  };

  assert.deepEqual(summarizeModelScores(scores, "model", {
    suiteByTask: new Map([
      ["a", "firmware"],
      ["b", "auxiliary"],
    ]),
  }), {
    model: "model",
    totals: [4, 8, 12],
    totalMean: 8,
    totalSd: Math.sqrt(32 / 3),
    totalRange: 8,
    tasks: [
      {
        task: "a",
        values: [1, 3, 5],
        mean: 3,
        sd: Math.sqrt(8 / 3),
        range: 4,
      },
      {
        task: "b",
        values: [3, 5, 7],
        mean: 5,
        sd: Math.sqrt(8 / 3),
        range: 4,
      },
    ],
    suites: [
      {
        suite: "firmware",
        tasks: ["a"],
        totals: [1, 3, 5],
        totalMean: 3,
        totalSd: Math.sqrt(8 / 3),
        totalRange: 4,
      },
      {
        suite: "auxiliary",
        tasks: ["b"],
        totals: [3, 5, 7],
        totalMean: 5,
        totalSd: Math.sqrt(8 / 3),
        totalRange: 4,
      },
    ],
  });
});

test("summaries reject missing and inconsistent score arrays", () => {
  assert.throws(
    () => summarizeModelScores({ tasks: ["a"] }, "missing"),
    /missing scores/,
  );
  assert.throws(
    () => summarizeModelScores({
      tasks: ["a", "b"],
      model: { run1: [1] },
    }, "model"),
    /invalid scores/,
  );
  assert.throws(
    () => summarizeModelScores({
      tasks: ["a"],
      model: { run1: [1] },
    }, "model", {
      suiteByTask: new Map(),
    }),
    /missing suite/,
  );
});
