import ts from "typescript";

const parsed = ts.parseCommandLine(process.argv.slice(2));
const program = ts.createProgram({
  rootNames: parsed.fileNames,
  options: parsed.options,
});
const emitted = program.emit();
const diagnostics = [
  ...parsed.errors,
  ...ts.getPreEmitDiagnostics(program),
  ...emitted.diagnostics,
];

for (const diagnostic of diagnostics) {
  const location = diagnostic.file && diagnostic.start !== undefined
    ? `${diagnostic.file.fileName}:${
      ts.getLineAndCharacterOfPosition(diagnostic.file, diagnostic.start).line + 1}`
    : "typescript";
  console.error(`${location}: ${ts.flattenDiagnosticMessageText(
    diagnostic.messageText,
    "\n",
  )}`);
}

if (diagnostics.some((diagnostic) => diagnostic.category === ts.DiagnosticCategory.Error)) {
  process.exitCode = 1;
}
