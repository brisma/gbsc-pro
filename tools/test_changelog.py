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

## Unreleased

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

    def test_unknown_version(self):
        self.assertIsNone(changelog.section(DOC, "9.9.9"))


class BetaTest(unittest.TestCase):
    def test_unreleased_becomes_the_beta_section(self):
        out = changelog.beta(DOC, "3.1.0")
        self.assertIn("## Unreleased\n\n## 3.1.0 - beta\n\n### Menus\n- A new row.\n\n## 3.0.0 - 2026-11-20", out)
        self.assertEqual(changelog.section(out, "3.1.0"), "### Menus\n- A new row.")

    def test_a_later_beta_takes_the_new_entries(self):
        doc = changelog.beta(DOC, "3.1.0").replace("## Unreleased\n", "## Unreleased\n\n### Fixed\n- A crash.\n", 1)
        out = changelog.beta(doc, "3.1.0")
        self.assertEqual(changelog.section(out, "3.1.0"), "### Menus\n- A new row.\n\n### Fixed\n- A crash.")
        self.assertIn("## Unreleased\n\n## 3.1.0 - beta", out)

    def test_nothing_to_release(self):
        with self.assertRaises(changelog.ChangelogError):
            changelog.beta(changelog.beta(DOC, "3.1.0"), "3.2.0")   # Unreleased is empty

    def test_a_released_version_takes_no_beta(self):
        with self.assertRaises(changelog.ChangelogError):
            changelog.beta(DOC, "3.0.0")


class StableTest(unittest.TestCase):
    def test_the_beta_section_gets_the_date(self):
        out = changelog.stable(changelog.beta(DOC, "3.1.0"), "3.1.0", "2026-12-10")
        self.assertIn("## 3.1.0 - 2026-12-10\n\n### Menus", out)
        self.assertNotIn("3.1.0 - beta", out)

    def test_needs_a_beta_first(self):
        with self.assertRaises(changelog.ChangelogError):
            changelog.stable(DOC, "3.1.0", "2026-12-10")


class CheckTest(unittest.TestCase):
    def test_beta_tag_needs_its_beta_section(self):
        doc = changelog.beta(DOC, "3.1.0")
        self.assertEqual(changelog.check(doc, "v3.1.0-beta2"), "")
        self.assertIn("3.1.0", changelog.check(DOC, "v3.1.0-beta1"))

    def test_stable_tag_needs_a_dated_section(self):
        self.assertEqual(changelog.check(DOC, "v3.0.0"), "")
        self.assertIn("date", changelog.check(changelog.beta(DOC, "3.1.0"), "v3.1.0"))

    def test_other_tags_are_refused(self):
        self.assertIn("tag", changelog.check(DOC, "v3.0"))


if __name__ == "__main__":
    unittest.main()
