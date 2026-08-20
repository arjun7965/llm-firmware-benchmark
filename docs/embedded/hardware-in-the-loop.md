# Optional Hardware-in-the-Loop Validation

Hardware-in-the-loop (HIL) runs are supplemental engineering evidence. They do
not change benchmark scores, replace deterministic host mocks, alter prompts,
or make results from different lab configurations directly comparable. The
required scoring path remains the active fixture and its isolated host-side
validation.

The repository supplies a strict target catalog, a common test protocol,
dependency-readiness checks, and a versioned result contract. A lab supplies
the physical board, controlled wiring, vendor software, and a trusted smoke
image built from a reviewed source revision. No vendor SDK, programmer, or
prebuilt firmware is redistributed here.

## Representative Boards

| Catalog ID | Board and MCU | Debug probe | Official details |
| --- | --- | --- | --- |
| `stm32-nucleo-f446re` | ST [NUCLEO-F446RE](https://www.st.com/en/evaluation-tools/nucleo-f446re.html), STM32F446RE Cortex-M4F | On-board ST-LINK/V2-1 over SWD | [MCU datasheet](https://www.st.com/resource/en/datasheet/stm32f446re.pdf), [board user manual](https://www.st.com/resource/en/user_manual/dm00105823.pdf) |
| `nxp-frdm-mcxn947` | NXP [FRDM-MCXN947](https://www.nxp.com/design/design-center/development-boards-and-designs/FRDM-MCXN947), MCXN947 dual Cortex-M33 | On-board MCU-Link over SWD | [MCU datasheet](https://www.nxp.com/docs/en/data-sheet/MCXNP184M150F70.pdf), [board documentation](https://www.nxp.com/design/design-center/development-boards-and-designs/FRDM-MCXN947#documentation) |
| `ti-lp-mspm0g3507` | TI [LP-MSPM0G3507](https://www.ti.com/tool/LP-MSPM0G3507), MSPM0G3507 Cortex-M0+ | On-board XDS110 over SWD | [MCU datasheet](https://www.ti.com/lit/ds/symlink/mspm0g3507.pdf), [board user guide](https://www.ti.com/lit/ug/slau873d/slau873d.pdf) |

These boards exercise three vendor ecosystems and multiple Cortex-M profiles.
They are not aliases for the fictional MMIO in benchmark prompts. A HIL smoke
image adapts the common behavioral protocol to one specific board; it does not
turn a host fixture into vendor-specific model input.

## Tools, SDKs, and Licenses

Every lab must pin one reviewed revision, retain its installer or source
checksum, and record the exact resolved version in the HIL report. The catalog
uses `not-vendored` for every external dependency: install it directly from
the publisher and accept its terms there.

| Target | Programmer | SDK dependency | Toolchain | License record |
| --- | --- | --- | --- | --- |
| STM32 | [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html) CLI | [STM32CubeF4](https://www.st.com/en/embedded-software/stm32cubef4.html), exposed through `HIL_STM32CUBEF4_ROOT` | [Arm GNU Toolchain](https://gitlab.arm.com/tooling/gnu-toolchains-for-arm/-/releases), `arm-none-eabi` | Programmer terms accompany its download. STM32CubeF4 uses SLA0048, [package additional terms](https://www.st.com/content/ccc/resource/legal/legal_agreement/additional_license_terms/group0/fb/0b/a4/05/ba/01/48/85/additional-license-terms-stm32cubef4/files/additional-license-terms-stm32cubef4.html/jcr%3Acontent/translations/en.additional-license-terms-stm32cubef4.html), and component notices. |
| NXP | [LinkServer](https://www.nxp.com/design/design-center/software/development-software/mcuxpresso-software-and-tools-/linkserver-for-microcontrollers%3ALINKSERVER) | [MCUXpresso SDK](https://www.nxp.com/design/design-center/software/development-software/mcuxpresso-software-and-tools-/mcuxpresso-software-development-kit-sdk%3AMCUXpresso-SDK), exposed through `HIL_MCUXPRESSO_SDK_ROOT` | Arm GNU Toolchain, `arm-none-eabi` | LinkServer uses the terms presented at download. The SDK documents its online-hosting terms, repository-specific licenses, and merged SBOM in the [MCUXpresso SDK documentation](https://mcuxpresso.nxp.com/mcuxsdk/latest/html/index.html). |
| TI | [UniFlash](https://www.ti.com/tool/UNIFLASH) `dslite` CLI | [MSPM0 SDK](https://www.ti.com/tool/MSPM0-SDK), exposed through `HIL_MSPM0_SDK_ROOT` | Arm GNU Toolchain, `arm-none-eabi` | UniFlash terms accompany its download. The MSPM0 SDK is mostly BSD-3-Clause but contains component-specific TI commercial and third-party terms; retain and review its manifest. |

Arm GNU Toolchain releases contain components under GNU and permissive
licenses. Use the release manifest as the authority rather than assigning one
license to the entire binary distribution.

## Catalog and Readiness Checks

Validate the committed metadata without touching hardware:

```bash
npm run hil:check
```

The output includes the canonical catalog SHA-256 to copy into a result report.

Check the selected compiler, programmer CLI, and SDK root. This invokes only
fixed version argv and filesystem existence checks; it does not enumerate,
erase, program, reset, or open a serial device:

```bash
npm run hil:check -- --target stm32-nucleo-f446re --probe-tools
npm run hil:check -- --target stm32-nucleo-f446re --require-tools
```

Missing optional dependencies are reported as skipped with `--probe-tools` and
are fatal with `--require-tools`. The catalog is defined in
`hil-targets.json`; its runtime and JSON Schema validators reject unofficial
board-document domains, arbitrary command strings, missing license records,
and any policy that could affect required scoring.

## Lab Safety and Isolation

Before a HIL run:

1. Reserve one board and one probe under an exclusive lab lock. Match the probe
   serial against a private allowlist; only its SHA-256 belongs in the report.
2. Use a dedicated evaluation board whose flash may be erased. Disconnect
   unrelated targets, shields, external power, and production hardware.
3. Review voltage, jumper, reset, and connector details in that exact board
   revision's user manual. Record a wiring revision instead of free-form lab
   paths or operator data.
4. Build a trusted board-specific smoke image from a reviewed commit using the
   pinned SDK and compiler. Record the source commit and final image SHA-256.
5. Disable network access for build execution, flashing, serial capture, and
   evidence collection. Stage all dependencies before taking the lab offline.
6. Configure hard deadlines for every programmer and serial operation. A
   timeout is a failed test; it is never an instruction to retry another probe.

Flashing and reset operations overwrite the selected evaluation board. The lab
adapter must display the resolved catalog ID and hashed probe identity before
the first write and require a deliberate operator invocation. Recovery should
use the vendor-documented boot or debug path for the same board, never a broad
USB-device match.

## Common Test Protocol

Each board adapter must execute these tests in order and retain separate
evidence for each one:

1. `probe-identity` — enumerate exactly one allowlisted probe and hash its
   serial number. This is the only non-destructive phase.
2. `flash-verify` — erase, program the trusted image, then use the programmer's
   verification operation to match its digest.
3. `reset-boot` — issue a probe reset and require one clean boot marker within
   the board-specific deadline.
4. `uart-handshake` — send a fresh nonce over the probe virtual COM port and
   require an exact response. Do not use a fixed success string alone.
5. `gpio-loopback` — with the reviewed lab loopback wiring, verify low, high,
   and edge observations without exceeding documented electrical limits.
6. `watchdog-reset` — stop feeding the watchdog, require a reset inside the
   declared interval, and verify the reset-cause marker after reboot.

A phase is `pass` or `fail`; skipped tests are not valid completed HIL runs.
Evidence files remain private and are content-addressed by SHA-256 in the
report. A successful report requires all six phases to pass.

## Result Reports

`schemas/hil-validation-report.schema.json` defines the portable result. Its
runtime validator additionally cross-checks relationships that JSON Schema
cannot express: catalog fingerprint, target-specific dependency names, test
order, full coverage, and the aggregate success value.

Validate a lab-produced report:

```bash
npm run hil:report -- --report /private/path/hil-report.json
```

Reports must include:

- the canonical catalog SHA-256 and schema version;
- catalog target ID, board hardware revision, wiring revision, and hashed
  probe serial;
- trusted firmware source commit and image SHA-256;
- exact programmer, compiler, and SDK versions;
- timestamps, duration, pass/fail result, and private-evidence SHA-256 for all
  six tests; and
- `success: true` only when every required test passed.

Keep raw reports and evidence outside Git. Before publishing a report, remove
absolute paths, raw USB serials, usernames, hostnames, device nodes, operator
identity, and vendor-account data. HIL evidence is disclosed alongside host
results as supplemental context, never merged into benchmark scores.
