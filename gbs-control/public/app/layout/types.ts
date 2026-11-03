// The declarative page description: pages -> sections -> groups -> rows, data only, shared by the
// phone and PC renderers. Rows point at the firmware data (menu.json nodes, settings, actions, live readbacks).
import type { ComponentType } from "preact";
import type { PredState } from "../model/predicates.ts";

export type RowKind = "toggle" | "stepper" | "inline" | "menu" | "cards" | "picker" | "pad" | "action" | "link" | "live" | "note" | "secret";
export type Cap = "adv" | "osd" | "std";
// "menu:<menu.json id>" | "setting:<key>" | "action:<name>" | "live:<MenuLive name>" | "text:<source>"
export type Ref = `menu:${string}` | `setting:${string}` | `action:${string}` | `live:${string}` | `text:${string}`;

export interface Row {
  kind: RowKind;
  ref?: Ref;
  label?: string;          // default: the menu node's label
  icon: string;
  help?: string;           // key in texts/help.ts (default: the menu id)
  advanced?: boolean;      // web-only rows; menu rows follow the node's own Advanced flag
  cap?: Cap;
  tint?: "red" | "green" | "blue";   // the icon in that colour (Picture Red/Green/Blue)
  look?: "filter" | "scanlines" | "deint" | "hdmi" | "resolution" | "source" | "grid9";   // thumbnails of cards and pickers; grid9: nine places as a 3×3 grid
  to?: string;             // link: a hash route
  sub?: string;            // link, action: a short line under the label
  bicon?: string;          // action: the icon inside its button
  button?: string;         // action: the button text (default Run / Reset)
  confirm?: { title: string; body: string; ok: string; icon?: string };
  danger?: boolean;        // action: the red button (Factory reset)
  head?: boolean;          // action: a button in the header of the section card (Reset picture)
  secret?: string;         // secret: the write-only string setting it sets (OTA password)
  text?: string;           // note: the text
  showIf?: (s: PredState) => boolean;   // web-only rows (notes): shown only when this holds
  phoneOnly?: boolean;     // the PC layout shows it another way (Auto search: the inputs strip)
}
export interface Group { title?: string; icon?: string; advanced?: boolean; cap?: Cap; col?: 0 | 1; picker?: boolean; rows: Row[] }
export interface Section {
  id: string; title: string; icon: string; hint: string;
  group?: string;          // sidebar group title (Developer)
  cap?: Cap;
  summary?: () => string;
  groups?: Group[];
  acts?: ComponentType;    // render: the buttons of the card header
  render?: ComponentType<{ desk: boolean }>;   // a section that draws itself (register tools, Status, ...)
  bare?: boolean;          // render: draws its own groups and PC columns (System WiFi, Update, Backup)
  to?: string;             // a card that opens another page (phone System › Developer tools)
  dev?: boolean;           // shown in Dev only
  phoneOnly?: boolean;     // the PC reaches it another way (the sidebar)
}
export type PageId = "quick" | "settings" | "profiles" | "status" | "system" | "dev";
export interface Page { id: PageId; title: string; icon: string; tab: boolean; sections: Section[] }
