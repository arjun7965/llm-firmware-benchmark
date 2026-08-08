import { spawnSync } from "node:child_process";
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";

const root = fileURLToPath(new URL("../", import.meta.url));
const temporaryRoot = mkdtempSync(join(tmpdir(), "secure-maintenance-self-test-"));
const binary = join(temporaryRoot, "public-tests");
function run(command, args) {
  const result = spawnSync(command, args, { cwd: root, stdio: "inherit", timeout: 30000 });
  if (result.error) throw result.error;
  if (result.status !== 0) throw new Error(`${command} exited with status ${result.status}`);
}
try {
  run("cc", ["-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic", "-Istarter", "-Imocks", "reference/secure_maintenance_command.c", "mocks/mock_secure_maintenance.c", "tests/public/test_secure_maintenance_command.c", "-o", binary]);
  run(binary, []);
  console.log("Secure maintenance trusted reference passed.");
} finally {
  rmSync(temporaryRoot, { recursive: true, force: true });
}
