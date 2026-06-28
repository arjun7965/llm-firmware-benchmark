import { parseArgs } from "node:util";
import { exportPublicResults } from "./src/public-results.mjs";

const { values } = parseArgs({
  options: {
    input: { type: "string", default: "results" },
    output: { type: "string", default: "public-results" },
    "allow-redactions": { type: "boolean", default: false },
  },
});

try {
  const summary = exportPublicResults({
    inputDir: values.input,
    outputDir: values.output,
  });
  console.log(JSON.stringify(summary));
  if (summary.redactionCount > 0 && !values["allow-redactions"]) {
    console.error(
      "Export contains redactions and requires review; " +
      "rerun with --allow-redactions only after inspection.",
    );
    process.exitCode = 2;
  }
} catch (error) {
  console.error(`Public export failed: ${error.message}`);
  process.exitCode = 1;
}
