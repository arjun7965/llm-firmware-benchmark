import { parseArgs } from "node:util";
import {
  loadOciImageActivation,
  loadOciImageRecipe,
} from "../src/oci-image-recipe.mjs";

const { values } = parseArgs({
  strict: true,
  options: { activation: { type: "boolean", default: false } },
});
const { definition, publication = null } = values.activation
  ? loadOciImageActivation()
  : loadOciImageRecipe();
const evidence = publication === null
  ? "recipe only"
  : publication.platformManifestDigest;
console.log(
  `OCI image recipe is valid: ${definition.id} ` +
    `(${definition.platform.operatingSystem}/${definition.platform.architecture}, ` +
    `${evidence})`,
);
