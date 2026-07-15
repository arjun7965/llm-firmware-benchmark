import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { runFixtureMutationTests } from "../src/fixture-mutations.mjs";

const repositoryRoot = fileURLToPath(new URL("../", import.meta.url));

runFixtureMutationTests({
  fixturesRoot: join(repositoryRoot, "fixtures"),
  taskIds: [
    "bare-metal-timer",
    "binary-parser",
    "embedded-ring-buffer",
    "firmware-state-machine",
    "interrupt-vector-configuration",
    "i2c-controller-recovery",
    "rtos-priority-inversion",
    "uart-interrupt-driver",
    "spi-dma-transfer",
  ],
  tasksPath: join(repositoryRoot, "tasks.json"),
});
