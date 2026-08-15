import { readFileSync } from "node:fs";
import test from "node:test";
import assert from "node:assert/strict";
import {
  loadTasks,
  validateTasks,
} from "../src/harness.mjs";
import { scoringModeIds } from "../src/scoring-modes.mjs";
import { validationProfileIds } from "../src/validation-profiles.mjs";

test("repository tasks are valid and cover distinct categories", () => {
  const tasks = loadTasks(new URL("../tasks.json", import.meta.url));

  assert.ok(tasks.length >= 12);
  assert.equal(new Set(tasks.map((task) => task.id)).size, tasks.length);
  assert.ok(new Set(tasks.map((task) => task.category)).size >= 8);
  assert.equal(tasks.filter((task) => task.suite === "firmware").length, 42);
  assert.equal(tasks.filter((task) => task.suite === "auxiliary").length, 9);
  assert.deepEqual(
    [...new Set(tasks.map((task) => task.validationProfile))].sort(),
    validationProfileIds,
  );
  assert.deepEqual(
    [...new Set(tasks.map((task) => task.scoringMode))].sort(),
    ["deterministic"],
  );
  const profileDocumentation = readFileSync(
    new URL("../docs/validation-profiles.md", import.meta.url),
    "utf8",
  );
  for (const task of tasks) {
    assert.match(profileDocumentation, new RegExp(`\\\`${task.id}\\\``));
  }
});

test("task validation rejects duplicate and malformed IDs", () => {
  const valid = {
    id: "valid-task",
    category: "test",
    suite: "auxiliary",
    scoringMode: "deterministic",
    validationProfile: "python3-stdlib",
    prompt: "Do work.",
  };

  assert.throws(
    () => validateTasks([valid, { ...valid }]),
    /duplicate task id/,
  );
  assert.throws(
    () => validateTasks([{ ...valid, id: "Invalid_Task" }]),
    /invalid id/,
  );
});

test("task validation requires nonempty categories and prompts", () => {
  assert.throws(
    () => validateTasks([{ id: "missing-category", category: " ", prompt: "x" }]),
    /must have a category/,
  );
  assert.throws(
    () => validateTasks([{ id: "missing-prompt", category: "test", prompt: "" }]),
    /must have a prompt/,
  );
  assert.throws(
    () => validateTasks([{
      id: "missing-suite",
      category: "test",
      scoringMode: "deterministic",
      validationProfile: "python3-stdlib",
      prompt: "x",
    }]),
    /suite/,
  );
  assert.throws(
    () => validateTasks([{
      id: "invalid-suite",
      category: "test",
      suite: "primary",
      scoringMode: "deterministic",
      validationProfile: "python3-stdlib",
      prompt: "x",
    }]),
    /suite/,
  );
  assert.throws(
    () => validateTasks([{
      id: "extra-field",
      category: "test",
      suite: "auxiliary",
      scoringMode: "deterministic",
      validationProfile: "python3-stdlib",
      prompt: "x",
      answer: "not part of the task contract",
    }]),
    /unexpected fields/,
  );
});

test("embedded tasks require a known target profile", () => {
  const embeddedTask = {
    id: "embedded-task",
    category: "embedded",
    suite: "firmware",
    scoringMode: "deterministic",
    validationProfile: "c11-host",
    prompt: "Implement firmware.",
  };

  assert.throws(
    () => validateTasks([embeddedTask]),
    /must have a targetProfile/,
  );
  assert.throws(
    () => validateTasks([{
      ...embeddedTask,
      targetProfile: "unknown-profile",
    }]),
    /invalid targetProfile/,
  );
  assert.equal(
    validateTasks([{
      ...embeddedTask,
      targetProfile: "armv7m-bare-metal",
    }])[0].targetProfile,
    "armv7m-bare-metal",
  );
  assert.throws(
    () => validateTasks([{
      id: "auxiliary-profile",
      category: "test",
      suite: "auxiliary",
      scoringMode: "deterministic",
      validationProfile: "python3-stdlib",
      targetProfile: "armv7m-bare-metal",
      prompt: "Run a hosted test.",
    }]),
    /auxiliary task.*cannot have a targetProfile/,
  );
  assert.throws(
    () => validateTasks([{
      ...embeddedTask,
      suite: "auxiliary",
    }]),
    /must have a targetProfile|category must use the firmware suite/,
  );
});

test("tasks require a known validation profile", () => {
  const task = {
    id: "hosted-task",
    category: "test",
    suite: "auxiliary",
    scoringMode: "deterministic",
    prompt: "Run a hosted test.",
  };

  assert.throws(
    () => validateTasks([task]),
    /validationProfile/,
  );
  assert.throws(
    () => validateTasks([{
      ...task,
      validationProfile: "unknown-profile",
    }]),
    /validationProfile/,
  );
  assert.equal(
    validateTasks([{
      ...task,
      validationProfile: "python3-stdlib",
    }])[0].validationProfile,
    "python3-stdlib",
  );
});

