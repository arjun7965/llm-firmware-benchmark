export function extractAnswer(stdout) {
  if (typeof stdout !== "string") {
    throw new TypeError("result stdout must be a string");
  }

  try {
    const envelope = JSON.parse(stdout);
    if (envelope && typeof envelope === "object" && !Array.isArray(envelope)) {
      if (typeof envelope.result === "string") return envelope.result;
      const metadataKeys = [
        "session_id",
        "uuid",
        "usage",
        "modelUsage",
        "total_cost_usd",
      ];
      if (metadataKeys.some((key) => key in envelope)) {
        throw new TypeError(
          "provider metadata envelope does not contain a string result",
        );
      }
    }
  } catch (error) {
    if (error instanceof SyntaxError) return stdout;
    throw error;
  }

  return stdout;
}
