import { isDeepStrictEqual } from "node:util";

const outcomes = ["answer", "timeout", "generation-limit", "provider-error", "no-answer"];
const digestPattern = /^[a-f0-9]{64}$/u;

// Inputs come from loadCalibrationCohort and the private extraction/validation audit.
// Never project diagnostics, paths, answer text, provider options, or raw usage.
export function summarizeReasoningSweep({ cohort, evidence, phaseIds }) {
  const { plan, attempts } = cohort;
  if (!Array.isArray(phaseIds) || phaseIds.length === 0 ||
      new Set(phaseIds).size !== phaseIds.length) {
    throw new TypeError("expected fixture phase IDs are required and must be unique");
  }
  const planned = plan.models.flatMap((model) => model.runs.map((run) => ({ model, run })));
  const key = (row) => `${row.modelName}\0${row.run}`;
  const byAttempt = new Map(attempts.map((row) => [key(row), row]));
  const byEvidence = new Map(evidence.map((row) => [key(row), row]));
  if (byAttempt.size !== attempts.length || byEvidence.size !== evidence.length ||
      attempts.length !== planned.length || evidence.length !== planned.length) {
    throw new TypeError("duplicate or incomplete sweep evidence");
  }
  let environment = null;
  const rows = planned.map(({ model, run }) => {
    const id = key({ modelName: model.modelName, run });
    const attempt = byAttempt.get(id);
    const entry = byEvidence.get(id);
    if (!attempt || !entry || !outcomes.includes(attempt.outcome)) {
      throw new TypeError("missing or unplanned sweep attempt");
    }
    if (!digestPattern.test(attempt.resultSha256) ||
        attempt.resultSha256 !== entry.resultSha256 || entry.outcome !== attempt.outcome ||
        !Number.isSafeInteger(entry.durationMs) || entry.durationMs < 0) {
      throw new TypeError("sweep evidence does not match the original attempt");
    }
    let validationOutcome = "not-run";
    let candidateCompiled = false;
    let compiled = false;
    const report = entry.report;
    if (attempt.outcome !== "answer") {
      if (entry.extraction != null || report != null) {
        throw new TypeError("failed generation cannot have extraction or validation evidence");
      }
    } else if (typeof entry.extraction?.success !== "boolean") {
      throw new TypeError("answer is missing extraction evidence");
    } else if (!entry.extraction.success) {
      if (report != null) throw new TypeError("failed extraction cannot have a validation report");
      validationOutcome = "extraction-failure";
    } else {
      if (!report || report.taskId !== plan.taskId ||
          !digestPattern.test(entry.extraction.sha256) ||
          report.answerSha256 !== entry.extraction.sha256 ||
          !digestPattern.test(entry.reportSha256)) {
        throw new TypeError("validation report does not match the extracted answer");
      }
      const fingerprint = {
        profile: report.validationProfile,
        profileRevision: report.validationProfileRevision,
        profileSha256: report.validationProfileSha256,
        environmentId: report.validationEnvironment?.id,
        environmentRevision: report.validationEnvironment?.revision,
        environmentSha256: report.validationEnvironment?.sha256,
      };
      if (!fingerprint.profile || !fingerprint.environmentId ||
          !Number.isSafeInteger(fingerprint.profileRevision) ||
          !Number.isSafeInteger(fingerprint.environmentRevision) ||
          !digestPattern.test(fingerprint.profileSha256) ||
          !digestPattern.test(fingerprint.environmentSha256) ||
          (environment && !isDeepStrictEqual(environment, fingerprint))) {
        throw new TypeError("validation environment is missing or inconsistent");
      }
      environment = fingerprint;
      const phases = report.phases;
      if (!Array.isArray(phases) || phases.length === 0 || phases.length > phaseIds.length ||
          phases.some((phase, index) => phase.id !== phaseIds[index] ||
            !["compile", "test"].includes(phase.phase) ||
            !["passed", "failed", "timed-out", "error"].includes(phase.outcome)) ||
          phases.slice(0, -1).some((phase) => phase.outcome !== "passed")) {
        throw new TypeError("invalid validation phase sequence");
      }
      const failed = phases.find((phase) => phase.outcome !== "passed");
      const passed = phases.length === phaseIds.length && !failed;
      if (report.success !== passed || (!passed && !failed)) {
        throw new TypeError("incomplete or inconsistent validation result");
      }
      candidateCompiled = phases[0].phase === "compile" && phases[0].outcome === "passed";
      compiled = phases.some((phase) => phase.phase === "test");
      validationOutcome = passed ? "passed" : failed.outcome === "error"
        ? "validation-error"
        : `${failed.phase === "compile" ? "compile" : "runtime"}-failure`;
    }
    return {
      modelName: model.modelName,
      run,
      generationOutcome: attempt.outcome,
      durationMs: entry.durationMs,
      resultSha256: entry.resultSha256,
      extracted: entry.extraction?.success ?? false,
      candidateCompiled,
      compiled,
      validationOutcome,
      validationReportSha256: entry.reportSha256 ?? null,
    };
  });
  const averageSeconds = (values) => values.length
    ? values.reduce((sum, row) => sum + row.durationMs, 0) / values.length / 1000
    : null;
  return {
    schemaVersion: "1.0",
    taskId: plan.taskId,
    promptSha256: plan.promptSha256,
    plannedFamilyCount: new Set(plan.models.map((model) => model.family)).size,
    validation: environment,
    groups: plan.models.map((model) => {
      const selected = rows.filter((row) => row.modelName === model.modelName);
      const answers = selected.filter((row) => row.generationOutcome === "answer");
      return {
        modelName: model.modelName,
        scheduled: model.runs.length,
        answers: answers.length,
        outcomes: Object.fromEntries(outcomes.map((outcome) => [
          outcome, selected.filter((row) => row.generationOutcome === outcome).length,
        ])),
        extracted: selected.filter((row) => row.extracted).length,
        candidateCompiled: selected.filter((row) => row.candidateCompiled).length,
        compiled: selected.filter((row) => row.compiled).length,
        passed: selected.filter((row) => row.validationOutcome === "passed").length,
        meanAttemptSeconds: averageSeconds(selected),
        meanAnswerSeconds: averageSeconds(answers),
      };
    }),
    attempts: rows,
  };
}
