import {
  loadHilCatalog,
} from "../src/hil-targets.mjs";
import { loadHilReport } from "../src/hil-reports.mjs";

const usage = `Usage: npm run hil:report -- --report <report.json>

Validate one supplemental hardware-in-the-loop result report.`;

try {
  const args = process.argv.slice(2);
  if (args.length === 1 && args[0] === "--help") {
    console.log(usage);
  } else if (args.length === 2 && args[0] === "--report") {
    const catalog = loadHilCatalog();
    const report = loadHilReport(args[1], catalog);
    console.log(
      "HIL report valid:",
      JSON.stringify({ success: report.success, targetId: report.targetId }),
    );
  } else {
    throw new TypeError(usage);
  }
} catch (error) {
  console.error(error.message);
  process.exitCode = 1;
}
