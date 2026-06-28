import { runBenchmarkCli } from "./src/benchmark-cli.mjs";

await runBenchmarkCli({
  commandName: "benchmark:repeats",
  defaultRuns: [2, 3],
});
