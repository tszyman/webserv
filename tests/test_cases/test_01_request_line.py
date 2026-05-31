import unittest
import subprocess

class TestRequestLine(unittest.TestCase):

    def run_tester(self, full_payload):
        """Helper to execute unit_tester for internal class validation"""
        res = subprocess.run(
            ["./unit_tester", full_payload],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        return res.stdout.strip().split("\n")

    def test_valid_get(self):
        """Parse valid GET request line internally and finalize headers"""
        # Appending \r\n\r\n simulates the end of the HTTP Header section
        payload = "GET /index.html HTTP/1.1\r\n\r\n"
        out = self.run_tester(payload)
        
        self.assertEqual(out[0], "SUCCESS")
        self.assertIn("Method: GET", out)
        self.assertIn("Path: /index.html", out)
        self.assertIn("Version: HTTP/1.1", out)

    def test_invalid_method(self):
        """Reject unsupported methods internally"""
        payload = "INVALID /index.html HTTP/1.1\r\n\r\n"
        out = self.run_tester(payload)
        self.assertEqual(out[0], "FAILED")