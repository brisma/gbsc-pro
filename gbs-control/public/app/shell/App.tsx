// The app: phone layout (header, page, bottom bar) below 960 px, PC layout (sidebar + column, the window scrolls)
// above; the signal chain tops every page; sheets and toasts on top. data-ready marks the first complete load (settings and schema), for tests and speed checks.
import { useEffect } from "preact/hooks";
import { device, desk, devOn, ready, mode } from "../store/app.ts";
import { route, startRouter, navigate } from "./router.ts";
import { Header, Notices } from "./Header.tsx";
import { PhoneTabs, SectionTabs } from "./PhoneTabs.tsx";
import { Chain } from "./Chain.tsx";
import { Footer } from "./Footer.tsx";
import { Sidebar } from "./Sidebar.tsx";
import { SheetHost } from "./Sheet.tsx";
import { Toasts } from "./Toasts.tsx";
import { Placeholder } from "../pages/Placeholder.tsx";
import { SectionsPage } from "../sections/SectionsPage.tsx";
import { pageById } from "../layout/pages.ts";
import { QuickPage } from "../pages/quick/QuickPage.tsx";
import { ProfilesPage } from "../pages/profiles/ProfilesPage.tsx";
import { StatusPage } from "../pages/status/StatusPage.tsx";
import "../pages/system/renders.ts";
import "../pages/dev/renders.ts";
import { openAcc } from "../rows/accordion.ts";

function Page() {
  const p = pageById(route.value.page);
  if (p && (p.id === "settings" || p.id === "system" || p.id === "dev")) return <SectionsPage page={p} />;
  if (p?.id === "quick") return <QuickPage />;
  if (p?.id === "profiles") return <ProfilesPage />;
  if (p?.id === "status") return <StatusPage />;
  return <Placeholder />;
}

export function App() {
  useEffect(() => { startRouter(); void device.start(); }, []);
  const r = route.value;
  // The Developer page lives in Dev only (a bookmark or a mode change elsewhere lands on System)
  useEffect(() => { if (ready.value && r.page === "dev" && !devOn.value) navigate("system"); }, [ready.value, r.page, devOn.value]);
  useEffect(() => {   // accordions close and the page starts from its top when the page or section changes
    openAcc.value = null;
    if (desk.value) window.scrollTo(0, 0); else document.querySelector(".ui .list")?.scrollTo(0, 0);
  }, [r.page, r.section]);
  useEffect(() => { if (ready.value) document.getElementById("boot")?.remove(); }, [ready.value]);   // the inline loader of index.html.tpl
  const isDesk = desk.value;
  const c = device.connection.value;
  const sig = device.info.value?.signal;
  const searching = device.info.value?.input?.state === "searching";
  const cls = ["ui", isDesk ? "desk" : "", sig?.none && !searching ? "lost" : "", searching ? "search" : "",
    c === "reconnecting" || c === "offline" ? "off" : ""].filter(Boolean).join(" ");
  const page = <div class="page" data-page={r.page} data-mode={mode.value}><Chain />{isDesk ? null : <SectionTabs />}<Page /></div>;
  return (
    <div class={cls} data-ready={ready.value ? "1" : undefined}>
      {isDesk ? (
        <>
          <Sidebar />
          <div class="col"><div class="top"><Header desk /><Notices /></div><main class="list">{page}</main><Footer /><SheetHost /><Toasts /></div>
        </>
      ) : (
        <><div class="top"><Header desk={false} /><Notices /></div><main class="list">{page}<Footer /></main><PhoneTabs /><SheetHost /><Toasts /></>
      )}
    </div>
  );
}
