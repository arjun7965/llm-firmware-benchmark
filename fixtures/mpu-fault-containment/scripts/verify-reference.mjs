import { spawnSync } from "node:child_process";
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";

const root = fileURLToPath(new URL("../", import.meta.url));
const temporaryRoot = mkdtempSync(join(tmpdir(), "mpu-fault-containment-self-test-"));
const binary = join(temporaryRoot, "public-tests");
function run(command, args) {
  const result = spawnSync(command, args, { cwd: root, stdio: "inherit", timeout: 30000 });
  if (result.error) throw result.error;
  if (result.status !== 0) throw new Error(`${command} exited with status ${result.status}`);
}
try {
  run("cc", ["-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic", "-Istarter", "-Imocks", "reference/mpu_fault_containment.c", "mocks/mock_mpu_fault_containment.c", "tests/public/test_mpu_fault_containment.c", "-o", binary]);
  run(binary, []);
  console.log("MPU fault-containment trusted reference passed.");
} finally {
  rmSync(temporaryRoot, { recursive: true, force: true });
}
