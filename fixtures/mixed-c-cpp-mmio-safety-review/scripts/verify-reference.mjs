import { spawnSync } from "node:child_process";
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";

const fixtureRoot = fileURLToPath(new URL("../", import.meta.url));
const temporaryRoot = mkdtempSync(join(tmpdir(), "mixed-c-cpp-mmio-self-test-"));
const mockObject = join(temporaryRoot, "mock_mmio_safety.o");
const answerObject = join(temporaryRoot, "answer.o");
const testObject = join(temporaryRoot, "test_mmio_safety_review.o");
const opaqueAccess = join(temporaryRoot, "opaque-access.cpp");
const binary = join(temporaryRoot, "public-tests");

function run(command, args) {
  const result = spawnSync(command, args, {
    cwd: fixtureRoot,
    stdio: "inherit",
    timeout: 30_000,
  });
  if (result.error) throw result.error;
  if (result.status !== 0) throw new Error(`${command} exited with status ${result.status}`);
}

try {
  run("cc", [
    "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic", "-Wcast-qual",
    "-Istarter", "-Imocks", "-c", "mocks/mock_mmio_safety.c", "-o", mockObject,
  ]);
  run("c++", [
    "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic",
    "-Wconversion", "-Wsign-conversion", "-Wold-style-cast", "-Wshadow",
    "-fno-exceptions", "-fno-rtti", "-Istarter", "-c",
    "reference/mmio_safety_review.cpp", "-o", answerObject,
  ]);
  run("c++", [
    "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic",
    "-Wconversion", "-Wsign-conversion", "-Wold-style-cast", "-Wshadow",
    "-fno-exceptions", "-fno-rtti", "-Istarter", "-Imocks", "-c",
    "tests/public/test_mmio_safety_review.cpp", "-o", testObject,
  ]);
  writeFileSync(
    opaqueAccess,
    "#include \"fixture_mmio_safety.h\"\nvoid forbidden(volatile mmio_registers_t *r) { r->status = 0u; }\n",
  );
  const opaqueResult = spawnSync("c++", [
    "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic", "-Istarter",
    "-c", opaqueAccess, "-o", join(temporaryRoot, "opaque-access.o"),
  ], { cwd: fixtureRoot, timeout: 30_000 });
  if (opaqueResult.error) throw opaqueResult.error;
  if (opaqueResult.status === 0) {
    throw new Error("opaque MMIO member access unexpectedly compiled");
  }
  run("c++", [
    answerObject, testObject, mockObject, "-o", binary,
  ]);
  run(binary, []);
  console.log("Mixed C/C++ MMIO safety trusted reference passed.");
} finally {
  rmSync(temporaryRoot, { recursive: true, force: true });
}
