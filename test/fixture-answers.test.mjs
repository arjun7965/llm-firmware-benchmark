import assert from "node:assert/strict";
import {
  mkdirSync,
  mkdtempSync,
  readFileSync,
  readdirSync,
  rmSync,
  symlinkSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import {
  extractFencedCode,
  extractFixtureAnswer,
} from "../src/fixture-answers.mjs";
import { promptSha256 } from "../src/harness.mjs";

let resultCounter = 0;
const examplePrompt = "Return one fenced C block.";

function temporaryDirectory(t) {
  const path = mkdtempSync(join(tmpdir(), "fixture-answer-test-"));
  t.after(() => rmSync(path, { recursive: true, force: true }));
  return path;
}

function fixtureRepository(t) {
  const root = temporaryDirectory(t);
  const fixturesRoot = join(root, "fixtures");
  const fixtureRoot = join(fixturesRoot, "example-task");
  for (const directory of [
    "mocks",
    "reference",
    "scripts",
    "starter",
    "tests/public",
  ]) {
    mkdirSync(join(fixtureRoot, directory), { recursive: true });
  }
  const tasksPath = join(root, "tasks.json");
  writeFileSync(tasksPath, JSON.stringify([{
    id: "example-task",
    category: "systems-security",
    targetProfile: "portable-c11",
    prompt: examplePrompt,
  }]));
  writeFileSync(join(fixtureRoot, "manifest.json"), JSON.stringify({
    schemaVersion: "1.1",
    taskId: "example-task",
    targetProfile: "portable-c11",
    status: "scaffold",
    language: "c11",
    toolVersionArgs: {
      cc: ["--version"],
    },
    answer: {
      format: "markdown-fenced-code",
      language: "c",
      output: "generated/answer.c",
    },
    paths: {
      starter: "starter",
      mocks: "mocks",
      publicTests: "tests/public",
      reference: "reference",
      scripts: "scripts",
      generated: "generated",
      build: "build",
    },
    commands: [{
      id: "host-compile",
      phase: "compile",
      argv: ["cc", "-c", "generated/answer.c"],
      requiredTools: ["cc"],
      timeoutMs: 30_000,
    }],
  }));
  return {
    fixtureRoot,
    fixturesRoot,
    root,
    tasksPath,
  };
}

function writeResult(root, overrides = {}) {
  const path = join(root, `result-${resultCounter++}.json`);
  writeFileSync(path, JSON.stringify({
    task: "example-task",
    category: "systems-security",
    targetProfile: "portable-c11",
    exitCode: 0,
    signal: null,
    error: null,
    promptSha256: promptSha256(examplePrompt),
    stdout: "```c\nint answer(void) { return 42; }\n```\n",
    ...overrides,
  }));
  return path;
}

test("fenced-code extraction preserves code and normalizes line endings", () => {
  const answer = [
    "Explanation",
    "```C",
    "#include <stdint.h>",
    "",
    "int answer(void) { return 42; }",
    "```",
    "```text",
    "ignored",
    "```",
  ].join("\r\n");

  assert.equal(
    extractFencedCode(answer, { language: "c" }),
    "#include <stdint.h>\n\nint answer(void) { return 42; }\n",
  );
});

test("fenced-code extraction rejects malformed or ambiguous answers", () => {
  const options = { language: "c" };
  assert.throws(
    () => extractFencedCode("plain text", options),
    /exactly one/u,
  );
  assert.throws(
    () => extractFencedCode("```c\n```\n", options),
    /empty/u,
  );
  assert.throws(
    () => extractFencedCode("```c\none\n```\n```c\ntwo\n```", options),
    /multiple/u,
  );
  assert.throws(
    () => extractFencedCode("```c extra\ncode\n```", options),
    /only its language label/u,
  );
  assert.throws(
    () => extractFencedCode("```c\ncode", options),
    /unterminated/u,
  );
  assert.throws(
    () => extractFencedCode("```c\nx\0y\n```", options),
    /NUL/u,
  );
  assert.throws(
    () => extractFencedCode("```c\n12345\n```", {
      language: "c",
      codeLimit: 4,
    }),
    /exceeds 4 bytes/u,
  );
});

test("fixture extraction unwraps provider output and writes only code", (t) => {
  const repository = fixtureRepository(t);
  const resultPath = writeResult(repository.root, {
    stdout: JSON.stringify({
      type: "result",
      result: "Before\n```c\nint value = 7;\n```\nAfter",
      session_id: "private",
    }),
  });

  const summary = extractFixtureAnswer({
    resultPath,
    fixturesRoot: repository.fixturesRoot,
    tasksPath: repository.tasksPath,
  });
  const outputPath = join(
    repository.fixtureRoot,
    "generated",
    "answer.c",
  );

  assert.equal(summary.taskId, "example-task");
  assert.equal(summary.language, "c");
  assert.equal(summary.outputPath, outputPath);
  assert.equal(summary.byteLength, 15);
  assert.match(summary.sha256, /^[a-f0-9]{64}$/u);
  assert.equal(readFileSync(outputPath, "utf8"), "int value = 7;\n");
});

test("fixture extraction requires matching success and explicit overwrite", (t) => {
  const repository = fixtureRepository(t);
  assert.throws(
    () => extractFixtureAnswer({
      resultPath: writeResult(repository.root, { exitCode: 1 }),
      fixturesRoot: repository.fixturesRoot,
      tasksPath: repository.tasksPath,
    }),
    /successful provider results/u,
  );
  assert.throws(
    () => extractFixtureAnswer({
      resultPath: writeResult(repository.root, { category: "wrong" }),
      fixturesRoot: repository.fixturesRoot,
      tasksPath: repository.tasksPath,
    }),
    /metadata does not match/u,
  );
  assert.throws(
    () => extractFixtureAnswer({
      resultPath: writeResult(repository.root, {
        promptSha256: "0".repeat(64),
      }),
      fixturesRoot: repository.fixturesRoot,
      tasksPath: repository.tasksPath,
    }),
    /prompt hash does not match/u,
  );

  const firstResult = writeResult(repository.root);
  extractFixtureAnswer({
    resultPath: firstResult,
    fixturesRoot: repository.fixturesRoot,
    tasksPath: repository.tasksPath,
  });
  assert.throws(
    () => extractFixtureAnswer({
      resultPath: firstResult,
      fixturesRoot: repository.fixturesRoot,
      tasksPath: repository.tasksPath,
    }),
    /already exists/u,
  );

  const replacement = writeResult(repository.root, {
    stdout: "```c\nint replacement = 1;\n```",
  });
  extractFixtureAnswer({
    resultPath: replacement,
    fixturesRoot: repository.fixturesRoot,
    tasksPath: repository.tasksPath,
    overwrite: true,
  });
  assert.equal(
    readFileSync(
      join(repository.fixtureRoot, "generated", "answer.c"),
      "utf8",
    ),
    "int replacement = 1;\n",
  );
});

test("fixture extraction rejects symlinked output directories", (t) => {
  const repository = fixtureRepository(t);
  const outside = join(repository.root, "outside");
  mkdirSync(outside);
  symlinkSync(outside, join(repository.fixtureRoot, "generated"), "dir");

  assert.throws(
    () => extractFixtureAnswer({
      resultPath: writeResult(repository.root),
      fixturesRoot: repository.fixturesRoot,
      tasksPath: repository.tasksPath,
    }),
    /output directory is unsafe/u,
  );
  assert.deepEqual(readdirSync(outside), []);
});
