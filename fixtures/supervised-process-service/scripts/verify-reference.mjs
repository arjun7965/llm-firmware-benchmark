import { spawnSync } from "node:child_process";
import {
  mkdtempSync,
  readFileSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";

const fixtureRoot = fileURLToPath(new URL("../", import.meta.url));
const temporaryRoot = mkdtempSync(
  join(tmpdir(), "supervised-process-service-self-test-"),
);
const object = join(temporaryRoot, "answer.o");
const binary = join(temporaryRoot, "public-tests");

function run(command, args) {
  const result = spawnSync(command, args, {
    cwd: fixtureRoot,
    stdio: "inherit",
    timeout: 30_000,
  });
  if (result.error) throw result.error;
  if (result.status !== 0) {
    throw new Error(`${command} exited with status ${result.status}`);
  }
}

function verify(sourcePath) {
  run("cc", [
    "-D_GNU_SOURCE",
    "-std=c11",
    "-Wall",
    "-Wextra",
    "-Werror",
    "-pedantic",
    "-Istarter",
    "-Imocks",
    "-include",
    "mocks/redirect_posix.h",
    "-c",
    sourcePath,
    "-o",
    object,
  ]);
  run("cc", [
    "-D_GNU_SOURCE",
    "-std=c11",
    "-Wall",
    "-Wextra",
    "-Werror",
    "-pedantic",
    "-Istarter",
    "-Imocks",
    object,
    "mocks/mock_supervisor_os.c",
    "tests/public/test_supervised_process_service.c",
    "-o",
    binary,
  ]);
  run(binary, []);
}

function replaceExactlyOnce(source, find, replacement) {
  if (source.split(find).length !== 2) {
    throw new Error("Reference transformation must match exactly once");
  }
  return source.replace(find, replacement);
}

try {
  const referencePath = join(fixtureRoot, "reference/supervised_process_service.c");
  const source = readFileSync(referencePath, "utf8");
  const pipeCleanup = "  close_if_open(&wake_pipe[1]);\n  close_if_open(&wake_pipe[0]);";
  const handlerCleanup = [
    "  if (terminate_installed) {",
    "    (void)sigaction(SIGTERM, &old_terminate_action, NULL);",
    "  }",
    "  if (interrupt_installed) {",
    "    (void)sigaction(SIGINT, &old_interrupt_action, NULL);",
    "  }",
  ].join("\n");
  const reversedHandlers = handlerCleanup.split("\n").slice(3)
    .concat(handlerCleanup.split("\n").slice(0, 3)).join("\n");
  const reorderedPipe = replaceExactlyOnce(source, pipeCleanup,
    "  close_if_open(&wake_pipe[0]);\n  close_if_open(&wake_pipe[1]);");
  const withProbe = replaceExactlyOnce(source,
    "  if (!child_ready) {\n    if (kill(pid, SIGTERM)",
    "  if (!child_ready) child_ready = poll_pidfd(pidfd, 0) > 0;\n" +
    "  if (!child_ready) {\n    if (kill(pid, SIGTERM)");
  function reorderPolls(input) {
    let output = replaceExactlyOnce(input,
      "  const int result = poll(descriptors, 3u, timeout_ms);",
      "  const struct pollfd temporary = descriptors[0];\n" +
      "  descriptors[0] = descriptors[2];\n" +
      "  descriptors[2] = temporary;\n" +
      "  const int result = poll(descriptors, 3u, timeout_ms);");
    output = replaceExactlyOnce(output,
      "events->wake_events = descriptors[0].revents;",
      "events->wake_events = descriptors[2].revents;");
    return replaceExactlyOnce(output,
      "events->channel_events = descriptors[2].revents;",
      "events->channel_events = descriptors[0].revents;");
  }
  const variants = [
    ["reference", source],
    ["pipe-close-order", reorderedPipe],
    ["handler-restore-order", replaceExactlyOnce(source, handlerCleanup, reversedHandlers)],
    ["combined-cleanup-order", replaceExactlyOnce(reorderedPipe, handlerCleanup, reversedHandlers)],
    ["zero-time-pidfd-probe", withProbe],
    ["poll-descriptor-order", reorderPolls(source)],
    ["probe-and-poll-order", reorderPolls(withProbe)],
  ];
  for (const [name, contents] of variants) {
    const candidatePath = join(temporaryRoot, `${name}.c`);
    writeFileSync(candidatePath, contents);
    verify(candidatePath);
    console.log(`Supervised process service ${name} passed.`);
  }
} finally {
  rmSync(temporaryRoot, { recursive: true, force: true });
}
