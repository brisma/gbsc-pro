#!/usr/bin/env python3
"""Tests of tools/changelog.py: the CHANGELOG.md sections a release reads and writes.

Run: python tools/test_changelog.py -v
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import changelog  # noqa: E402

DOC = """# Changelog

Intro.

## 3.1.0-beta2

### Fixed
- A crash.

## 3.1.0-beta1 - 2026-12-01

Summary of 3.1.0.

### Menus
- A new row.

## 3.0.0 - 2026-11-20

Summary.

### Fixed
- #22 export.

## 2.4.1 - 2026-05-02

- Old.
"""


class SectionTest(unittest.TestCase):
    def test_body_of_a_version(self):
        self.assertEqual(changelog.section(DOC, "3.0.0"), "Summary.\n\n### Fixed\n- #22 export.")
        self.assertEqual(changelog.section(DOC, "3.1.0-beta1"), "Summary of 3.1.0.\n\n### Menus\n- A new row.")

    def test_body_of_the_beta_in_the_works(self):
        self.assertEqual(changelog.section(DOC, "3.1.0-beta2"), "### Fixed\n- A crash.")

    def test_unknown_version(self):
        self.assertIsNone(changelog.section(DOC, "9.9.9"))
        self.assertIsNone(changelog.section(DOC, "3.1.0"))   # only its betas are there


class BetaTest(unittest.TestCase):
    def test_the_section_in_the_works_gets_the_date(self):
        out = changelog.beta(DOC, "3.1.0-beta2", "2026-12-08")
        self.assertIn("Intro.\n\n## 3.1.0-beta2 - 2026-12-08\n\n### Fixed\n- A crash.\n\n## 3.1.0-beta1 - 2026-12-01", out)

    def test_nothing_to_release(self):
        with self.assertRaises(changelog.ChangelogError):
            changelog.beta(DOC, "3.1.0-beta3", "2026-12-08")                                 # no such section
        with self.assertRaises(changelog.ChangelogError):
            changelog.beta(DOC.replace("### Fixed\n- A crash.\n\n", "", 1), "3.1.0-beta2", "2026-12-08")   # an empty one

    def test_a_released_beta_is_not_dated_again(self):
        with self.assertRaises(changelog.ChangelogError):
            changelog.beta(DOC, "3.1.0-beta1", "2026-12-08")

    def test_only_a_beta_version(self):
        with self.assertRaises(changelog.ChangelogError):
            changelog.beta(DOC, "3.1.0", "2026-12-08")


class StableTest(unittest.TestCase):
    def test_the_betas_become_the_version(self):
        doc = changelog.beta(DOC, "3.1.0-beta2", "2026-12-08")
        out = changelog.stable(doc, "3.1.0", "2026-12-10")
        # oldest first: the first beta describes the version, the later ones what changed on the way
        self.assertIn("Intro.\n\n## 3.1.0 - 2026-12-10\n\nSummary of 3.1.0.\n\n### Menus\n- A new row.\n\n### Fixed\n- A crash.\n\n"
                      "## 3.0.0 - 2026-11-20", out)
        self.assertNotIn("beta", out)

    def test_needs_a_beta_first(self):
        with self.assertRaises(changelog.ChangelogError):
            changelog.stable(DOC, "3.2.0", "2026-12-10")

    def test_a_beta_in_the_works_stops_it(self):
        with self.assertRaises(changelog.ChangelogError):
            changelog.stable(DOC, "3.1.0", "2026-12-10")      # beta2 has entries and is not out


class CheckTest(unittest.TestCase):
    def test_beta_tag_needs_its_dated_section(self):
        self.assertEqual(changelog.check(DOC, "v3.1.0-beta1"), "")
        self.assertIn("3.1.0-beta2", changelog.check(DOC, "v3.1.0-beta2"))    # still in the works: no date
        self.assertEqual(changelog.check(changelog.beta(DOC, "3.1.0-beta2", "2026-12-08"), "v3.1.0-beta2"), "")

    def test_stable_tag_needs_a_dated_section(self):
        self.assertEqual(changelog.check(DOC, "v3.0.0"), "")
        self.assertIn("date", changelog.check(DOC, "v3.1.0"))

    def test_other_tags_are_refused(self):
        self.assertIn("tag", changelog.check(DOC, "v3.0"))


if __name__ == "__main__":
    unittest.main()
