import assert from "node:assert/strict";
import {
  mkdirSync,
  mkdtempSync,
  readFileSync,
  readdirSync,
  renameSync,
  rmSync,
  symlinkSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import { canonicalBundleSha256 } from "../src/fixture-answer-digests.mjs";
import {
  extractFencedCode,
  extractFileBundle,
  extractFixtureAnswer,
  writeFileBundle,
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
    suite: "firmware",
    scoringMode: "deterministic",
    targetProfile: "portable-c11",
    validationProfile: "c11-host",
    prompt: examplePrompt,
  }]));
  writeFileSync(join(fixtureRoot, "manifest.json"), JSON.stringify({
    schemaVersion: "1.4",
    taskId: "example-task",
    targetProfile: "portable-c11",
    validationProfile: "c11-host",
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

function bundleFixtureRepository(t) {
  const repository = fixtureRepository(t);
  const manifestPath = join(repository.fixtureRoot, "manifest.json");
  const manifest = JSON.parse(readFileSync(manifestPath, "utf8"));
  manifest.answer = {
    format: "markdown-file-bundle",
    files: [
      { path: "server.go", language: "go" },
      { path: "server_test.go", language: "go" },
    ],
  };
  writeFileSync(manifestPath, JSON.stringify(manifest));
  return repository;
}

function writeResult(root, overrides = {}) {
  const path = join(root, `result-${resultCounter++}.json`);
  writeFileSync(path, JSON.stringify({
    task: "example-task",
    category: "systems-security",
    suite: "firmware",
    scoringMode: "deterministic",
    targetProfile: "portable-c11",
    validationProfile: "c11-host",
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
  ].join("\r\n");

  assert.equal(
    extractFencedCode(answer, { language: "c" }),
    "#include <stdint.h>\n\nint answer(void) { return 42; }\n",
  );
});

test("fenced-code extraction honors marker type and length", () => {
  assert.equal(
    extractFencedCode("````c\n```\n````", { language: "c" }),
    "```\n",
  );
  assert.equal(
    extractFencedCode("~~~c\ncode\n~~~", { language: "c" }),
    "code\n",
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
    () => extractFencedCode("```c\ncode\n```\n```text\nextra\n```", options),
    /additional/u,
  );
  assert.throws(
    () => extractFencedCode("```\nextra\n```\n```c\ncode\n```", options),
    /additional/u,
  );
  assert.throws(
    () => extractFencedCode("~~~c\ncode\n~~~\n```text\nextra\n```", options),
    /additional/u,
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

test("file-bundle extraction enforces declared paths, order, and languages", () => {
  const files = [
    { path: "server.go", language: "go" },
    { path: "server_test.go", language: "go" },
  ];
  const answer = [
    "Implementation and focused tests:",
    "### `server.go`",
    "```go",
    "package shutdown",
    "```",
    "",
    "### `server_test.go`",
    "```go",
    "package shutdown",
    "```",
  ].join("\r\n");
  assert.deepEqual(extractFileBundle(answer, { files }), [
    {
      content: "package shutdown\n",
      language: "go",
      path: "server.go",
    },
    {
      content: "package shutdown\n",
      language: "go",
      path: "server_test.go",
    },
  ]);

  const replace = (from, to) => answer.replace(from, to);
  assert.throws(
    () => extractFileBundle(
      answer.split("### `server_test.go`")[0],
      { files },
    ),
    /missing declared files/u,
  );
  assert.throws(
    () => extractFileBundle(
      replace("server_test.go", "server.go"),
      { files },
    ),
    /duplicate file/u,
  );
  assert.throws(
    () => extractFileBundle(replace("server.go", "other.go"), { files }),
    /undeclared file/u,
  );
  assert.throws(
    () => extractFileBundle(replace("server.go", "../server.go"), { files }),
    /safe relative path/u,
  );
  assert.throws(
    () => extractFileBundle(replace("server.go", "C:/server.go"), { files }),
    /safe relative path/u,
  );
  assert.throws(
    () => extractFileBundle([
      "### `server_test.go`",
      "```go",
      "package shutdown",
      "```",
      "### `server.go`",
      "```go",
      "package shutdown",
      "```",
    ].join("\n"), { files }),
    /manifest order/u,
  );
  assert.throws(
    () => extractFileBundle(replace("```go", "```go extra"), { files }),
    /language label/u,
  );
  assert.throws(
    () => extractFileBundle(`\`\`\`go\npackage shutdown\n\`\`\`\n${answer}`, {
      files,
    }),
    /immediately follow/u,
  );
  assert.throws(
    () => extractFileBundle(answer, { files, fileLimit: 5 }),
    /server.go exceeds 5/u,
  );
  assert.throws(
    () => extractFileBundle(answer, { files, bundleLimit: 20 }),
    /bundle exceeds 20/u,
  );
});

test("canonical bundle digests cover paths, lengths, and content", () => {
  const original = canonicalBundleSha256([
    { path: "generated/a", content: "bc" },
    { path: "generated/b", content: "d" },
  ]);
  assert.match(original, /^[a-f0-9]{64}$/u);
  assert.notEqual(
    original,
    canonicalBundleSha256([
      { path: "generated/a", content: "b" },
      { path: "generated/b", content: "cd" },
    ]),
  );
  assert.notEqual(
    original,
    canonicalBundleSha256([
      { path: "generated/x", content: "bc" },
      { path: "generated/b", content: "d" },
    ]),
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
      resultPath: writeResult(repository.root, { suite: "auxiliary" }),
      fixturesRoot: repository.fixturesRoot,
      tasksPath: repository.tasksPath,
    }),
    /metadata does not match/u,
  );
  assert.throws(
    () => extractFixtureAnswer({
      resultPath: writeResult(repository.root, {
        validationProfile: "stable-rust",
      }),
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

test("rubric-only tasks reject fixture answer extraction", (t) => {
  const repository = fixtureRepository(t);
  const tasks = JSON.parse(readFileSync(repository.tasksPath, "utf8"));
  tasks[0].scoringMode = "rubric-only";
  tasks[0].rubricOnlyReasons = ["undocumented-service"];
  tasks[0].rubricOnlyRationale = "The required service cannot be reproduced.";
  writeFileSync(repository.tasksPath, JSON.stringify(tasks));

  assert.throws(
    () => extractFixtureAnswer({
      resultPath: writeResult(repository.root, {
        scoringMode: "rubric-only",
      }),
      fixturesRoot: repository.fixturesRoot,
      tasksPath: repository.tasksPath,
    }),
    /rubric-only task.*cannot use fixture answer extraction/u,
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

test("fixture extraction writes a complete bundle and canonical summary", (t) => {
  const repository = bundleFixtureRepository(t);
  const resultPath = writeResult(repository.root, {
    stdout: [
      "### `server.go`",
      "```go",
      "package shutdown",
      "```",
      "### `server_test.go`",
      "```go",
      "package shutdown",
      "```",
    ].join("\n"),
  });

  const summary = extractFixtureAnswer({
    resultPath,
    fixturesRoot: repository.fixturesRoot,
    tasksPath: repository.tasksPath,
  });
  assert.equal(summary.format, "markdown-file-bundle");
  assert.deepEqual(
    summary.files.map((file) => file.path),
    ["generated/server.go", "generated/server_test.go"],
  );
  assert.equal(summary.byteLength, 34);
  assert.match(summary.sha256, /^[a-f0-9]{64}$/u);
  for (const file of summary.files) {
    assert.equal(readFileSync(file.outputPath, "utf8"), "package shutdown\n");
    assert.equal(file.byteLength, 17);
    assert.match(file.sha256, /^[a-f0-9]{64}$/u);
  }
});

test("bundle writes restore the previous directory when commit fails", (t) => {
  const repository = bundleFixtureRepository(t);
  const generated = join(repository.fixtureRoot, "generated");
  mkdirSync(generated);
  writeFileSync(join(generated, "server.go"), "old server\n");
  writeFileSync(join(generated, "server_test.go"), "old tests\n");
  let renames = 0;

  assert.throws(
    () => writeFileBundle({
      fixtureRoot: repository.fixtureRoot,
      generatedDirectory: "generated",
      files: [
        { path: "server.go", content: "new server\n" },
        { path: "server_test.go", content: "new tests\n" },
      ],
      overwrite: true,
      renameImpl: (source, destination) => {
        renames++;
        if (renames === 2) throw new Error("injected commit failure");
        renameSync(source, destination);
      },
    }),
    /injected commit failure/u,
  );
  assert.equal(readFileSync(join(generated, "server.go"), "utf8"), "old server\n");
  assert.equal(
    readFileSync(join(generated, "server_test.go"), "utf8"),
    "old tests\n",
  );
  assert.deepEqual(
    readdirSync(repository.fixtureRoot)
      .filter((name) => name.includes(".stage") || name.includes(".backup")),
    [],
  );
});