test("mixed C/C++ MMIO review prompt is a self-contained fixture contract", () => {
  const task = loadTasks(new URL("../tasks.json", import.meta.url))
    .find((item) => item.id === "mixed-c-cpp-mmio-safety-review");
  assert.ok(task);
  for (const requiredText of [
    "Providers receive this prompt, not fixture files.",
    "typedef struct mmio_registers mmio_registers_t",
    "MMIO_STATUS_TERMINAL",
    "mmio_write_transfer_count",
    "struct mmio_review_findings_t",
    "mmio_transfer_t &operator=(mmio_transfer_t &&other) noexcept",
    "private: volatile mmio_registers_t *registers_; std::uint16_t count_; bool active_;",
    "legacy_owner_t(const legacy_owner_t &) = default",
    "reinterpret_cast<volatile std::uint32_t *>(bytes)",
    "legacy_mmio.h line 6 begins typedef struct",
    "dangling local-owner reference line 17",
    "layout, host-pointer-width, or long assumption line 25",
    "Diagnosis is deliberately limited to exactly seven fields/findings",
    "checked static_cast<std::uint16_t>(count) is required and permitted",
    "namespace-scope immutable constants are allowed",
    "Invalid arguments take precedence over busy",
    "at most one live active mmio_transfer_t owns a given opaque MMIO handle",
    "move assignment between distinct active owners requires distinct handles",
  ]) {
    assert.ok(
      task.prompt.includes(requiredText),
      "prompt omits " + requiredText,
    );
  }
});

test("fixed-point filter prompt is a self-contained fixture contract", () => {
  const task = loadTasks(new URL("../tasks.json", import.meta.url))
    .find((item) => item.id === "fixed-point-filter-optimization");
  assert.ok(task);
  for (const requiredText of [
    "fixed_point_filter_t { int16_t history[6]; bool initialized; }",
    "The complete opaque cost boundary is:",
    "filter_cost_begin_step(filter_cost_model_t *,uint32_t mac_count)",
    "filter_cost_mac_q15(filter_cost_model_t *,int64_t *accumulator",
    "History index zero is x[n-1] and index five is x[n-6]",
    "call begin_step exactly once declaring four MACs",
    "call mac_q15 exactly four times in this order",
    "Pair sums use int32_t and the accumulator is int64_t",
    "do not multiply samples and coefficients directly or issue dummy MACs",
    "Only after a successful commit shift history toward index five",
    "one final nearest rounding",
    "fence label is exactly `c`",
  ]) {
    assert.ok(task.prompt.includes(requiredText), "prompt omits " + requiredText);
  }
});

test("firmware security prompts are self-contained fixture contracts", () => {
  const tasks = new Map(
    loadTasks(new URL("../tasks.json", import.meta.url))
      .map((task) => [task.id, task]),
  );
  const requiredText = {
    "secure-maintenance-command": [
      "The complete public API is:",
      "typedef struct sec0_handle sec0_handle_t",
      "sec0_verify_debug_response",
      "sec0_verify_update_authorization",
      "The exact debug frame is 16 bytes",
      "The exact update frame is 24 bytes",
      "process does not reread lifecycle or physical presence",
      "write the debug gate LOCKED, write the update gate LOCKED",
      "Never access, cast, copy, expose, derive, or implement a device secret",
    ],
    "mpu-fault-containment": [
      "The complete public API is:",
      "Include stdbool.h, stddef.h, and stdint.h",
      "typedef struct mpu0_registers mpu0_registers_t",
      "mpu0_region_config_t { uint32_t base; uint32_t size;",
      "The only permitted accessor signatures are",
      "programs exactly four regions in index and priority order",
      "Immediately after each region program, sample fault status exactly once",
      "give nonzero fault bits precedence",
      "secctl_contain_configuration exactly once",
      "Recovery requires reboot or reinitialization",
    ],
  };

  for (const [taskId, fragments] of Object.entries(requiredText)) {
    const task = tasks.get(taskId);
    assert.ok(task, `missing task: ${taskId}`);
    for (const fragment of fragments) {
      assert.ok(
        task.prompt.includes(fragment),
        `${taskId} prompt omits ${fragment}`,
      );
    }
  }
});

test("tasks require explicit scoring modes and rubric-only rationale", () => {
  const task = {
    id: "manual-task",
    category: "review",
    suite: "auxiliary",
    validationProfile: "python3-stdlib",
    prompt: "Review the implementation.",
  };

  assert.throws(
    () => validateTasks([task]),
    /scoringMode/,
  );
  assert.throws(
    () => validateTasks([{ ...task, scoringMode: "manual" }]),
    /scoringMode/,
  );
  assert.throws(
    () => validateTasks([{
      ...task,
      scoringMode: "rubric-only",
    }]),
    /rubricOnlyReasons/,
  );
  assert.throws(
    () => validateTasks([{
      ...task,
      scoringMode: "deterministic",
      rubricOnlyReasons: ["undocumented-service"],
      rubricOnlyRationale: "Not used.",
    }]),
    /deterministic scoringMode/,
  );
  const rubricOnly = validateTasks([{
    ...task,
    scoringMode: "rubric-only",
    rubricOnlyReasons: [
      "undocumented-service",
      "environment-dependent-scoring",
    ],
    rubricOnlyRationale: "The service cannot be published or reproduced.",
  }])[0];
  assert.equal(rubricOnly.scoringMode, "rubric-only");
  assert.deepEqual(scoringModeIds, ["deterministic", "rubric-only"]);
});
