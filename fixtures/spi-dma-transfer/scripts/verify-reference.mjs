import { spawnSync } from "node:child_process";
import {
  mkdtempSync,
  rmSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";

const fixtureRoot = fileURLToPath(new URL("../", import.meta.url));
const temporaryRoot = mkdtempSync(
  join(tmpdir(), "spi-dma-transfer-self-test-"),
);
const binary = join(temporaryRoot, "public-tests");

function run(command, args, options = {}) {
  const result = spawnSync(command, args, {
    cwd: fixtureRoot,
    stdio: "inherit",
    timeout: 30_000,
    ...options,
  });
  if (result.error) throw result.error;
  if (result.status !== 0) {
    throw new Error(`${command} exited with status ${result.status}`);
  }
}

try {
  run("cc", [
    "-std=c11",
    "-Wall",
    "-Wextra",
    "-Werror",
    "-pedantic",
    "-Istarter",
    "-Imocks",
    "reference/spi_dma_driver.c",
    "mocks/mock_spi_dma.c",
    "tests/public/test_spi_dma_driver.c",
    "-o",
    binary,
  ]);
  run(binary, []);
  console.log("SPI DMA transfer trusted reference passed.");
} finally {
  rmSync(temporaryRoot, { recursive: true, force: true });
}
