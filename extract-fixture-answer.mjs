import { parseArgs } from "node:util";
import { extractFixtureAnswer } from "./src/fixture-answers.mjs";

const help = `Usage:
  npm run fixture:extract -- --result <result.json> [options]

Options:
  --result <path>       Successful raw benchmark result
  --fixtures <path>     Fixture root (default: fixtures)
  --tasks-file <path>   Task definitions (default: tasks.json)
  --force               Replace an existing generated answer
  -h, --help            Show this help
`;

try {
  const { values } = parseArgs({
    strict: true,
    options: {
      fixtures: { type: "string", default: "fixtures" },
      force: { type: "boolean", default: false },
      help: { type: "boolean", short: "h", default: false },
      result: { type: "string" },
      "tasks-file": { type: "string", default: "tasks.json" },
    },
  });
  if (values.help) {
    console.log(help);
  } else {
    if (!values.result) throw new TypeError("--result is required");
    const summary = extractFixtureAnswer({
      resultPath: values.result,
      fixturesRoot: values.fixtures,
      tasksPath: values["tasks-file"],
      overwrite: values.force,
    });
    console.log(JSON.stringify(summary));
  }
} catch (error) {
  console.error(`Fixture answer extraction failed: ${error.message}`);
  process.exitCode = 1;
}
