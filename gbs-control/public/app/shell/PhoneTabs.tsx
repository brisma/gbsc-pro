// Phone bottom bar: Quick, Settings, Profiles, Status, System and the Remote cell; a dot on System with an update on
// offer. SectionTabs: the sections of the open page, under the chain; they stay at the top while the page scrolls.
import { Icon } from "../art/icon.tsx";
import { PAGES, pageById } from "../layout/pages.ts";
import { route, hashOf } from "./router.ts";
import { offer } from "../store/app.ts";
import { openRemote } from "./Remote.tsx";
import { visibleSections } from "./sections.ts";
import { pageHref } from "./Sidebar.tsx";

export const tabOf = (page: string) => (page === "dev" ? "system" : page);

export function PhoneTabs() {
  const cur = tabOf(route.value.page);
  return (
    <nav class="tabs" aria-label="Pages" style="grid-template-columns:repeat(5,1fr) 56px">
      {PAGES.filter((p) => p.tab).map((p) => {
        const badge = p.id === "system" && !!offer.value;
        return (
          <a href={pageHref(p)} class={cur === p.id ? "on" : ""} aria-current={cur === p.id ? "page" : undefined}>
            <Icon name={p.icon} fill={cur === p.id} cls={badge ? "dotw" : ""} />{p.title}{badge ? <span class="vh">, update available</span> : null}
          </a>
        );
      })}
      <button type="button" class="rbtn" aria-label="Remote" title="Remote" onClick={openRemote}><Icon name="settings_remote" fill /></button>
    </nav>
  );
}

// Short names where the tab row is tight (six tabs on System)
const SHORT: Record<string, string> = { tvmenu: "TV menu", reset: "Reset", devlink: "Developer" };

export function SectionTabs() {
  const r = route.value;
  const page = pageById(r.page);
  const secs = page ? visibleSections(page) : [];
  if (!page || secs.length < 2) return null;
  return (
    <nav class={`stabs${secs.length > 4 ? " many" : ""}${secs.length > 6 ? " scroll" : ""}`} aria-label={`${page.title} sections`}>
      {secs.map((s) => {
        const on = r.section === s.id;
        const dot = page.id === "system" && s.id === "update" && !!offer.value;
        return (
          <a class={`${on ? "on" : ""}${dot ? " dot" : ""}`} href={s.to ?? hashOf({ page: page.id, section: s.id, id: null })} aria-current={on ? "location" : undefined}>
            {SHORT[s.id] ?? s.title}{dot ? <span class="vh">, update available</span> : null}
          </a>
        );
      })}
    </nav>
  );
}
