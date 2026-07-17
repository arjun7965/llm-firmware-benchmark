import { spawnSync } from "node:child_process";
import {
  mkdtempSync,
  rmSync,
} from "node:fs";
import { tmpdir } from "node:os";
import {
  join,
  resolve,
} from "node:path";

const commonFlags = Object.freeze([
  "-std=c11",
  "-Wall",
  "-Wextra",
  "-Werror",
  "-pedantic",
  "-ffreestanding",
  "-fno-builtin",
  "-Os",
]);

const trustedSources = Object.freeze([
  Object.freeze({
    id: "bare-metal-timer",
    source: "fixtures/bare-metal-timer/reference/fictional_timer.c",
    includes: Object.freeze([
      "fixtures/bare-metal-timer/starter",
    ]),
    targetIds: Object.freeze([
      "armv7m-bare-metal",
    ]),
  }),
  Object.freeze({
    id: "interrupt-vector-configuration",
    source:
      "fixtures/interrupt-vector-configuration/reference/interrupt_vector.c",
    includes: Object.freeze([
      "fixtures/interrupt-vector-configuration/starter",
    ]),
    targetIds: Object.freeze([
      "armv7m-bare-metal",
    ]),
  }),
  Object.freeze({
    id: "linker-memory-map",
    source: "fixtures/linker-memory-map/reference/linker_memory_map.c",
    includes: Object.freeze([
      "fixtures/linker-memory-map/starter",
    ]),
    targetIds: Object.freeze([
      "armv7m-bare-metal",
    ]),
  }),
  Object.freeze({
    id: "i2c-controller-recovery",
    source: "fixtures/i2c-controller-recovery/reference/i2c_controller.c",
    includes: Object.freeze([
      "fixtures/i2c-controller-recovery/starter",
    ]),
    targetIds: Object.freeze([
      "armv7m-bare-metal",
    ]),
  }),
  Object.freeze({
    id: "gpio-edge-debounce",
    source: "fixtures/gpio-edge-debounce/reference/gpio_debounce.c",
    includes: Object.freeze([
      "fixtures/gpio-edge-debounce/starter",
    ]),
    targetIds: Object.freeze([
      "armv7m-bare-metal",
    ]),
  }),
  Object.freeze({
    id: "adc-threshold-watchdog",
    source: "fixtures/adc-threshold-watchdog/reference/adc_watchdog.c",
    includes: Object.freeze([
      "fixtures/adc-threshold-watchdog/starter",
    ]),
    targetIds: Object.freeze([
      "armv7m-bare-metal",
    ]),
  }),
  Object.freeze({
    id: "pwm-synchronized-update",
    source: "fixtures/pwm-synchronized-update/reference/pwm_update.c",
    includes: Object.freeze([
      "fixtures/pwm-synchronized-update/starter",
    ]),
    targetIds: Object.freeze([
      "armv7m-bare-metal",
    ]),
  }),
  Object.freeze({
    id: "watchdog-window-recovery",
    source:
      "fixtures/watchdog-window-recovery/reference/watchdog_window.c",
    includes: Object.freeze([
      "fixtures/watchdog-window-recovery/starter",
    ]),
    targetIds: Object.freeze([
      "armv7m-bare-metal",
    ]),
  }),
  Object.freeze({
    id: "timer-dma-handoff",
    source: "fixtures/timer-dma-handoff/reference/timer_dma_handoff.c",
    includes: Object.freeze([
      "fixtures/timer-dma-handoff/starter",
    ]),
    targetIds: Object.freeze([
      "armv7m-bare-metal",
    ]),
  }),
  Object.freeze({
    id: "timer-capture-overflow",
    source: "fixtures/timer-capture-overflow/reference/timer_capture.c",
    includes: Object.freeze([
      "fixtures/timer-capture-overflow/starter",
    ]),
    targetIds: Object.freeze([
      "armv7m-bare-metal",
    ]),
  }),
  Object.freeze({
    id: "uart-interrupt-driver",
    source: "fixtures/uart-interrupt-driver/reference/uart_driver.c",
    includes: Object.freeze([
      "fixtures/uart-interrupt-driver/starter",
    ]),
    targetIds: Object.freeze([
      "armv7m-bare-metal",
    ]),
  }),
  Object.freeze({
    id: "spi-dma-transfer",
    source: "fixtures/spi-dma-transfer/reference/spi_dma_driver.c",
    includes: Object.freeze([
      "fixtures/spi-dma-transfer/starter",
    ]),
    targetIds: Object.freeze([
      "armv7m-bare-metal",
    ]),
  }),
  Object.freeze({
    id: "binary-parser",
    source: "fixtures/binary-parser/reference/binary_parser.c",
    includes: Object.freeze([
      "fixtures/binary-parser/starter",
    ]),
  }),
  Object.freeze({
    id: "embedded-ring-buffer",
    source: "fixtures/embedded-ring-buffer/reference/ring_buffer.c",
    includes: Object.freeze([
      "fixtures/embedded-ring-buffer/starter",
    ]),
  }),
  Object.freeze({
    id: "firmware-state-machine",
    source:
      "fixtures/firmware-state-machine/reference/firmware_state_machine.c",
    includes: Object.freeze([
      "fixtures/firmware-state-machine/starter",
    ]),
  }),
]);

