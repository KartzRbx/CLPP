"""A sanitizer's exit code 1 must never look like a valid compiler rejection."""
import subprocess
import unittest
from core_validation import acceptable_result


class AdversarialResultTests(unittest.TestCase):
    def result(self, code=1, stdout='', stderr=''):
        return subprocess.CompletedProcess(['clpp'], code, stdout, stderr)

    def test_sanitizer_exit_one_is_failure_for_each_mode(self):
        for diagnostic in ['ERROR: AddressSanitizer: heap-use-after-free',
                           'SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior',
                           'ERROR: LeakSanitizer: detected memory leaks',
                           'C:/src/solver.cpp:42:7: runtime error: signed integer overflow']:
            for expected, check_only in [(None, True), (None, False), ('ok\n', False)]:
                with self.subTest(diagnostic=diagnostic, check_only=check_only, expected=expected):
                    self.assertFalse(acceptable_result(self.result(stdout='ok\n', stderr=diagnostic), expected, check_only))

    def test_ordinary_diagnostic_is_successful_rejection(self):
        result = self.result(stderr='input.clp:1:3: error: undefined name')
        self.assertTrue(acceptable_result(result, None, True))
        self.assertTrue(acceptable_result(result, None))

    def test_crash_and_silent_exit_are_failures(self):
        for result in [self.result(code=-11), self.result(code=3221225477), self.result()]:
            self.assertFalse(acceptable_result(result, None, True))

    def test_runtime_output_is_verified(self):
        self.assertTrue(acceptable_result(self.result(code=0, stdout='ok\n'), 'ok\n'))
        self.assertFalse(acceptable_result(self.result(code=0, stdout='wrong\n'), 'ok\n'))


if __name__ == '__main__':
    unittest.main(verbosity=2)
