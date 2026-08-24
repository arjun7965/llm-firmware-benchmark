# C11 OCI Validation Image

This directory defines the registered portable C11 validation userspace. The
published image is:

```text
ghcr.io/arjun7965/llm-firmware-benchmark-c11@sha256:56e94b0d34b2bb2dd0b0eda59d7b7e502f610166bbbaf24c89e45ad94fa07193
```

`image-recipe.json` records both the upstream multi-platform index and the
independently resolved Linux/amd64 platform manifest. `Containerfile` uses the
platform digest directly, downloads or installs nothing, adds only deterministic
account metadata, and selects UID/GID 65532. The build sets timestamp zero and
embeds the full source revision through the required OCI provenance label.
`publication.json` binds the resulting platform-manifest digest to that source
revision. `runtime-contract.json` records the rootless Podman, crun, conmon,
generated configuration, and seccomp inputs observed on the calibration runner.
Run `npm run oci:recipe:check` before publication and
`npm run oci:activation:check` after committing the resulting evidence.

To reproduce the filesystem and local image from the recorded source revision:

```bash
podman pull --platform linux/amd64 \
  docker.io/library/gcc@sha256:82549aa8f90ada3236a8be70c74543132a76662ef33f0c3271ed802b81584a82
podman build \
  --build-arg SOURCE_REVISION=44b80f25ad286abbe819dd4287a61e1feaeef14e \
  --file oci/c11/Containerfile \
  --format oci \
  --network none \
  --platform linux/amd64 \
  --pull=never \
  --timestamp 0 \
  oci/c11
```

The publication workflow is deliberately two-stage. Its recipe gate does not
require existing publication evidence, so a changed recipe can be reviewed and
published before activation. A same-repository pull request must receive the
maintainer-owned `publish-oci-c11` label before its exact head commit may push a
source-revision tag. The workflow verifies that the platform base belongs to
the recorded multi-platform index, compares the push digest with a separate
registry resolution, and uploads both publication and runtime evidence. The
activation gate and calibration require those committed evidence files. The
label is removed after that one-shot provisioning step. Normal validation never
authenticates to the registry or pulls an image.

CI preloads the registered digest as a trusted provisioning step, verifies the
live runtime contract byte-for-byte, then runs every C11 trusted reference and
all controlled mutations through the hardened OCI runner. Any base, source,
toolchain, runtime, security-profile, or image change requires a new published
digest, environment revision, profile revision, and contract fingerprints.
