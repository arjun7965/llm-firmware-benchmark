import {
  checkHilReadiness,
  hilCatalogReference,
  hilUsage,
  loadHilCatalog,
  parseHilArgs,
  selectHilTargets,
} from "../src/hil-targets.mjs";

try {
  const options = parseHilArgs(process.argv.slice(2));
  if (options.help) {
    console.log(hilUsage);
  } else {
    const catalog = loadHilCatalog();
    const targets = selectHilTargets(catalog, options.targetIds);
    const summary = {
      catalog: hilCatalogReference(catalog),
      policy: catalog.policy.role,
      targetIds: targets.map((target) => target.id),
    };
    if (options.probeTools) {
      summary.readiness = checkHilReadiness(catalog, {
        requireTools: options.requireTools,
        targetIds: options.targetIds,
      });
    }
    console.log("HIL catalog check complete:", JSON.stringify(summary));
  }
} catch (error) {
  console.error(error.message);
  process.exitCode = 1;
}
