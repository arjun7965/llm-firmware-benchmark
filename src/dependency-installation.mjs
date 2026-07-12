import { createHash } from "node:crypto";
import {
  existsSync,
  lstatSync,
  readFileSync,
  readdirSync,
} from "node:fs";
import { relative, resolve, sep } from "node:path";
import { fileURLToPath } from "node:url";

const maximumFiles = 50_000;
const maximumBytes = 512 * 1024 * 1024;

function asPath(value) {
  return value instanceof URL ? fileURLToPath(value) : value;
}

function requireContained(root, path) {
  const relativePath = relative(root, path);
  if (
    relativePath === "" ||
    relativePath === ".." ||
    relativePath.startsWith(`..${sep}`)
  ) {
    throw new TypeError("dependency installation entry escapes its root");
  }
}

function requireImmutableEntry(metadata, name, requiredUid) {
  if (metadata.uid !== requiredUid || (metadata.mode & 0o022) !== 0) {
    throw new TypeError(
      `${name} must be owned by the approved user and non-writable`,
    );
  }
}

export function dependencyInstallationFingerprint(path, {
  requiredUid = 0,
} = {}) {
  const root = resolve(asPath(path));
  if (!existsSync(root)) {
    throw new TypeError("dependency installation does not exist");
  }
  const rootMetadata = lstatSync(root);
  if (rootMetadata.isSymbolicLink() || !rootMetadata.isDirectory()) {
    throw new TypeError(
      "dependency installation must be a non-symlink directory",
    );
  }
  requireImmutableEntry(
    rootMetadata,
    "dependency installation root",
    requiredUid,
  );

  const fingerprint = createHash("sha256");
  let fileCount = 0;
  let totalBytes = 0;
  function visit(directory, relativeDirectory = "") {
    const names = readdirSync(directory).sort();
    for (const name of names) {
      if (name.includes("\0")) {
        throw new TypeError("dependency installation entry name is invalid");
      }
      const relativePath = relativeDirectory
        ? `${relativeDirectory}/${name}`
        : name;
      const entryPath = resolve(directory, name);
      requireContained(root, entryPath);
      const metadata = lstatSync(entryPath);
      if (
        metadata.isSymbolicLink() ||
        (!metadata.isDirectory() && !metadata.isFile())
      ) {
        throw new TypeError(
          `dependency installation entry ${relativePath} has an ` +
          "unsupported type",
        );
      }
      requireImmutableEntry(
        metadata,
        `dependency installation entry ${relativePath}`,
        requiredUid,
      );
      const mode = (metadata.mode & 0o777).toString(8).padStart(3, "0");
      if (metadata.isDirectory()) {
        fingerprint.update(`directory\0${relativePath}\0${mode}\0`);
        visit(entryPath, relativePath);
        continue;
      }
      fileCount++;
      totalBytes += metadata.size;
      if (fileCount > maximumFiles || totalBytes > maximumBytes) {
        throw new TypeError(
          "dependency installation exceeds attestation limits",
        );
      }
      const contentSha256 = createHash("sha256")
        .update(readFileSync(entryPath))
        .digest("hex");
      fingerprint.update(
        `file\0${relativePath}\0${mode}\0${metadata.size}\0` +
        `${contentSha256}\0`,
      );
    }
  }
  visit(root);
  if (fileCount === 0) {
    throw new TypeError("dependency installation must contain files");
  }
  return fingerprint.digest("hex");
}

export function attestDependencyInstallation(profile) {
  const install = profile.dependencyInstall;
  if (
    install?.kind !== "lockfile" ||
    !Object.hasOwn(install, "installRoot")
  ) {
    throw new TypeError(
      `validation profile ${profile.id} has no runtime-attested ` +
      "dependency installation",
    );
  }
  const actualSha256 = dependencyInstallationFingerprint(install.installRoot);
  if (actualSha256 !== install.installSha256) {
    throw new TypeError(
      `validation profile ${profile.id} dependency installation sha256 ` +
      "does not match",
    );
  }
  return {
    installRoot: install.installRoot,
    mountPath: install.mountPath,
    sha256: actualSha256,
  };
}
