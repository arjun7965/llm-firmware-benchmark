import test from "node:test";
import assert from "node:assert/strict";
import {
  loadModels,
  validateModels,
} from "../src/models.mjs";

const validModel = {
  id: "model-a",
  provider: "ncode",
  model: "provider/model-a",
  options: {
    effort: "medium",
  },
};

test("example model configuration is valid", () => {
  const models = loadModels(new URL("../models.example.json", import.meta.url));

  assert.equal(models.length, 6);
  assert.equal(models[0].provider, "ncode");
  assert.deepEqual(
    models.slice(2, 5).map(({ id, provider, model }) => ({
      id,
      provider,
      model,
    })),
    [
      {
        id: "gpt-5.6-luna",
        provider: "codex",
        model: "gpt-5.6-luna",
      },
      {
        id: "gpt-5.6-sol",
        provider: "codex",
        model: "gpt-5.6-sol",
      },
      {
        id: "gpt-5.6-terra",
        provider: "codex",
        model: "gpt-5.6-terra",
      },
    ],
  );
  assert.equal(models[5].provider, "openai-compatible");
});

test("model validation rejects unsafe and duplicate IDs", () => {
  assert.throws(
    () => validateModels([{ ...validModel, id: "../model" }]),
    /invalid id/,
  );
  assert.throws(
    () => validateModels([validModel, { ...validModel }]),
    /duplicate model id/,
  );
});

test("model validation requires provider, model, and object options", () => {
  assert.throws(
    () => validateModels([{ ...validModel, provider: "Bad Provider" }]),
    /invalid provider/,
  );
  assert.throws(
    () => validateModels([{ ...validModel, model: " " }]),
    /model identifier/,
  );
  assert.throws(
    () => validateModels([{ ...validModel, options: [] }]),
    /options must be an object/,
  );
});
