// PC sidebar: the five pages, the open page's sections as sub-items (group titles with icon and rule), Developer in
// Dev. With an update on offer: a "1" on System and on Update, and System opens on Update.
import { Logo } from "../art/logo.tsx";
import { Icon } from "../art/icon.tsx";
import { PAGES, GROUP_ICONS, type PageDef } from "../layout/pages.ts";
import { route, hashOf } from "./router.ts";
import { offer, devOn } from "../store/app.ts";
import { visibleSections } from "./sections.ts";

const Badge = () => <><span class="sp" /><span class="badge" aria-hidden="true">1</span><span class="vh">, update available</span></>;

// Where a page link lands: its first section, or Update while an update is on offer
export const pageHref = (p: PageDef) => hashOf({ page: p.id, section: p.id === "system" && offer.value ? "update" : null, id: null });

function Subs({ page }: { page: PageDef }) {
  const r = route.value;
  let group: string | undefined;
  return (
    <>
      {visibleSections(page).filter((s) => !s.to).map((s) => {
        const head = s.group && s.group !== group ? (group = s.group) : null;
        return (
          <>
            {head ? <div class="sidegrp"><Icon name={GROUP_ICONS[head] ?? "folder"} /><span>{head}</span></div> : null}
            <a class={`subl${r.section === s.id ? " on" : ""}`} href={hashOf({ page: page.id, section: s.id, id: null })}
               aria-current={r.section === s.id ? "location" : undefined}>{s.title}{page.id === "system" && s.id === "update" && offer.value ? <Badge /> : null}</a>
          </>
        );
      })}
    </>
  );
}

export function Sidebar() {
  const r = route.value;
  const withSubs = (id: string) => (id === "settings" || id === "system" || id === "dev") && r.page === id;
  const link = (p: PageDef) => {
    const on = r.page === p.id;
    return (
      <>
        <a href={pageHref(p)} class={`${on ? "on" : ""}${withSubs(p.id) ? " par" : ""}`} aria-current={on ? "page" : undefined}>
          <Icon name={p.icon} fill={on} />{p.title}{p.id === "system" && offer.value ? <Badge /> : null}
        </a>
        {withSubs(p.id) ? <Subs page={p} /> : null}
      </>
    );
  };
  const dev = PAGES.find((p) => p.id === "dev")!;
  return (
    <nav class="side" aria-label="Pages">
      <span class="brand-side"><Logo height={26} /></span>
      {PAGES.filter((p) => p.tab).map(link)}
      {devOn.value ? link(dev) : null}
    </nav>
  );
}
