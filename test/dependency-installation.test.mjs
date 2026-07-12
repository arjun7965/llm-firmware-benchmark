import assert from "node:assert/strict";
import {
  chmodSync,
  mkdirSync,
  mkdtempSync,
  rmSync,
  symlinkSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import { dependencyInstallationFingerprint } from
  "../src/dependency-installation.mjs";

function installation(t) {
  const root = mkdtempSync(join(tmpdir(), "dependency-installation-test-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const packageRoot = join(root, "example-package");
  mkdirSync(packageRoot);
  writeFileSync(
    join(packageRoot, "package.json"),
    "{\"name\":\"example-package\",\"version\":\"1.0.0\"}\n",
  );
  chmodSync(root, 0o700);
  chmodSync(packageRoot, 0o755);
  chmodSync(join(packageRoot, "package.json"), 0o644);
  return { packageRoot, root };
}

test("dependency installation fingerprints are deterministic and complete", (t) => {
  const { packageRoot, root } = installation(t);
  const options = { requiredUid: process.getuid() };
  const original = dependencyInstallationFingerprint(root, options);

  assert.match(original, /^[a-f0-9]{64}$/u);
  assert.equal(dependencyInstallationFingerprint(root, options), original);

  writeFileSync(
    join(packageRoot, "package.json"),
    "{\"name\":\"example-package\",\"version\":\"1.0.1\"}\n",
  );
  assert.notEqual(dependencyInstallationFingerprint(root, options), original);
});

test("dependency installation attestation rejects unsafe entries", (t) => {
  const { packageRoot, root } = installation(t);
  const options = { requiredUid: process.getuid() };

  symlinkSync("package.json", join(packageRoot, "linked-package.json"));
  assert.throws(
    () => dependencyInstallationFingerprint(root, options),
    /unsupported type/u,
  );
  rmSync(join(packageRoot, "linked-package.json"));

  chmodSync(packageRoot, 0o775);
  assert.throws(
    () => dependencyInstallationFingerprint(root, options),
    /approved user and non-writable/u,
  );
});
