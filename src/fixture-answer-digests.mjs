import { createHash } from "node:crypto";

function lengthPrefix(length) {
  const prefix = Buffer.alloc(8);
  prefix.writeBigUInt64BE(BigInt(length));
  return prefix;
}

export function sha256(content) {
  return createHash("sha256").update(content).digest("hex");
}

export function canonicalBundleSha256(files) {
  if (!Array.isArray(files) || files.length === 0) {
    throw new TypeError("answer bundle files must be a non-empty array");
  }
  const digest = createHash("sha256");
  digest.update(lengthPrefix(files.length));
  for (const file of files) {
    if (
      !file ||
      typeof file.path !== "string" ||
      file.path === "" ||
      !(typeof file.content === "string" || Buffer.isBuffer(file.content))
    ) {
      throw new TypeError("answer bundle file is invalid");
    }
    const path = Buffer.from(file.path, "utf8");
    const content = Buffer.isBuffer(file.content)
      ? file.content
      : Buffer.from(file.content, "utf8");
    digest.update(lengthPrefix(path.length));
    digest.update(path);
    digest.update(lengthPrefix(content.length));
    digest.update(content);
  }
  return digest.digest("hex");
}

export function summarizeAnswerFiles(files, { bundle = false } = {}) {
  if (!Array.isArray(files) || files.length === 0) {
    throw new TypeError("answer files must be a non-empty array");
  }
  const summaries = files.map((file) => {
    const content = Buffer.isBuffer(file.content)
      ? file.content
      : Buffer.from(file.content, "utf8");
    return {
      path: file.path,
      byteLength: content.length,
      sha256: sha256(content),
    };
  });
  return {
    files: summaries,
    byteLength: summaries.reduce(
      (total, file) => total + file.byteLength,
      0,
    ),
    sha256: bundle
      ? canonicalBundleSha256(files)
      : summaries[0].sha256,
  };
}
