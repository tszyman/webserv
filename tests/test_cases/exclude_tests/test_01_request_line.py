import unittest
import subprocess

class TestRequestLine(unittest.TestCase):

    def run_tester(self, request_line):
        """Helper to execute unit_tester for internal class validation"""
        # Ensure the test string is correctly terminated with true CRLF bytes
        if not request_line.endswith("\r\n"):
            request_line += "\r\n"

        res = subprocess.run(
            ["./unit_tester", request_line],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        return res.stdout.strip().split("\n")

    def test_valid_get(self):
        """Parse valid GET request line internally"""
        out = self.run_tester("GET /index.html HTTP/1.1")
        self.assertEqual(out[0], "SUCCESS")
        self.assertIn("Method: GET", out)
        self.assertIn("Path: /index.html", out)
        self.assertIn("Version: HTTP/1.1", out)

    def test_invalid_method(self):
        """Reject unsupported methods internally"""
        out = self.run_tester("INVALID /index.html HTTP/1.1")
        self.assertEqual(out[0], "FAILED")