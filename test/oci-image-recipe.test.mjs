import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";
import {
  loadOciImageActivation,
  loadOciImageRecipe,
  validateOciContainerfile,
  validateOciImageRecipeDefinition,
  validateOciPublication,
  validateOciRuntimeContract,
} from "../src/oci-image-recipe.mjs";

function validRecipe() {
  return structuredClone(loadOciImageRecipe().definition);
}

test("the committed C11 OCI recipe is immutable and least-privileged", () => {
  const { containerfile, definition } = loadOciImageRecipe();
  assert.match(definition.base.reference, /@sha256:[a-f0-9]{64}$/u);
  assert.equal(definition.user.uid, 65_532);
  assert.equal(definition.user.gid, 65_532);
  assert.equal(definition.toolchains[0].version, "14.2.0");
  assert.equal(validateOciContainerfile(containerfile, definition), definition);
});

test("OCI recipe validation rejects mutable inputs and privileged users", () => {
  const { containerfile } = loadOciImageRecipe();
  const taggedBase = validRecipe();
  taggedBase.base.reference = "docker.io/library/gcc:14.2.0-bookworm";
  assert.throws(
    () => validateOciImageRecipeDefinition(taggedBase),
    /platform base is invalid/u,
  );

  const rootUser = validRecipe();
  rootUser.user.uid = 0;
  rootUser.user.gid = 0;
  assert.throws(
    () => validateOciImageRecipeDefinition(rootUser),
    /positive safe integer|UID\/GID 65532/u,
  );

  assert.throws(
    () => validateOciContainerfile(
      `${containerfile}\nRUN apt-get update\n`,
      validRecipe(),
    ),
    /may not fetch, install, add, or copy/u,
  );
  assert.throws(
    () => validateOciContainerfile(
      containerfile.replace("USER 65532:65532", "USER 0:0"),
      validRecipe(),
    ),
    /select UID\/GID 65532/u,
  );
});

test("OCI publication metadata binds recipe, digest, platform, and source", () => {
  const definition = validRecipe();
  const digest = `sha256:${"a".repeat(64)}`;
  const publication = {
    schemaVersion: "1.0",
    image: `${definition.imageRepository}@${digest}`,
    platformManifestDigest: digest,
    platform: "linux/amd64",
    base: definition.base.reference,
    source: definition.source,
    sourceRevision: "b".repeat(40),
  };
  assert.equal(validateOciPublication(publication, definition), publication);
  assert.throws(
    () => validateOciPublication(
      { ...publication, image: `ghcr.io/example/other@${digest}` },
      definition,
    ),
    /does not match its recipe/u,
  );
  assert.throws(
    () => validateOciPublication(
      { ...publication, sourceRevision: "main" },
      definition,
    ),
    /source revision is invalid/u,
  );
});

test("committed OCI activation evidence is internally consistent", () => {
  const activation = loadOciImageActivation();
  assert.equal(
    activation.publication.sourceRevision,
    "44b80f25ad286abbe819dd4287a61e1feaeef14e",
  );
  assert.equal(activation.runtimeContract.sandbox.runtime.version, "4.9.3");
  assert.equal(
    activation.runtimeContract.sandbox.configurationSha256,
    "132e8108e8ba944864fdfb8823ec3c7ed586ace3300b7573dfd7326f4235a3b7",
  );
});

test("OCI runtime contracts reject drift in runtime security inputs", () => {
  const { runtimeContract } = loadOciImageActivation();
  const differentConfiguration = structuredClone(runtimeContract);
  differentConfiguration.sandbox.configurationSha256 = "0".repeat(64);
  assert.throws(
    () => validateOciRuntimeContract(differentConfiguration),
    /configuration fingerprint is invalid/u,
  );

  const differentLimiter = structuredClone(runtimeContract);
  differentLimiter.sandbox.limiter.version = "5.0.0";
  assert.throws(
    () => validateOciRuntimeContract(differentLimiter),
    /runtime and limiter contracts must match/u,
  );

  const untrustedSeccomp = structuredClone(runtimeContract);
  untrustedSeccomp.sandbox.seccompProfile.path = "/tmp/seccomp.json";
  assert.throws(
    () => validateOciRuntimeContract(untrustedSeccomp),
    /seccomp profile path is invalid/u,
  );
});

test("OCI workflow gates publication and calibrates the registered digest", () => {
  const workflow = readFileSync(
    new URL("../.github/workflows/oci-c11.yml", import.meta.url),
    "utf8",
  );
  assert.match(
    workflow,
    /head\.repo\.full_name == github\.repository/u,
  );
  assert.match(workflow, /publish-oci-c11/u);
  assert.match(workflow, /packages: write/u);
  assert.match(workflow, /podman push --digestfile/u);
  assert.match(workflow, /skopeo inspect --format '\{\{\.Digest\}\}'/u);
  assert.match(workflow, /test "\$\{pushed_digest\}" = "\$\{resolved_digest\}"/u);
  assert.match(workflow, /podman=4\.9\.3\+ds1-1ubuntu0\.2/u);
  assert.match(workflow, /cmp oci\/c11\/runtime-contract\.json/u);
  assert.match(workflow, /npm run oci:c11:calibrate/u);
  const calibrationJob = workflow.slice(workflow.indexOf("  calibrate:"));
  assert.doesNotMatch(calibrationJob, /login ghcr\.io/u);
});
