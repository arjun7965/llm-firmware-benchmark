import { parseArgs } from "node:util";
import { runFixtureValidation } from "./src/fixture-sandbox.mjs";

const help = `Usage:
  npm run fixture:validate -- --task <task-id> [options]

Options:
  --task <id>           Fixture task ID
  --fixtures <path>     Fixture root (default: fixtures)
  --tasks-file <path>   Task definitions (default: tasks.json)
  -h, --help            Show this help
`;

try {
  const { values } = parseArgs({
    strict: true,
    options: {
      fixtures: { type: "string", default: "fixtures" },
      help: { type: "boolean", short: "h", default: false },
      task: { type: "string" },
      "tasks-file": { type: "string", default: "tasks.json" },
    },
  });
  if (values.help) {
    console.log(help);
  } else {
    if (!values.task) throw new TypeError("--task is required");
    const { report, reportPath } = runFixtureValidation({
      taskId: values.task,
      fixturesRoot: values.fixtures,
      tasksPath: values["tasks-file"],
    });
    console.log(JSON.stringify({
      taskId: report.taskId,
      success: report.success,
      reportPath,
    }));
    if (!report.success) process.exitCode = 2;
  }
} catch (error) {
  console.error(`Fixture validation failed: ${error.message}`);
  process.exitCode = 1;
}
