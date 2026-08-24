import { writeFileSync } from "node:fs";
import { parseArgs } from "node:util";
import {
  loadOciImageRecipe,
  validateOciPublication,
} from "../src/oci-image-recipe.mjs";

const { values } = parseArgs({
  strict: true,
  options: {
    digest: { type: "string" },
    output: { type: "string" },
    revision: { type: "string" },
  },
});
if (!values.digest || !values.output || !values.revision) {
  throw new TypeError("--digest, --output, and --revision are required");
}

const { definition } = loadOciImageRecipe();
const publication = {
  schemaVersion: "1.0",
  image: `${definition.imageRepository}@${values.digest}`,
  platformManifestDigest: values.digest,
  platform:
    `${definition.platform.operatingSystem}/${definition.platform.architecture}`,
  base: definition.base.reference,
  source: definition.source,
  sourceRevision: values.revision,
};
validateOciPublication(publication, definition);
writeFileSync(values.output, `${JSON.stringify(publication, null, 2)}\n`, {
  encoding: "utf8",
  flag: "wx",
  mode: 0o600,
});
console.log(`Recorded OCI publication ${publication.image}`);
