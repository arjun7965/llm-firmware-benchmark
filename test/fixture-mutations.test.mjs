import {
  chmodSync,
  mkdirSync,
  mkdtempSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import assert from "node:assert/strict";
import {
  createMutationCommandPlan,
  fixtureMutationCommands,
  runFixtureMutationTests,
} from "../src/fixture-mutations.mjs";

function temporaryDirectory(t) {
  const path = mkdtempSync(join(tmpdir(), "fixture-mutations-test-"));
  t.after(() => rmSync(path, { recursive: true, force: true }));
  return path;
}

function manifestFor(commands) {
  return {
    taskId: "rust-stream-decoder",
    language: "rust",
    answer: {
      output: "generated/lib.rs",
    },
    paths: {
      build: "build",
    },
    commands,
  };
}

function writeJson(path, value) {
  writeFileSync(path, JSON.stringify(value, null, 2));
}

function makeDirectory(path) {
  mkdirSync(path, { recursive: true });
}

function writeExecutable(path, content) {
  writeFileSync(path, content, { mode: 0o700 });
  chmodSync(path, 0o700);
}

test("mutation command planning rewrites non-C source and test binary paths", (t) => {
  const candidateRoot = temporaryDirectory(t);
  const candidatePath = join(candidateRoot, "lib.rs");
  const manifest = manifestFor([
    {
      phase: "compile",
      argv: [
        "rustc",
        "--edition=2021",
        "--test",
        "generated/lib.rs",
        "-o",
        "build/public-tests",
      ],
      timeoutMs: 30000,
    },
    {
      phase: "test",
      argv: ["build/public-tests", "--nocapture"],
      timeoutMs: 5000,
    },
  ]);
  const commands = fixtureMutationCommands(manifest);
  const plan = createMutationCommandPlan({
    candidatePath,
    candidateRoot,
    commands,
    manifest,
  });

  assert.equal(plan.compile.command, "rustc");
  assert.deepEqual(plan.compile.args, [
    "--edition=2021",
    "--test",
    candidatePath,
    "-o",
    join(candidateRoot, "build/public-tests"),
  ]);
  assert.equal(plan.test.command, join(candidateRoot, "build/public-tests"));
  assert.deepEqual(plan.test.args, ["--nocapture"]);
});

test("mutation command planning supports tool-backed test commands", (t) => {
  const candidateRoot = temporaryDirectory(t);
  const candidatePath = join(candidateRoot, "main.go");
  const manifest = manifestFor([
    {
      phase: "compile",
      argv: [
        "go",
        "test",
        "-c",
        "-o",
        "build/public-tests",
        "generated/lib.rs",
      ],
      timeoutMs: 30000,
    },
    {
      phase: "test",
      argv: ["go", "tool", "test2json", "build/public-tests"],
      timeoutMs: 5000,
    },
  ]);
  const commands = fixtureMutationCommands(manifest);
  const plan = createMutationCommandPlan({
    candidatePath,
    candidateRoot,
    commands,
    manifest,
  });

  assert.equal(plan.test.command, "go");
  assert.deepEqual(plan.test.args, [
    "tool",
    "test2json",
    join(candidateRoot, "build/public-tests"),
  ]);
});

test("mutation command planning requires an adaptable compile source", (t) => {
  const candidateRoot = temporaryDirectory(t);
  const manifest = manifestFor([
    {
      phase: "compile",
      argv: ["rustc", "--test", "reference/lib.rs", "-o", "build/tests"],
      timeoutMs: 30000,
    },
    {
      phase: "test",
      argv: ["build/tests"],
      timeoutMs: 5000,
    },
  ]);

  assert.throws(
    () => createMutationCommandPlan({
      candidatePath: join(candidateRoot, "lib.rs"),
      candidateRoot,
      commands: fixtureMutationCommands(manifest),
      manifest,
    }),
    /compile command cannot be adapted/u,
  );
});

test("mutation execution handles a valid baseline mutation id", (t) => {
  const root = temporaryDirectory(t);
  const binRoot = join(root, "bin");
  const fixturesRoot = join(root, "fixtures");
  const fixtureRoot = join(fixturesRoot, "baseline-collision");
  for (const directory of [
    binRoot,
    join(fixtureRoot, "starter"),
    join(fixtureRoot, "mocks"),
    join(fixtureRoot, "tests/public"),
    join(fixtureRoot, "reference"),
    join(fixtureRoot, "scripts"),
  ]) {
    makeDirectory(directory);
  }

  writeExecutable(
    join(binRoot, "cc"),
    `#!/usr/bin/env node
import { chmodSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname } from "node:path";

const args = process.argv.slice(2);
const outputIndex = args.indexOf("-o");
if (outputIndex === -1 || outputIndex + 1 >= args.length) process.exit(2);
const output = args[outputIndex + 1];
const source = args.find((arg) => arg.endsWith(".c"));
if (!source) process.exit(2);
const code = readFileSync(source, "utf8");
mkdirSync(dirname(output), { recursive: true });
writeFileSync(
  output,
  "#!/usr/bin/env node\\nprocess.exit(" +
    (code.includes("return 1;") ? "0" : "1") +
    ");\\n",
);
chmodSync(output, 0o700);
`,
  );

  writeJson(join(root, "tasks.json"), [{
    id: "baseline-collision",
    category: "embedded",
    suite: "firmware",
    validationProfile: "c11-host",
    targetProfile: "portable-c11",
    prompt: "Implement value.",
  }]);
  writeJson(join(fixtureRoot, "manifest.json"), {
    schemaVersion: "1.3",
    taskId: "baseline-collision",
    targetProfile: "portable-c11",
    validationProfile: "c11-host",
    status: "active",
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
    commands: [
      {
        id: "host-compile",
        phase: "compile",
        argv: [
          "cc",
          "generated/answer.c",
          "tests/public/test_value.c",
          "-o",
          "build/public-tests",
        ],
        requiredTools: ["cc"],
        timeoutMs: 30000,
      },
      {
        id: "public-tests",
        phase: "test",
        argv: ["build/public-tests"],
        requiredTools: [],
        timeoutMs: 5000,
      },
    ],
  });
  writeJson(join(fixtureRoot, "mutations.json"), {
    schemaVersion: "1.2",
    source: "reference/answer.c",
    mutations: [{
      id: "baseline",
      description: "Return the wrong value.",
      find: "return 1;",
      replace: "return 0;",
    }],
  });
  writeFileSync(join(fixtureRoot, "reference/answer.c"), [
    "int value(void) {",
    "  return 1;",
    "}",
    "",
  ].join("\n"));
  writeFileSync(join(fixtureRoot, "tests/public/test_value.c"), [
    "int value(void);",
    "int main(void) {",
    "  return value() == 1 ? 0 : 1;",
    "}",
    "",
  ].join("\n"));

  const previousPath = process.env.PATH;
  process.env.PATH = `${binRoot}:${previousPath}`;
  t.after(() => {
    process.env.PATH = previousPath;
  });

  assert.deepEqual(
    runFixtureMutationTests({
      fixturesRoot,
      logger: () => {},
      tasksPath: join(root, "tasks.json"),
      temporaryRoot: join(root, "tmp"),
    }),
    {
      fixtureCount: 1,
      killedMutations: 1,
    },
  );
});
