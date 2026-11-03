// Host/Origin rules of the firmware (pro/api/api-guard.cpp, api-web.cpp apiGuardRequest)
export const HOSTNAME = "gbscontrol";

export function normalizeHost(input: string): string | null {
  const i = input.indexOf("://");
  let p = i >= 0 ? input.slice(i + 3) : input;
  if (p.startsWith("[")) return null;                       // IPv6: the device is IPv4 only
  let len = 0;
  while (len < p.length && p[len] !== "/" && p[len] !== ":") len++;
  p = p.slice(0, len);
  while (p.endsWith(".")) p = p.slice(0, -1);
  if (!p || p.length >= 64) return null;
  return p.toLowerCase();
}

export function isIpv4(s: string): boolean {
  const parts = s.split(".");
  return parts.length === 4 && parts.every((x) => /^\d{1,3}$/.test(x) && Number(x) <= 255);
}

function inList(host: string, list: string): boolean {
  return list.split(",").map((e) => e.trim().toLowerCase()).some((e) => e.length > 0 && e === host);
}

// simulatorLocal: "localhost" also passes (tests and the browser on the developer's machine)
export function hostAllowed(raw: string | undefined, extraHosts: string, simulatorLocal = true): boolean {
  const h = raw ? normalizeHost(raw) : null;
  if (!h) return false;
  if (isIpv4(h) || h === HOSTNAME || h === HOSTNAME + ".local") return true;
  if (simulatorLocal && h === "localhost") return true;
  return inList(h, extraHosts);
}

export function originAllowed(origin: string | undefined, extraHosts: string): boolean {
  if (!origin || origin === "null") return false;
  return hostAllowed(origin, extraHosts);
}

// guardExtraHostsValid (api-guard.cpp)
export function extraHostsValid(list: string): boolean {
  if (list.length > 63) return false;
  if (!list.length) return true;
  for (const raw of list.split(",")) {
    const name = raw.replace(/^ +| +$/g, "");
    if (!name || name.length > 63 || !/^[A-Za-z0-9.-]+$/.test(name)) return false;
    const l = name.toLowerCase();
    if (isIpv4(l) || l === "localhost") return false;
  }
  return true;
}

export function hostLostByChange(host: string | undefined, oldList: string, newList: string): boolean {
  return hostAllowed(host, oldList, false) && !hostAllowed(host, newList, false);
}
