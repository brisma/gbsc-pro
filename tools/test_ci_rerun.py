import unittest

import ci_rerun

NO_RUNNER = "The job was not acquired by Runner of type hosted even after multiple attempts"


def job(name, conclusion, steps=0, notes=()):
    return {"name": name, "conclusion": conclusion, "steps": steps, "annotations": list(notes)}


class DecideTest(unittest.TestCase):
    """A job GitHub cancelled because no runner came is not a failure of the code: the run is tried again."""

    def test_a_job_without_a_runner_is_run_again(self):
        jobs = [job("build (macos)", "cancelled", 0, [NO_RUNNER]), job("build (windows)", "success", 20),
                job("publish", "skipped")]
        self.assertEqual(ci_rerun.decide(1, jobs), (True, "no runner for: build (macos)"))

    def test_a_real_failure_is_left_alone(self):
        jobs = [job("build (macos)", "cancelled", 0, [NO_RUNNER]), job("tests", "failure", 9)]
        self.assertEqual(ci_rerun.decide(1, jobs), (False, "tests: failure"))

    def test_a_job_somebody_cancelled_is_left_alone(self):
        self.assertEqual(ci_rerun.decide(1, [job("build (macos)", "cancelled", 0)]), (False, "build (macos): cancelled"))
        started = [job("build (macos)", "cancelled", 4, [NO_RUNNER])]      # it ran: not this case
        self.assertEqual(ci_rerun.decide(1, started), (False, "build (macos): cancelled"))

    def test_a_run_without_trouble_is_left_alone(self):
        self.assertEqual(ci_rerun.decide(1, [job("tests", "success", 9)]), (False, "no job is missing a runner"))

    def test_it_gives_up_after_the_last_attempt(self):
        jobs = [job("build (macos)", "cancelled", 0, [NO_RUNNER])]
        self.assertTrue(ci_rerun.decide(ci_rerun.MAX_RERUNS, jobs)[0])
        self.assertEqual(ci_rerun.decide(ci_rerun.MAX_RERUNS + 1, jobs),
                         (False, "still no runner after %d attempts" % (ci_rerun.MAX_RERUNS + 1)))


if __name__ == "__main__":
    unittest.main()
