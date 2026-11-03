#!/usr/bin/env python3
"""tools/ota/guard.py: may a workflow run publish this version (or resume its own earlier run)?"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import guard  # noqa: E402


class GuardTest(unittest.TestCase):
    def test_newer_publishes(self):
        self.assertEqual(guard.decide("3.1.0-beta5", "3.1.0-beta4", "3.0.0", "beta", "", "abc"), "publish")
        self.assertEqual(guard.decide("3.1.0-beta1", "", "", "beta", "", "abc"), "publish")   # first publish

    def test_older_or_other_commit_refused(self):
        with self.assertRaises(SystemExit):
            guard.decide("3.1.0-beta3", "3.1.0-beta4", "", "beta", "", "abc")
        with self.assertRaises(SystemExit):                  # same version from another commit
            guard.decide("3.1.0-beta4", "3.1.0-beta4", "", "beta", "zzz", "abc")

    # A re-run keeps run_number: the live version equals ours, from this commit -> finish the earlier run
    def test_rerun_of_the_same_commit_resumes(self):
        self.assertEqual(guard.decide("3.1.0-beta4", "3.1.0-beta4", "", "beta", "abc", "abc"), "resume")

    def test_beta_above_stable(self):
        with self.assertRaises(SystemExit):
            guard.decide("3.0.0-beta9", "", "3.0.0", "beta", "", "abc")
        self.assertEqual(guard.decide("3.1.0-beta1", "", "3.0.0", "beta", "", "abc"), "publish")
        self.assertEqual(guard.decide("3.0.1", "3.0.0", "3.0.0", "stable", "", "abc"), "publish")


class WorkflowTest(unittest.TestCase):
    """The callers hold the OTA_ENABLED switch and only run for their own tags: Beta from vX.Y.Z-betaN, Stable from
    vX.Y.Z (releases are tags on main, no dev or beta branch)."""

    def load(self, name):
        import yaml
        root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".github", "workflows")
        return yaml.safe_load(open(os.path.join(root, name)))

    def test_callers_are_switched_and_ref_guarded(self):
        for name, ref in (("ota-beta.yml", "refs/tags/v"), ("ota-stable.yml", "refs/tags/v")):
            cond = self.load(name)["jobs"]["publish"]["if"]
            self.assertIn("vars.OTA_ENABLED == 'true'", cond, name)
            self.assertIn(ref, cond, name)

    def test_publish_uses_the_guard_and_an_idempotent_release(self):
        steps = self.load("ota-publish.yml")["jobs"]["publish"]["steps"]
        text = "\n".join(str(s.get("run", "")) for s in steps)
        self.assertIn("tools/ota/guard.py", text)
        self.assertIn("gh release view", text)
        self.assertIn("--clobber", text)


class WorkflowRobustnessTest(WorkflowTest):
    """The rollback version is data, not code; an AWS error is not 'no manifest yet'."""

    def steps(self):
        return {s.get("name", ""): str(s.get("run", "")) for s in self.load("ota-publish.yml")["jobs"]["publish"]["steps"]}

    def test_rollback_passes_the_version_as_an_argument(self):
        run = next(v for k, v in self.steps().items() if k.startswith("Rollback"))
        self.assertNotIn("'$RB'", run)
        self.assertIn("jq -er '.Metadata.sha256'", run)    # a missing sha256 fails instead of writing "null"

    def test_live_manifest_errors_are_not_absence(self):
        run = next(v for k, v in self.steps().items() if k.startswith("Newer than the live channel"))
        self.assertIn("NoSuchKey", run)

    # GitHub cancels a job whose runner never comes: that is "cancelled", not "failure". The publish job once ran
    # after a cancelled macOS flasher build, on its way to a release with two flashers
    def test_publish_needs_the_flashers_and_the_tests_to_have_succeeded(self):
        job = self.load("ota-publish.yml")["jobs"]["publish"]
        self.assertEqual(sorted(job["needs"]), ["flashers", "full-tests"])
        for need in job["needs"]:
            self.assertIn("""contains(fromJSON('["success", "skipped"]'), needs.%s.result)""" % need, job["if"])
        self.assertNotIn("!= 'failure'", job["if"])

    def test_nothing_is_uploaded_without_the_three_flashers(self):
        names = [s.get("name", "") for s in self.load("ota-publish.yml")["jobs"]["publish"]["steps"]]
        guard_at = next(i for i, n in enumerate(names) if n.startswith("The three flashers are here"))
        self.assertLess(guard_at, names.index("Upload the image (never over other bytes)"))
        run = next(v for k, v in self.steps().items() if k.startswith("The three flashers are here"))
        for z in ("GBSC-PRO-Flasher-macOS.zip", "GBSC-PRO-Flasher-Windows.zip", "GBSC-PRO-Flasher-Linux.zip"):
            self.assertIn(z, run)

    # A system already built for these sources is not built again: its runner may not come the next time either
    def test_flashers_are_built_only_when_missing(self):
        wf = self.load("flashers.yml")
        plan = [s for s in wf["jobs"]["plan"]["steps"] if "actions/cache/restore" in str(s.get("uses", ""))]
        self.assertEqual(sorted(s["with"]["key"].split("-")[1] for s in plan), ["linux", "macos", "windows"])
        build = wf["jobs"]["build"]
        self.assertIn("fromJSON(needs.plan.outputs.build)", str(build["strategy"]["matrix"]["include"]))
        self.assertFalse(build["strategy"]["fail-fast"])
        saves = [s for s in build["steps"] if "actions/cache/save" in str(s.get("uses", ""))]
        self.assertEqual(len(saves), 1)
        self.assertTrue(all(s["with"]["enableCrossOsArchive"] is True for s in plan + saves))
        self.assertEqual(self.load("flasher.yml")["jobs"]["flashers"]["uses"], "./.github/workflows/flashers.yml")


if __name__ == "__main__":
    unittest.main()
