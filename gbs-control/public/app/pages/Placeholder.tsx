// A page or section without content of its own: its heading only
import { Icon } from "../art/icon.tsx";
import { pageById } from "../layout/pages.ts";
import { route } from "../shell/router.ts";

export function Placeholder() {
  const r = route.value;
  const p = pageById(r.page);
  const s = p?.sections.find((x) => x.id === r.section);
  if (!s) return <div class="empty-page" />;
  return (
    <div class="sechead">
      <span class="vis"><Icon name={s.icon} /></span>
      <span><h2>{s.title}</h2><small>{s.hint}</small></span>
    </div>
  );
}
