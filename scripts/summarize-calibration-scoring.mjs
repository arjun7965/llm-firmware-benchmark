import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { parseArgs } from "node:util";
import {
  resolvePrivateResultsPath,
  summarizeCompletedCalibrationScoring,
} from "../src/calibration-scoring.mjs";

const { values } = parseArgs({
  allowPositionals: false,
  options: {
    directory: { type: "string" },
  },
  strict: true,
});

if (typeof values.directory !== "string" || values.directory.trim() === "") {
  throw new TypeError("--directory is required");
}
const directory = resolvePrivateResultsPath(values.directory, {
  name: "--directory",
});

function readJson(name) {
  const text = readFileSync(resolve(directory, name), "utf8");
  return { text, value: JSON.parse(text) };
}

const packet = readJson("packet.json");
const scoreSheet = readJson("score-sheet.json");
const identityKey = readJson("identity-key.json");
const summary = summarizeCompletedCalibrationScoring({
  identityKey: identityKey.value,
  packet: packet.value,
  packetText: packet.text,
  scoreSheet: scoreSheet.value,
  scoreSheetText: scoreSheet.text,
});

console.log(JSON.stringify(summary, null, 2));
