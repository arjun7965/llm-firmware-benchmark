import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { runFixtureMutationTests } from "../src/fixture-mutations.mjs";

const repositoryRoot = fileURLToPath(new URL("../", import.meta.url));

runFixtureMutationTests({
  fixturesRoot: join(repositoryRoot, "fixtures"),
  taskIds: [
    "bare-metal-timer",
    "binary-parser",
    "modbus-rtu-receiver",
    "can-transport-reassembly",
    "ble-advertising-reassembly",
    "embedded-ring-buffer",
    "static-memory-pool",
    "fixed-point-stack-budget",
    "dma-cache-coherency",
    "firmware-state-machine",
    "interrupt-vector-configuration",
    "linker-memory-map",
    "i2c-controller-recovery",
    "gpio-edge-debounce",
    "adc-threshold-watchdog",
    "pwm-synchronized-update",
    "watchdog-window-recovery",
    "brownout-safe-mode",
    "fault-crash-record",
    "idempotent-system-init",
    "secure-boot-image-validation",
    "dual-slot-update-recovery",
    "low-power-wake-clock",
    "real-time-deadline-budget",
    "timer-dma-handoff",
    "timer-capture-overflow",
    "rtos-priority-inversion",
    "rtos-periodic-scheduler",
    "rtos-queue-semaphore",
    "rtos-event-flags-deadlock",
    "uart-interrupt-driver",
    "spi-dma-transfer",
    "can-controller-recovery",
    "interrupt-deferred-work",
  ],
  tasksPath: join(repositoryRoot, "tasks.json"),
});
