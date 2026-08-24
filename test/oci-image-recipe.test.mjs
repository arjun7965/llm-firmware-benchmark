import assert from "node:assert/strict";
import test from "node:test";
import {
  loadOciImageRecipe,
  validateOciContainerfile,
  validateOciImageRecipeDefinition,
  validateOciPublication,
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
