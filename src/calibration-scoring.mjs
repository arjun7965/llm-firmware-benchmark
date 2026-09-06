import {
  createHash,
  randomBytes,
  randomInt,
} from "node:crypto";
import {
  existsSync,
  lstatSync,
  readFileSync,
  readdirSync,
  statSync,
} from "node:fs";
import {
  join,
  relative,
  resolve,
  sep,
} from "node:path";
import { isDeepStrictEqual } from "node:util";
import { extractAnswer } from "./answers.mjs";
import { promptSha256 } from "./harness.mjs";
import { describeGenerationBudget } from "./generation-budget.mjs";

const maximumResultBytes = 5 * 1024 * 1024;
const taskIdPattern = /^[a-z0-9]+(?:-[a-z0-9]+)*$/u;

function sha256(value) {
  return createHash("sha256").update(value).digest("hex");
}

function serializeJson(value) {
  return `${JSON.stringify(value, null, 2)}\n`;
}

function fencedMarkdownSource(value) {
  const longestRun = Math.max(
    0,
    ...(value.match(/`+/gu) ?? []).map((run) => run.length),
  );
  const fence = "`".repeat(Math.max(3, longestRun + 1));
  const closingLineBreak = value.endsWith("\n") ? "" : "\n";
  return `${fence}markdown\n${value}${closingLineBreak}${fence}`;
}

function requireNonEmptyString(value, name) {
  if (typeof value !== "string" || value.trim() === "") {
    throw new TypeError(`${name} must be a non-empty string`);
  }
  return value;
}

function normalizedIdentity(value) {
  return value.normalize("NFKD")
    .toLowerCase()
    .replace(/[^a-z0-9]+/gu, "");
}

export function resolvePrivateResultsPath(path, {
  mustExist = true,
  name = "path",
  results = "results",
} = {}) {
  const resultsRoot = resolve(results);
  const candidate = resolve(requireNonEmptyString(path, name));
  if (
    !existsSync(resultsRoot) ||
    lstatSync(resultsRoot).isSymbolicLink() ||
    !lstatSync(resultsRoot).isDirectory()
  ) {
    throw new TypeError("results root must be an existing non-symlink directory");
  }
  const fromResults = relative(resultsRoot, candidate);
  if (
    fromResults === "" ||
    fromResults === ".." ||
    fromResults.startsWith(`..${sep}`)
  ) {
    throw new TypeError(`${name} must be inside the ignored results directory`);
  }

  const segments = fromResults.split(sep);
  let current = resultsRoot;
  for (let index = 0; index < segments.length; index++) {
    current = join(current, segments[index]);
    if (!existsSync(current)) {
      if (mustExist || index !== segments.length - 1) {
        throw new TypeError(`${name} path does not exist: ${current}`);
      }
      break;
    }
    if (lstatSync(current).isSymbolicLink()) {
      throw new TypeError(`${name} path cannot contain symlinks`);
    }
  }
  return candidate;
}

function resultPaths(inputRoot, taskId) {
  const paths = [];
  const expectedPrefix = `${taskId}--`;

  function visit(directory) {
    for (const entry of readdirSync(directory, { withFileTypes: true })) {
      const path = join(directory, entry.name);
      if (entry.isSymbolicLink()) continue;
      if (entry.isDirectory()) {
        visit(path);
      } else if (
        entry.isFile() &&
        entry.name.startsWith(expectedPrefix) &&
        entry.name.endsWith(".json")
      ) {
        paths.push(path);
      }
    }
  }

  visit(inputRoot);
  return paths.sort();
}

function parseResult(path, inputRoot, task, { allowFailure = false } = {}) {
  const size = statSync(path).size;
  if (size > maximumResultBytes) {
    throw new RangeError(
      `calibration result exceeds ${maximumResultBytes} bytes: ${path}`,
    );
  }

  let result;
  try {
    result = JSON.parse(readFileSync(path, "utf8"));
  } catch (error) {
    throw new TypeError(`cannot parse calibration result ${path}: ${error.message}`);
  }
  if (!result || typeof result !== "object" || Array.isArray(result)) {
    throw new TypeError(`calibration result must be an object: ${path}`);
  }
  if (result.task !== task.id) {
    throw new TypeError(`calibration result has the wrong task: ${path}`);
  }
  const expectedMetadata = {
    category: task.category,
    scoringMode: task.scoringMode,
    suite: task.suite,
    targetProfile: task.targetProfile ?? null,
    validationProfile: task.validationProfile,
  };
  for (const [field, expected] of Object.entries(expectedMetadata)) {
    if (result[field] !== expected) {
      throw new TypeError(
        `calibration result ${field} does not match the task: ${path}`,
      );
    }
  }
  if (!Number.isSafeInteger(result.run) || result.run < 1) {
    throw new TypeError(`calibration result has an invalid run: ${path}`);
  }
  const modelName = requireNonEmptyString(
    result.modelName,
    `calibration result modelName in ${path}`,
  );
  const modelId = requireNonEmptyString(
    result.modelId,
    `calibration result modelId in ${path}`,
  );
  const provider = requireNonEmptyString(
    result.provider,
    `calibration result provider in ${path}`,
  );
  const expectedPromptSha256 = promptSha256(task.prompt);
  if (result.promptSha256 !== expectedPromptSha256) {
    throw new TypeError(`calibration result prompt does not match: ${path}`);
  }
  if ((result.exitCode !== null && !Number.isSafeInteger(result.exitCode)) ||
      (result.signal !== null && typeof result.signal !== "string") ||
      (result.error !== null && typeof result.error !== "string") ||
      typeof result.stdout !== "string") {
    throw new TypeError(`calibration result has malformed execution metadata: ${path}`);
  }
  const generationBudget = describeGenerationBudget(
    provider, result.modelOptions ?? {}, result.generationBudget !== undefined,
  );
  if (result.generationBudget !== undefined &&
      !isDeepStrictEqual(result.generationBudget, generationBudget)) {
    throw new TypeError(`calibration result generation budget does not match: ${path}`);
  }

  const successful = result.exitCode === 0 && result.signal === null &&
    result.error === null;
  if (!successful && !allowFailure) {
    throw new TypeError(`calibration result was not successful: ${path}`);
  }
  let answer = null;
  if (successful) {
    try {
      answer = extractAnswer(result.stdout);
      if (answer.trim() === "") throw new TypeError("answer is empty");
    } catch (error) {
      if (!allowFailure) throw error;
      answer = null;
    }
  }
  const metadata = {
    modelId,
    modelName,
    provider,
    run: result.run,
    source: relative(inputRoot, path),
    resultSha256: sha256(readFileSync(path)),
    modelOptions: result.modelOptions ?? {},
    providerConfigSha256: result.providerConfigSha256 ?? null,
    generationBudget,
    generationOutcome: answer === null ? generationFailure(result) : "answer",
  };
  if (answer === null) return metadata;
  const normalizedAnswer = normalizedIdentity(answer);
  const leakedIdentity = [modelName, modelId, provider]
    .map(normalizedIdentity)
    .filter((identity) => identity.length >= 4)
    .find((identity) => normalizedAnswer.includes(identity));
  if (leakedIdentity !== undefined) {
    throw new TypeError(
      `calibration answer exposes its model identity in ${path}`,
    );
  }

  return {
    answer,
    answerSha256: sha256(answer),
    ...metadata,
  };
}

export function loadCalibrationSamples(input, task) {
  const inputRoot = resolve(requireNonEmptyString(input, "input"));
  if (!task || typeof task !== "object" || Array.isArray(task)) {
    throw new TypeError("task must be an object");
  }
  if (typeof task.id !== "string" || !taskIdPattern.test(task.id)) {
    throw new TypeError("task.id must be a valid task ID");
  }
  requireNonEmptyString(task.prompt, "task.prompt");
  if (!lstatSync(inputRoot).isDirectory()) {
    throw new TypeError("calibration input must be a directory");
  }

  const paths = resultPaths(inputRoot, task.id);
  if (paths.length === 0) {
    throw new TypeError(`no calibration results found for ${task.id}`);
  }
  const samples = paths.map((path) => parseResult(path, inputRoot, task));
  const identities = new Set();
  for (const sample of samples) {
    const identity = `${sample.modelName}\0${sample.run}`;
    if (identities.has(identity)) {
      throw new TypeError(
        `duplicate calibration result for ${sample.modelName} run ${sample.run}`,
      );
    }
    identities.add(identity);
  }
  return samples;
}

const generationOutcomes = [
  "answer", "timeout", "generation-limit", "provider-error", "no-answer", "missing",
];

function generationFailure(result) {
  if (/^timed out after [0-9]+ ms/u.test(result.error ?? "")) return "timeout";
  // Only provider event metadata is evidence of a length stop, not answer prose.
  if (result.provider === "opencode") {
    let finish;
    for (const line of (result.stdout ?? "").split("\n")) {
      try {
        const event = JSON.parse(line);
        if (event.type === "step_finish") finish = event.part?.reason;
      } catch { /* Non-event text is not stop-reason evidence. */ }
    }
    if (finish === "length") return "generation-limit";
  }
  return result.exitCode === 0 && result.signal === null && result.error === null
    ? "no-answer" : "provider-error";
}

export function validateCalibrationPlan(plan, task) {
  requireObject(plan, "cohort plan");
  const planFields = ["schemaVersion", "taskId", "promptSha256", "models"];
  if (Object.keys(plan).some((field) => !planFields.includes(field))) {
    throw new TypeError("unknown cohort plan field");
  }
  if (plan.schemaVersion !== "1.0" || plan.taskId !== task.id ||
      plan.promptSha256 !== promptSha256(task.prompt)) {
    throw new TypeError("cohort plan version, task, or prompt does not match");
  }
  if (!Array.isArray(plan.models) || plan.models.length === 0) {
    throw new TypeError("cohort plan models must be a non-empty array");
  }
  const names = new Set();
  for (const model of plan.models) {
    requireObject(model, "cohort model");
    const modelFields = [
      "modelName", "modelId", "provider", "family", "modelOptions",
      "providerConfigSha256", "runs",
    ];
    if (Object.keys(model).some((field) => !modelFields.includes(field))) {
      throw new TypeError("unknown cohort model field");
    }
    for (const field of ["modelName", "modelId", "provider", "family"]) {
      requireNonEmptyString(model[field], `cohort model ${field}`);
    }
    if (names.has(model.modelName)) throw new TypeError("duplicate cohort model");
    names.add(model.modelName);
    requireObject(model.modelOptions, "cohort modelOptions");
    if (model.providerConfigSha256 !== null &&
        !/^[a-f0-9]{64}$/u.test(model.providerConfigSha256)) {
      throw new TypeError("cohort provider configuration digest is invalid");
    }
    if (!Array.isArray(model.runs) || model.runs.length === 0 ||
        model.runs.some((run) => !Number.isSafeInteger(run) || run < 1) ||
        new Set(model.runs).size !== model.runs.length) {
      throw new TypeError("cohort runs must be unique positive integers");
    }
  }
  return plan;
}

export function loadCalibrationCohort(input, task, plan) {
  validateCalibrationPlan(plan, task);
  const inputRoot = resolve(requireNonEmptyString(input, "input"));
  const records = resultPaths(inputRoot, task.id).map((path) =>
    parseResult(path, inputRoot, task, { allowFailure: true }));
  const byRun = new Map();
  const budgets = new Map();
  for (const record of records) {
    const model = plan.models.find((entry) => entry.modelName === record.modelName);
    if (!model || !model.runs.includes(record.run)) {
      throw new TypeError("result is outside the declared cohort");
    }
    for (const field of ["modelId", "provider", "modelOptions", "providerConfigSha256"]) {
      if (!isDeepStrictEqual(record[field], model[field])) {
        throw new TypeError(`cohort result ${field} does not match the plan`);
      }
    }
    if (budgets.has(record.modelName) &&
        !isDeepStrictEqual(budgets.get(record.modelName), record.generationBudget)) {
      throw new TypeError("cohort model has inconsistent recorded generation budgets");
    }
    budgets.set(record.modelName, record.generationBudget);
    const id = `${record.modelName}\0${record.run}`;
    if (byRun.has(id)) {
      throw new TypeError("duplicate cohort run; keep retries in a separate cohort");
    }
    byRun.set(id, record);
  }
  const attempts = plan.models.flatMap((model) => model.runs.map((run) => {
    const record = byRun.get(`${model.modelName}\0${run}`);
    return {
      modelName: model.modelName,
      run,
      outcome: record?.generationOutcome ?? "missing",
      source: record?.source ?? null,
      resultSha256: record?.resultSha256 ?? null,
      answerSha256: record?.answerSha256 ?? null,
      generationBudget: record?.generationBudget ?? null,
    };
  }));
  return {
    samples: records.filter((record) => record.generationOutcome === "answer"),
    cohort: { plan, attempts },
  };
}

function validateCohort(cohort, task, samples) {
  requireObject(cohort, "cohort");
  validateCalibrationPlan(cohort.plan, task);
  const expected = new Map(cohort.plan.models.flatMap((model) =>
    model.runs.map((run) => [`${model.modelName}\0${run}`, model])));
  if (!Array.isArray(cohort.attempts) || cohort.attempts.length !== expected.size) {
    throw new TypeError("cohort attempt set does not match the plan");
  }
  const answers = new Map(samples.map((sample) =>
    [`${sample.modelName}\0${sample.run}`, sample]));
  if (answers.size !== samples.length) throw new TypeError("duplicate cohort answer");
  for (const attempt of cohort.attempts) {
    requireObject(attempt, "cohort attempt");
    if (!Number.isSafeInteger(attempt.run) || attempt.run < 1) {
      throw new TypeError("invalid cohort attempt run");
    }
    const id = `${attempt.modelName}\0${attempt.run}`;
    const model = expected.get(id);
    if (!model || !generationOutcomes.includes(attempt.outcome)) {
      throw new TypeError("invalid or duplicate cohort attempt");
    }
    expected.delete(id);
    if (attempt.outcome === "missing") {
      if (attempt.source !== null || attempt.resultSha256 !== null ||
          attempt.generationBudget !== null) {
        throw new TypeError("missing cohort attempt contains result evidence");
      }
    } else {
      requireNonEmptyString(attempt.source, "cohort result source");
      if (!/^[a-f0-9]{64}$/u.test(attempt.resultSha256)) {
        throw new TypeError("cohort result digest is invalid");
      }
      requireObject(attempt.generationBudget, "cohort generation budget");
    }
    const answer = answers.get(id);
    if (attempt.outcome === "answer") {
      if (!answer || attempt.answerSha256 !== answer.answerSha256 ||
          answer.modelId !== model.modelId || answer.provider !== model.provider ||
          answer.source !== attempt.source) {
        throw new TypeError("cohort answer set does not match scoring samples");
      }
      answers.delete(id);
    } else if (answer || attempt.answerSha256 !== null) {
      throw new TypeError("failed cohort attempt cannot have a rubric score");
    }
  }
  if (answers.size) throw new TypeError("scoring samples outside cohort");
}

function generationCounts(attempts) {
  const outcomes = Object.fromEntries(generationOutcomes.map((name) => [name, 0]));
  for (const attempt of attempts) outcomes[attempt.outcome]++;
  return {
    scheduled: attempts.length,
    recorded: attempts.length - outcomes.missing,
    answers: outcomes.answer,
    answerRate: attempts.length ? outcomes.answer / attempts.length : null,
    outcomes,
  };
}

function criterionId(label, index) {
  const normalized = label.toLowerCase()
    .replace(/&/gu, " and ")
    .replace(/[^a-z0-9]+/gu, "-")
    .replace(/^-|-$/gu, "");
  return normalized === "" ? `criterion-${index + 1}` : normalized;
}

export function parseCalibrationRubric(rubric) {
  requireNonEmptyString(rubric, "rubric");
  const criteria = [];
  const lines = rubric.replace(/\r\n?/gu, "\n").split("\n");
  for (let index = 0; index < lines.length; index++) {
    const match = lines[index].match(
      /^\s*-\s+([0-9]+(?:\.[0-9]+)?) points?\s+—\s+(.+)$/u,
    );
    if (!match) continue;

    let text = match[2].trim();
    for (
      let continuation = index + 1;
      continuation < lines.length && /^\s{2,}\S/u.test(lines[continuation]);
      continuation++
    ) {
      text += ` ${lines[continuation].trim()}`;
      index = continuation;
    }
    const emphasized = text.match(/^\*\*([^*]+?):?\*\*\s*(.*)$/u);
    const label = emphasized
      ? emphasized[1].replace(/:$/u, "")
      : `Criterion ${criteria.length + 1}`;
    const description = emphasized ? emphasized[2] : text;
    const maximum = Number(match[1]);
    criteria.push({
      description,
      id: criterionId(label, criteria.length),
      label,
      maximum,
    });
  }

  if (criteria.length === 0) {
    throw new TypeError("rubric does not contain point-valued criteria");
  }
  const maximum = criteria.reduce((sum, criterion) => sum + criterion.maximum, 0);
  if (maximum !== 10) {
    throw new TypeError(`rubric criteria total ${maximum}, expected 10`);
  }
  return criteria;
}

function rubricForBlindReview(rubric) {
  const lines = requireNonEmptyString(rubric, "rubric")
    .replace(/\r\n?/gu, "\n")
    .split("\n");
  const retained = [];
  let omitSection = false;

  for (const line of lines) {
    const heading = line.match(/^##\s+(.+?)\s*$/u);
    if (heading) {
      omitSection = heading[1].trim().toLowerCase() === "calibration";
      if (omitSection) continue;
    }
    if (!omitSection) retained.push(line);
  }

  return `${retained.join("\n").trimEnd()}\n`;
}

function shuffledSamples(samples, chooseIndex) {
  const shuffled = [...samples];
  for (let upper = shuffled.length; upper > 1; upper--) {
    const index = chooseIndex(upper);
    if (!Number.isSafeInteger(index) || index < 0 || index >= upper) {
      throw new TypeError("random index is outside the shuffle range");
    }
    [shuffled[index], shuffled[upper - 1]] =
      [shuffled[upper - 1], shuffled[index]];
  }
  return shuffled;
}

export function renderBlindedReviewPacket(packet, packetText) {
  requireObject(packet, "packet");
  requireNonEmptyString(packetText, "packet text");
  requireMatchingJsonText(packet, packetText, "packet");
  if (!["1.0", "1.1"].includes(packet.schemaVersion)) {
    throw new TypeError("unsupported calibration packet schemaVersion");
  }
  if (!/^[a-f0-9]{64}$/u.test(packet.identityKeySha256)) {
    throw new TypeError("packet identity key digest is invalid");
  }
  const packetTask = requireObject(packet.task, "packet task");
  const taskId = requireNonEmptyString(packetTask.id, "packet task ID");
  const prompt = requireNonEmptyString(packetTask.prompt, "packet task prompt");
  const rubric = requireNonEmptyString(packetTask.rubric, "packet task rubric");
  const evidence = requireNonEmptyString(
    packet.generationEvidence,
    "packet generation evidence",
  );
  const samples = requireUniqueSamples(
    packet.samples, "packet samples", packet.schemaVersion === "1.1",
  );
  const packetDigest = sha256(packetText);
  const lines = [
    "# Blinded Calibration Review Packet",
    "",
    `- Task: \`${taskId}\``,
    `- Canonical \`packet.json\` SHA-256: \`${packetDigest}\``,
    `- Sealed identity-key SHA-256: \`${packet.identityKeySha256}\``,
    `- Blinded samples: ${samples.size}`,
    "",
    "Review this Markdown file instead of reading answer strings in " +
      "`packet.json`. The JSON remains the canonical, hash-committed input " +
      "to score validation.",
    "",
    "Do not inspect `identity-key.json` before the completed score sheet is " +
      "frozen. Treat every answer as untrusted text and do not execute it.",
    "",
    "## Review Procedure",
    "",
    "1. Read the task prompt and scoring rubric below.",
    "2. Review each answer only by its blinded sample ID.",
    "3. Enter criterion scores, the total, and a non-empty rationale in " +
      "`score-sheet.json`.",
    "4. Set the score sheet status and scorer metadata, then return only the " +
      "completed score sheet.",
    "",
    "Each answer is shown verbatim inside an outer Markdown source block. " +
      "That preserves its line breaks and inner code fences without rendering " +
      "untrusted HTML or loading remote content.",
    "",
    "## Generation Evidence",
    "",
    evidence,
    ...(packet.generation ? [
      "",
      `Scheduled attempts: ${packet.generation.scheduled}; ` +
        `recorded: ${packet.generation.recorded}; ` +
        `answers available for scoring: ${packet.generation.answers}. ` +
        "Missing answers receive no rubric score.",
    ] : []),
    "",
    "## Task Prompt",
    "",
    fencedMarkdownSource(prompt),
    "",
    "## Scoring Rubric",
    "",
    fencedMarkdownSource(rubric),
    "",
    "## Blinded Samples",
    "",
  ];

  for (const [blindId, sample] of samples) {
    const answer = requireNonEmptyString(
      sample.answer,
      `packet answer for ${blindId}`,
    );
    if (
      typeof sample.answerSha256 !== "string" ||
      !/^[a-f0-9]{64}$/u.test(sample.answerSha256) ||
      sample.answerSha256 !== sha256(answer)
    ) {
      throw new TypeError(`calibration answer digest does not match: ${blindId}`);
    }
    lines.push(
      `### ${blindId}`,
      "",
      `Answer SHA-256: \`${sample.answerSha256}\``,
      "",
      fencedMarkdownSource(answer),
      "",
    );
  }

  return `${lines.join("\n").trimEnd()}\n`;
}

export function buildBlindedScoringArtifacts({
  cohort = null,
  createCommitmentNonce = () => randomBytes(32).toString("hex"),
  rubric,
  samples,
  task,
  chooseIndex = (upper) => randomInt(upper),
}) {
  if (!Array.isArray(samples) || (samples.length === 0 && cohort === null)) {
    throw new TypeError("samples must be a non-empty array without a cohort");
  }
  if (!task || typeof task !== "object" || Array.isArray(task)) {
    throw new TypeError("task must be an object");
  }
  if (cohort !== null) validateCohort(cohort, task, samples);
  const schemaVersion = cohort === null ? "1.0" : "1.1";
  const reviewRubric = rubricForBlindReview(rubric);
  const criteria = parseCalibrationRubric(reviewRubric);
  const shuffled = shuffledSamples(samples, chooseIndex);
  const width = Math.max(2, String(shuffled.length).length);
  const promptDigest = promptSha256(task.prompt);
  const keyedSamples = shuffled.map((sample, index) => ({
    blindId: `sample-${String(index + 1).padStart(width, "0")}`,
    ...sample,
  }));
  const commitmentNonce = createCommitmentNonce();
  if (
    typeof commitmentNonce !== "string" ||
    !/^[a-f0-9]{64}$/u.test(commitmentNonce)
  ) {
    throw new TypeError("identity key commitment nonce must be 32-byte hex");
  }
  const key = {
    schemaVersion,
    ...(cohort === null ? {} : { cohort }),
    taskId: task.id,
    commitmentNonce,
    samples: keyedSamples.map((sample) => ({
      answerSha256: sample.answerSha256,
      blindId: sample.blindId,
      modelId: sample.modelId,
      modelName: sample.modelName,
      provider: sample.provider,
      run: sample.run,
      source: sample.source,
    })),
  };
  const keyText = serializeJson(key);
  const packet = {
    schemaVersion,
    ...(cohort === null ? {} : { generation: generationCounts(cohort.attempts) }),
    identityKeySha256: sha256(keyText),
    task: {
      id: task.id,
      prompt: task.prompt,
      promptSha256: promptDigest,
      rubric: reviewRubric,
      rubricCriteria: criteria,
    },
    generationEvidence:
      "Every included provider result completed successfully and contained an extractable answer. Verify answer-contract extraction and deterministic validation evidence separately before scoring; inclusion in this packet does not attest either outcome.",
    samples: keyedSamples.map((sample) => ({
      answer: sample.answer,
      answerSha256: sample.answerSha256,
      blindId: sample.blindId,
    })),
  };
  const packetText = serializeJson(packet);
  const packetMarkdownText = renderBlindedReviewPacket(packet, packetText);
  const packetSha256 = sha256(packetText);
  const scoreSheet = {
    schemaVersion,
    taskId: task.id,
    packetSha256,
    status: "in-progress",
    scorer: {
      completedAt: null,
      identity: null,
      type: null,
    },
    criteria,
    samples: keyedSamples.map((sample) => ({
      blindId: sample.blindId,
      scores: Object.fromEntries(criteria.map((criterion) => [criterion.id, null])),
      total: null,
      rationale: "",
    })),
  };
  return {
    key,
    keyText,
    packet,
    packetMarkdownText,
    packetText,
    scoreSheet,
    scoreSheetText: serializeJson(scoreSheet),
  };
}

function requireObject(value, name) {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    throw new TypeError(`${name} must be an object`);
  }
  return value;
}

function requireUniqueSamples(samples, name, allowEmpty = false) {
  if (!Array.isArray(samples) || (!allowEmpty && samples.length === 0)) {
    throw new TypeError(`${name} must be a non-empty array`);
  }
  const byBlindId = new Map();
  for (const sample of samples) {
    requireObject(sample, `${name} sample`);
    requireNonEmptyString(sample.blindId, `${name} blindId`);
    if (byBlindId.has(sample.blindId)) {
      throw new TypeError(`${name} contains duplicate blind IDs`);
    }
    byBlindId.set(sample.blindId, sample);
  }
  return byBlindId;
}

function sameKeys(left, right) {
  const leftKeys = Object.keys(left).sort();
  const rightKeys = Object.keys(right).sort();
  return leftKeys.length === rightKeys.length &&
    leftKeys.every((key, index) => key === rightKeys[index]);
}

function summarizeTotals(values) {
  if (values.length === 0) return { mean: null, range: null, sd: null };
  const mean = values.reduce((sum, value) => sum + value, 0) / values.length;
  const variance = values.reduce(
    (sum, value) => sum + (value - mean) ** 2,
    0,
  ) / values.length;
  return {
    mean,
    range: Math.max(...values) - Math.min(...values),
    sd: Math.sqrt(variance),
  };
}

function requireMatchingJsonText(value, valueText, name) {
  let parsed;
  try {
    parsed = JSON.parse(valueText);
  } catch (error) {
    throw new TypeError(`${name} text is not valid JSON: ${error.message}`);
  }
  if (!isDeepStrictEqual(parsed, value)) {
    throw new TypeError(`${name} text does not match its parsed value`);
  }
}

export function summarizeCompletedCalibrationScoring({
  identityKey,
  identityKeyText,
  packet,
  packetText,
  scoreSheet,
  scoreSheetText,
}) {
  requireObject(identityKey, "identity key");
  requireObject(packet, "packet");
  requireObject(scoreSheet, "score sheet");
  requireNonEmptyString(identityKeyText, "identity key text");
  requireNonEmptyString(packetText, "packet text");
  requireNonEmptyString(scoreSheetText, "score sheet text");
  requireMatchingJsonText(identityKey, identityKeyText, "identity key");
  requireMatchingJsonText(packet, packetText, "packet");
  requireMatchingJsonText(scoreSheet, scoreSheetText, "score sheet");
  if (
    !["1.0", "1.1"].includes(packet.schemaVersion) ||
    identityKey.schemaVersion !== packet.schemaVersion ||
    scoreSheet.schemaVersion !== packet.schemaVersion
  ) {
    throw new TypeError("unsupported calibration scoring schemaVersion");
  }
  const taskId = requireNonEmptyString(identityKey.taskId, "identity key taskId");
  if (!/^[a-f0-9]{64}$/u.test(identityKey.commitmentNonce)) {
    throw new TypeError("calibration scoring identity key nonce is invalid");
  }
  if (packet.task?.id !== taskId || scoreSheet.taskId !== taskId) {
    throw new TypeError("calibration scoring task IDs do not match");
  }
  if (packet.identityKeySha256 !== sha256(identityKeyText)) {
    throw new TypeError("calibration scoring identity key digest does not match");
  }

  const packetDigest = sha256(packetText);
  if (scoreSheet.packetSha256 !== packetDigest) {
    throw new TypeError("calibration scoring packet digest does not match");
  }
  const hasCohort = packet.schemaVersion === "1.1";
  const packetSamples = requireUniqueSamples(packet.samples, "packet samples", hasCohort);
  const keySamples = requireUniqueSamples(identityKey.samples, "identity key samples", hasCohort);
  const scoredSamples = requireUniqueSamples(scoreSheet.samples, "scored samples", hasCohort);
  if (hasCohort) {
    validateCohort(identityKey.cohort, packet.task, identityKey.samples);
    if (!isDeepStrictEqual(packet.generation, generationCounts(identityKey.cohort.attempts))) {
      throw new TypeError("cohort generation counts do not match the packet");
    }
  }
  if (
    packetSamples.size !== keySamples.size ||
    packetSamples.size !== scoredSamples.size
  ) {
    throw new TypeError("calibration scoring sample sets do not match");
  }
  for (const [blindId, packetSample] of packetSamples) {
    const keySample = keySamples.get(blindId);
    if (keySample === undefined || !scoredSamples.has(blindId)) {
      throw new TypeError("calibration scoring sample sets do not match");
    }
    if (
      packetSample.answerSha256 !== sha256(packetSample.answer) ||
      keySample.answerSha256 !== packetSample.answerSha256
    ) {
      throw new TypeError(`calibration answer digest does not match: ${blindId}`);
    }
  }

  if (scoreSheet.status !== "complete") {
    throw new TypeError("calibration score sheet is not complete");
  }
  const scorer = requireObject(scoreSheet.scorer, "score sheet scorer");
  requireNonEmptyString(scorer.completedAt, "score sheet scorer.completedAt");
  requireNonEmptyString(scorer.identity, "score sheet scorer.identity");
  requireNonEmptyString(scorer.type, "score sheet scorer.type");
  if (!Number.isFinite(Date.parse(scorer.completedAt))) {
    throw new TypeError("score sheet scorer.completedAt must be an ISO date");
  }
  if (
    !Array.isArray(scoreSheet.criteria) ||
    JSON.stringify(scoreSheet.criteria) !==
      JSON.stringify(packet.task.rubricCriteria)
  ) {
    throw new TypeError("score sheet criteria do not match the packet");
  }
  const maxima = Object.fromEntries(
    scoreSheet.criteria.map((criterion) => [criterion.id, criterion.maximum]),
  );
  if (Object.keys(maxima).length !== scoreSheet.criteria.length) {
    throw new TypeError("score sheet contains duplicate criteria");
  }

  const unblinded = [];
  const modelRuns = new Set();
  for (const [blindId, keySample] of keySamples) {
    const scored = scoredSamples.get(blindId);
    requireObject(scored.scores, `scores for ${blindId}`);
    if (!sameKeys(scored.scores, maxima)) {
      throw new TypeError(`score criteria do not match: ${blindId}`);
    }
    for (const [criterion, value] of Object.entries(scored.scores)) {
      if (
        !Number.isFinite(value) ||
        value < 0 ||
        value > maxima[criterion]
      ) {
        throw new TypeError(`criterion score is outside its range: ${blindId}`);
      }
    }
    const total = Object.values(scored.scores)
      .reduce((sum, value) => sum + value, 0);
    if (!Number.isFinite(scored.total) || Math.abs(scored.total - total) > 1e-9) {
      throw new TypeError(`score total does not match criteria: ${blindId}`);
    }
    requireNonEmptyString(scored.rationale, `score rationale for ${blindId}`);
    const modelName = requireNonEmptyString(
      keySample.modelName,
      `modelName for ${blindId}`,
    );
    if (!Number.isSafeInteger(keySample.run) || keySample.run < 1) {
      throw new TypeError(`run is invalid for ${blindId}`);
    }
    const modelRun = `${modelName}\0${keySample.run}`;
    if (modelRuns.has(modelRun)) {
      throw new TypeError(`duplicate model run in identity key: ${modelName}`);
    }
    modelRuns.add(modelRun);
    unblinded.push({
      model: modelName,
      run: keySample.run,
      score: scored.total,
    });
  }

  unblinded.sort((left, right) =>
    left.model.localeCompare(right.model) || left.run - right.run);
  const byModel = new Map();
  for (const score of unblinded) {
    if (!byModel.has(score.model)) byModel.set(score.model, []);
    byModel.get(score.model).push(score);
  }
  if (hasCohort) {
    for (const model of identityKey.cohort.plan.models) {
      if (!byModel.has(model.modelName)) byModel.set(model.modelName, []);
    }
  }
  const models = [...byModel]
    .sort(([left], [right]) => left.localeCompare(right))
    .map(([model, runs]) => ({
      model,
      runs: runs.map(({ run, score }) => ({ run, score })),
      ...summarizeTotals(runs.map(({ score }) => score)),
      ...(hasCohort ? {
        generation: generationCounts(identityKey.cohort.attempts.filter((attempt) =>
          attempt.modelName === model)),
        generationRuns: identityKey.cohort.attempts.filter((attempt) =>
          attempt.modelName === model).map(({ run, outcome, generationBudget }) =>
          ({ run, outcome, generationBudget })),
      } : {}),
    }));
  const allScores = unblinded.map(({ score }) => score);

  return {
    schemaVersion: packet.schemaVersion,
    ...(hasCohort ? {
      generation: packet.generation,
      scorePopulation: "available-answers-only",
      cohortRecorded: packet.generation.outcomes.missing === 0,
      plannedFamilyCount: new Set(
        identityKey.cohort.plan.models.map((model) => model.family),
      ).size,
    } : {}),
    taskId,
    packetSha256: packetDigest,
    scoreSheetSha256: sha256(scoreSheetText),
    scorer,
    models,
    overall: {
      sampleCount: allScores.length,
      ...summarizeTotals(allScores),
    },
  };
}
