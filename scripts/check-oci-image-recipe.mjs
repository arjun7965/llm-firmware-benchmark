import { loadOciImageActivation } from "../src/oci-image-recipe.mjs";

const { definition, publication } = loadOciImageActivation();
console.log(
  `OCI image recipe is valid: ${definition.id} ` +
    `(${definition.platform.operatingSystem}/${definition.platform.architecture}, ` +
    `${publication.platformManifestDigest})`,
);
