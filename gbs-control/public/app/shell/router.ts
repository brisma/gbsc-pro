// Hash routes: #/quick, #/settings/<section>, #/profiles[/<id>], #/status,
// #/system/<section>, #/dev/<section>; default #/quick. A page with sections opens its first one.
import { signal } from "@preact/signals";
import { pageById, type PageId } from "../layout/pages.ts";

export interface Route { page: PageId; section: string | null; id: string | null }

// Bookmarks of sections that moved keep working
const OLD_SECTIONS: Record<string, string> = { "settings/screen": "scaler" };

export function parseHash(hash: string): Route {
  const parts = hash.replace(/^#\/?/, "").split("/").filter(Boolean);
  const page = pageById(parts[0] ?? "");
  if (!page) return { page: "quick", section: null, id: null };
  if (page.id === "profiles") return { page: "profiles", section: null, id: parts[1] ?? null };
  if (!page.sections.length) return { page: page.id, section: null, id: null };
  const wanted = OLD_SECTIONS[`${page.id}/${parts[1]}`] ?? parts[1];
  const sec = page.sections.find((s) => s.id === wanted) ?? page.sections[0];
  return { page: page.id, section: sec.id, id: null };
}

export function hashOf(r: Route): string {
  return "#/" + r.page + (r.section ? "/" + r.section : r.id ? "/" + r.id : "");
}

const initial = typeof location === "undefined" ? parseHash("") : parseHash(location.hash);
export const route = signal<Route>(initial);

// The hash always names the full route (#/settings -> #/settings/input) without a new history entry
function normalise(): void {
  const r = parseHash(location.hash);
  const h = hashOf(r);
  if (location.hash !== h) history.replaceState(null, "", h);
  route.value = r;
}

export function startRouter(): void {
  normalise();
  window.addEventListener("hashchange", normalise);
}

export function navigate(page: PageId, section?: string | null, id?: string | null): void {
  const h = hashOf(parseHash(`#/${page}${section ? "/" + section : id ? "/" + id : ""}`));
  if (location.hash === h) return;
  location.hash = h;
}
