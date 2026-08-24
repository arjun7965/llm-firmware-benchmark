import { loadOciImageRecipe } from "../src/oci-image-recipe.mjs";

const { definition } = loadOciImageRecipe();
console.log(
  `OCI image recipe is valid: ${definition.id} ` +
    `(${definition.platform.operatingSystem}/${definition.platform.architecture})`,
);
