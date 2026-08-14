function extractOpenCodeAnswer(stdout) {
  const lines = stdout.split(/\r?\n/u)
    .filter((line) => line.trim() !== "");
  if (lines.length < 2) return undefined;

  let events;
  try {
    events = lines.map((line) => JSON.parse(line));
  } catch {
    return undefined;
  }
  if (!events.every((event) =>
    event && typeof event === "object" && !Array.isArray(event) &&
    typeof event.type === "string" &&
    typeof event.timestamp === "number" &&
    typeof event.sessionID === "string")) {
    return undefined;
  }
  if (!events.some((event) =>
    event.type === "step_start" || event.type === "step_finish")) {
    return undefined;
  }

  const sessionIds = new Set(events.map((event) => event.sessionID));
  if (sessionIds.size !== 1) {
    throw new TypeError("OpenCode event stream contains multiple sessions");
  }
  const textParts = events
    .filter((event) => event.type === "text")
    .map((event) => event.part)
    .filter((part) =>
      part && typeof part === "object" && !Array.isArray(part) &&
      part.type === "text" && typeof part.text === "string")
    .map((part) => part.text);
  if (textParts.length === 0) {
    throw new TypeError(
      "OpenCode event stream does not contain a text result",
    );
  }
  return textParts.join("");
}

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
    if (error instanceof SyntaxError) {
      return extractOpenCodeAnswer(stdout) ?? stdout;
    }
    throw error;
  }

  return stdout;
}
