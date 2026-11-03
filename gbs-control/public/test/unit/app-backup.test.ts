// Backup file check before a restore: the rules of the firmware's backupValidate (pro/api/api-backup.cpp), so a wrong
// file is refused in the page with its line instead of after the upload
import { test } from "node:test";
import assert from "node:assert/strict";
import { checkBackup, backupErrorText, BACKUP_MAX_BYTES } from "../../app/model/backup.ts";
import { startSim } from "../../sim/server.ts";
import { req, fixturePage } from "./sim-helpers.ts";

const REAL = [
  "# gbsc-backup 1",
  "[file cfg/device.cfg]",
  "schema=1",
  "osd.theme=2",
  "system.menu_mode=1",
  "[file cfg/current.cfg]",
  "schema=1",
  "picture.brightness=140",
  "[profiles]",
  "[raw cfg/profiles.cfg]",
  "p00=12",
  "[profile p00]",
  "profile.name=Mega Drive",
  "profile.region=PAL",
  "reg.3.0=1a2b",
  "[profile p07]",
  "# a comment",
  "",
  "profile.name=Saturn",
  "",
].join("\n");

test("backup file check accepts a real backup and refuses others", async () => {
  assert.deepEqual(checkBackup(REAL), { ok: true, profiles: 2, replacesProfiles: true, adv: 0, replacesAdv: false });
  assert.deepEqual(checkBackup(REAL.replace(/\n/g, "\r\n")), { ok: true, profiles: 2, replacesProfiles: true, adv: 0, replacesAdv: false });   // saved on Windows
  assert.deepEqual(checkBackup("\n\n" + REAL), { ok: true, profiles: 2, replacesProfiles: true, adv: 0, replacesAdv: false });                 // blank lines first
  assert.deepEqual(checkBackup("# gbsc-backup 1\n[file cfg/device.cfg]\nosd.theme=1\n"), { ok: true, profiles: 0, replacesProfiles: false, adv: 0, replacesAdv: false });

  // the file the device (here the simulator) gives for Download backup
  const sim = await startSim({ port: 0, wsPort: 0, pagePath: fixturePage });
  try {
    await req(sim.url, "POST", "/api/profiles/saveas", { form: { name: "Mega Drive", region: "PAL" } });
    const r = await req(sim.url, "GET", "/api/system/backup");
    assert.equal(r.status, 200);
    const c = checkBackup(r.text);
    assert.ok(c.ok, JSON.stringify(c));
  } finally { await sim.stop(); }

  const bad = (text: string, error: string, line: number) => assert.deepEqual(checkBackup(text), { ok: false, error, line }, text.slice(0, 60));
  bad("", "header", 0);
  bad("hello\n# gbsc-backup 1\n[profiles]\n", "header", 1);
  bad("# gbsc-backup 2\n[profiles]\n", "header", 1);
  bad("# gbsc-backup 1\n", "empty", 0);
  bad("# gbsc-backup 1\n# only comments\n\n", "empty", 0);
  bad("# gbsc-backup 1\n[file cfg/other.cfg]\n", "section", 2);
  bad("# gbsc-backup 1\n[profiles]\n[profile p36]\n", "section", 3);                        // 36 profiles: p00..p35
  bad("# gbsc-backup 1\n[profile p07]\nprofile.name=x\n", "section", 2);          // a profile before [profiles]
  bad("# gbsc-backup 1\n[raw cfg/profiles.cfg]\n", "section", 2);
  bad("# gbsc-backup 1\n[profiles\n", "section", 2);
  bad("# gbsc-backup 1\nosd.theme=1\n", "syntax", 2);                             // a value outside a section
  bad("# gbsc-backup 1\n[profiles]\nosd.theme=1\n", "syntax", 3);                 // the marker has no lines
  bad("# gbsc-backup 1\n[file cfg/device.cfg]\nOsd.Theme=1\n", "syntax", 3);      // keys are lowercase
  bad("# gbsc-backup 1\n[file cfg/device.cfg]\nnot a setting\n", "syntax", 3);
  bad("# gbsc-backup 1\n[profiles]\n[profile p01]\nprofile.region=PAL\n[profile p02]\nprofile.name=B\n", "syntax", 3);   // no name: its section line
  bad("# gbsc-backup 1\n[profiles]\n[profile p01]\nprofile.name=\n", "syntax", 3);
  // lines over 127 characters are dropped by the device's line reader, as here
  assert.ok(checkBackup(`# gbsc-backup 1\n[file cfg/device.cfg]\n${"x".repeat(140)}\nosd.theme=1\n`).ok);
  assert.deepEqual(checkBackup("# gbsc-backup 1\n[profiles]\n" + "a=1\n".repeat(BACKUP_MAX_BYTES / 4)), { ok: false, error: "size", line: 0 });

  assert.match(backupErrorText({ ok: false, error: "header", line: 1 }), /gbsc-backup 1/);
  assert.match(backupErrorText({ ok: false, error: "syntax", line: 3 }), /line 3/i);
  assert.match(backupErrorText({ ok: false, error: "size", line: 0 }), /640 KB/);
});

test("backup check takes the ADV presets and macros and their text rules", () => {
  const base = "# gbsc-backup 1\n[file cfg/device.cfg]\nosd.theme=Dark\n[profiles]\n[adv]\n";
  const ok = checkBackup(base + "[adv preset p02]\nname: Soft\n# a comment\n\n          '0x0A': '0x10'\n[adv macro m31]\nname: \"ACE on\"\ncommands:\n  - wm 42 40 80 80\n");
  assert.deepEqual(ok, { ok: true, profiles: 0, replacesProfiles: true, adv: 2, replacesAdv: true });
  assert.deepEqual(checkBackup(base + "[adv preset p02]\ndescription: no name\n"), { ok: false, error: "syntax", line: 6 });
  assert.deepEqual(checkBackup(base + "[adv macro m01]\nname: x\nbad\x01\n"), { ok: false, error: "syntax", line: 8 });
  assert.deepEqual(checkBackup(base + "[adv macro m01]\nname: x\n" + "  - w 42 0A 10   # padding to pass 2 KB\n".repeat(60)),
    { ok: false, error: "syntax", line: 6 });                                               // a macro is at most 2 KB
  assert.deepEqual(checkBackup("# gbsc-backup 1\n[adv preset p01]\nname: x\n"), { ok: false, error: "section", line: 2 });
  assert.deepEqual(checkBackup(base + "[adv preset p16]\nname: x\n"), { ok: false, error: "section", line: 6 });
  assert.deepEqual(checkBackup("# gbsc-backup 1\n[file cfg/device.cfg]\nosd.theme=Dark\n"), { ok: true, profiles: 0, replacesProfiles: false, adv: 0, replacesAdv: false });
});
