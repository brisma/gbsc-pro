#!/usr/bin/env python3
"""A workflow run whose only trouble is a job GitHub cancelled because no runner came (seen with the macOS images:
"The job was not acquired by Runner of type hosted even after multiple attempts") says nothing about the code.
This tells such a run from a real failure, for .github/workflows/rerun-no-runner.yml, which runs it again.

  python3 tools/ci_rerun.py RUN_ID --repo owner/name --attempt N    prints the verdict; with GITHUB_OUTPUT set,
                                                                    writes rerun=true|false there (needs gh)
"""
import argparse
import json
import os
import subprocess
import sys

NO_RUNNER = "was not acquired by Runner"
MAX_RERUNS = 8      # with the wait of the workflow and GitHub's own 15 minutes in the queue: about three hours
PASSED = ("success", "skipped", "neutral")


def never_started(job):
    """Cancelled by GitHub before any step ran, with its own words for it."""
    return (job["conclusion"] == "cancelled" and not job["steps"]
            and any(NO_RUNNER in note for note in job["annotations"]))


def decide(attempt, jobs):
    """(run again?, why) for the jobs of one attempt: {name, conclusion, steps, annotations}."""
    waiting = [j["name"] for j in jobs if never_started(j)]
    for j in jobs:
        if j["conclusion"] not in PASSED and not never_started(j):
            return False, "%s: %s" % (j["name"], j["conclusion"])   # a failure, or a job somebody cancelled
    if not waiting:
        return False, "no job is missing a runner"
    if attempt > MAX_RERUNS:
        return False, "still no runner after %d attempts" % attempt
    return True, "no runner for: " + ", ".join(waiting)


def gh(path):
    out = subprocess.run(["gh", "api", "--paginate", path], check=True, capture_output=True, text=True).stdout
    return json.loads(out)


def jobs_of(repo, run, attempt):
    jobs = []
    for j in gh("repos/%s/actions/runs/%s/attempts/%d/jobs?per_page=100" % (repo, run, attempt))["jobs"]:
        notes = []
        if j["conclusion"] == "cancelled":      # only these are in doubt
            notes = [a.get("message", "") for a in gh("repos/%s/check-runs/%s/annotations" % (repo, j["id"]))]
        jobs.append({"name": j["name"], "conclusion": j["conclusion"] or "", "steps": len(j.get("steps") or []),
                     "annotations": notes})
    return jobs


def main():
    ap = argparse.ArgumentParser(description="Tell a run that only missed a runner from a real failure")
    ap.add_argument("run")
    ap.add_argument("--repo", required=True)
    ap.add_argument("--attempt", type=int, required=True)
    a = ap.parse_args()
    again, why = decide(a.attempt, jobs_of(a.repo, a.run, a.attempt))
    print("%s (attempt %d): %s" % ("run again" if again else "left as it is", a.attempt, why))
    if os.environ.get("GITHUB_OUTPUT"):
        with open(os.environ["GITHUB_OUTPUT"], "a") as f:
            f.write("rerun=%s\n" % ("true" if again else "false"))


if __name__ == "__main__":
    sys.exit(main())
