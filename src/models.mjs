import { readFileSync } from "node:fs";

const modelIdPattern = /^[a-z0-9]+(?:[._-][a-z0-9]+)*$/;
const providerPattern = /^[a-z0-9]+(?:[._-][a-z0-9]+)*$/;

export function validateModels(models) {
  if (!Array.isArray(models) || models.length === 0) {
    throw new TypeError("models must be a non-empty array");
  }

  const ids = new Set();
  for (const [index, model] of models.entries()) {
    if (!model || typeof model !== "object" || Array.isArray(model)) {
      throw new TypeError(`model at index ${index} must be an object`);
    }
    if (typeof model.id !== "string" || !modelIdPattern.test(model.id)) {
      throw new TypeError(`model at index ${index} has an invalid id`);
    }
    if (ids.has(model.id)) {
      throw new TypeError(`duplicate model id: ${model.id}`);
    }
    if (typeof model.provider !== "string" ||
        !providerPattern.test(model.provider)) {
      throw new TypeError(`model ${model.id} has an invalid provider`);
    }
    if (typeof model.model !== "string" || model.model.trim() === "") {
      throw new TypeError(`model ${model.id} must have a model identifier`);
    }
    if (model.options !== undefined &&
        (!model.options || typeof model.options !== "object" ||
         Array.isArray(model.options))) {
      throw new TypeError(`model ${model.id} options must be an object`);
    }
    ids.add(model.id);
  }

  return models;
}

export function loadModels(path) {
  const document = JSON.parse(readFileSync(path, "utf8"));
  return validateModels(document.models);
}