export const crossCompilationTargets = Object.freeze([
  Object.freeze({
    id: "armv7m-bare-metal",
    compiler: "arm-none-eabi-gcc",
    packageName: "gcc-arm-none-eabi",
    flags: Object.freeze([
      "-mcpu=cortex-m3",
      "-mthumb",
    ]),
  }),
  Object.freeze({
    id: "rv32-bare-metal",
    compiler: "riscv64-unknown-elf-gcc",
    packageName: "gcc-riscv64-unknown-elf",
    flags: Object.freeze([
      "-march=rv32imac",
      "-mabi=ilp32",
    ]),
  }),
]);

const targetById = new Map(
  crossCompilationTargets.map((target) => [target.id, target]),
);

export function parseCrossCompilationArgs(argv) {
  const targetIds = [];
  let requireTools = false;
  let help = false;

  for (let index = 0; index < argv.length; index++) {
    const argument = argv[index];
    if (argument === "--require-tools") {
      requireTools = true;
    } else if (argument === "--help" || argument === "-h") {
      help = true;
    } else if (argument === "--target") {
      index++;
      if (index >= argv.length) {
        throw new Error("--target requires a profile ID");
      }
      targetIds.push(argv[index]);
    } else if (argument.startsWith("--target=")) {
      targetIds.push(argument.slice("--target=".length));
    } else {
      throw new Error(`Unknown cross-compilation option: ${argument}`);
    }
  }

  const selectedIds = targetIds.length > 0
    ? [...new Set(targetIds)]
    : crossCompilationTargets.map((target) => target.id);
  for (const targetId of selectedIds) {
    if (!targetById.has(targetId)) {
      throw new Error(`Unknown cross-compilation target: ${targetId}`);
    }
  }

  return {
    help,
    requireTools,
    targetIds: selectedIds,
  };
}

function compilerVersion(target, spawn) {
  const result = spawn(target.compiler, ["--version"], {
    encoding: "utf8",
  });
  if (result.error?.code === "ENOENT") return null;
  if (result.error) throw result.error;
  if (result.status !== 0) {
    throw new Error(
      `${target.compiler} --version exited with status ${result.status}`,
    );
  }
  return result.stdout.split(/\r?\n/u)[0].trim();
}

function compileSource(target, source, output, spawn, repositoryRoot) {
  const includeFlags = source.includes.flatMap((directory) => [
    "-I",
    directory,
  ]);
  const args = [
    ...commonFlags,
    ...target.flags,
    ...includeFlags,
    "-c",
    source.source,
    "-o",
    output,
  ];
  const result = spawn(target.compiler, args, {
    cwd: repositoryRoot,
    encoding: "utf8",
  });
  if (result.error) throw result.error;
  if (result.status !== 0) {
    const detail = result.stderr?.trim();
    throw new Error(
      `${target.compiler} failed for ${source.id}` +
      `${detail ? `:\n${detail}` : ""}`,
    );
  }
}

export function runCrossCompilation({
  targetIds = crossCompilationTargets.map((target) => target.id),
  requireTools = false,
  repositoryRoot = resolve("."),
  spawn = spawnSync,
  log = console.log,
} = {}) {
  const selectedTargets = targetIds.map((targetId) => {
    const target = targetById.get(targetId);
    if (!target) {
      throw new Error(`Unknown cross-compilation target: ${targetId}`);
    }
    return target;
  });
  const summary = {
    compiledTargets: [],
    skippedTargets: [],
    objectCount: 0,
  };
  let temporaryRoot;

  try {
    for (const target of selectedTargets) {
      const version = compilerVersion(target, spawn);
      if (version === null) {
        if (requireTools) {
          throw new Error(
            `${target.compiler} is required; install ${target.packageName}`,
          );
        }
        summary.skippedTargets.push(target.id);
        log(`skip - ${target.id}: ${target.compiler} not found`);
        continue;
      }

      temporaryRoot ??= mkdtempSync(
        join(tmpdir(), "llm-benchmark-cross-"),
      );
      log(`target - ${target.id}: ${version}`);
      for (const source of trustedSources.filter((item) =>
        item.targetIds === undefined || item.targetIds.includes(target.id))) {
        const output = join(
          temporaryRoot,
          `${target.id}--${source.id}.o`,
        );
        compileSource(
          target,
          source,
          output,
          spawn,
          repositoryRoot,
        );
        summary.objectCount++;
        log(`ok - ${target.id}: ${source.id}`);
      }
      summary.compiledTargets.push(target.id);
    }
  } finally {
    if (temporaryRoot) {
      rmSync(temporaryRoot, { recursive: true, force: true });
    }
  }

  return summary;
}

export const crossCompilationUsage = `Usage:
  npm run cross:check -- [options]

Options:
  --target <profile>  Check one target profile; may be repeated
  --require-tools     Fail instead of skipping unavailable compilers
  -h, --help          Show this help
`;
