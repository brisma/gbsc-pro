// What the page embeds of gen/ (size budget): esbuild loads these trimmed copies instead of the full files

// Registry: key, type, range, default, caps, options {n, v, caps?, advanced?}; flags only when set (bridged, ro,
// secret, dev); no scope, bridge kind or bool words
export function trimRegistry(text) {
  const r = JSON.parse(text);
  const settings = r.settings.map((e) => {
    const o = { key: e.key, type: e.type, min: e.min, max: e.max, def: e.def, caps: e.caps };
    if (e.options?.length) o.options = e.options.map((x) => ({ n: x.n, v: x.v, caps: x.caps, ...(x.advanced ? { advanced: true } : {}) }));
    for (const f of ["bridged", "ro", "secret", "dev"]) if (e[f]) o[f] = true;
    return o;
  });
  return JSON.stringify({ settings });
}

export function trimActions(text) {
  return JSON.stringify(JSON.parse(text).map((a) => ({ name: a.name, caps: a.caps, flags: a.flags })));
}

// esbuild plugin: gen/registry.json and gen/actions-all.json are embedded trimmed; the truth table never is
export const appDataPlugin = {
  name: "app-data",
  setup(b) {
    b.onLoad({ filter: /[\\/]gen[\\/](registry|actions-all|predicates)\.json$/ }, async (args) => {
      const { readFile } = await import("node:fs/promises");
      const text = await readFile(args.path, "utf8");
      if (args.path.endsWith("predicates.json")) throw new Error("gen/predicates.json is test data: the page must not embed it");
      return { contents: args.path.endsWith("registry.json") ? trimRegistry(text) : trimActions(text), loader: "json" };
    });
  },
};
