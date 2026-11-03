// The unit shared by the WiFi pages: a new address drops the unit connected at the old one
import { test } from "node:test";
import assert from "node:assert/strict";
import { clearJob, failure, host, log, note, runOk, setHost, unit } from "../src/store.ts";

const row = { fw: "3.0.0", board: "pro", advVersion: "3.0.0", advUpdateAvailable: false, safeMode: false };

test("editing_the_address_drops_the_connected_unit", () => {
  setHost("192.168.1.50");
  unit.value = row;
  setHost("192.168.1.50");                 // the same address: still connected
  assert.equal(unit.value, row);
  setHost("192.168.1.5");                  // another one: Install and Restore wait for a new Connect
  assert.equal(host.value, "192.168.1.5");
  assert.equal(unit.value, null);
});

test("a_job_with_no_result_counts_as_done", async () => {
  // Tauri resolves a command that returns () with null: Restore, Download backup, Export and Import
  assert.equal(await runOk("restore", async () => null), true);
  assert.equal(await runOk("restore", async () => { throw "The GBSC did not answer"; }), false);
  assert.deepEqual(log.value, ["The GBSC did not answer"]);
});

test("a_failed_job_keeps_its_reason_for_the_page", async () => {
  await runOk("restore", async () => { throw "The GBSC did not answer"; });
  assert.equal(failure.value, "The GBSC did not answer");     // shown where the button is, not only in the log
  await runOk("restore", async () => null);
  assert.equal(failure.value, null);                           // a new job starts clean
});

test("another_area_starts_with_an_empty_log", async () => {
  await runOk("backup", async () => { throw "Cannot write /x"; });
  note("Saved /y");
  clearJob();
  assert.deepEqual(log.value, []);
  assert.equal(failure.value, null);
});
