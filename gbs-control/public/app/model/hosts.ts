// system.extra_hosts: the device's check (guardExtraHostsValid, pro/api/api-guard.cpp) as the sentence the page
// shows under the field; null when the device accepts the list
const IPV4 = /^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$/;

export function hostsProblem(list: string): string | null {
  if (list.length > 63) return "The whole list fits in 63 characters";
  if (!list.length) return null;
  for (const raw of list.split(",")) {
    const name = raw.replace(/^ +| +$/g, "");
    if (!name) return "A name is empty: remove the extra comma";
    if (name.includes(" ")) return "Names have no spaces: separate them with commas";
    if (!/^[A-Za-z0-9.-]+$/.test(name)) return "Names use letters, digits, dots and dashes only";
    const l = name.toLowerCase();
    if (IPV4.test(l) && l.split(".").every((p) => Number(p) <= 255)) return "An address works without being listed";
    if (l === "localhost") return "localhost cannot be listed";
  }
  return null;
}
