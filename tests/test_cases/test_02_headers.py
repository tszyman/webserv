import unittest
import subprocess

class TestHeaders(unittest.TestCase):

    def run_tester(self, full_payload):
        """Helper to pass the entire raw HTTP payload into the unit_tester"""
        res = subprocess.run(
            ["./unit_tester", "parser", full_payload],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        return res.stdout.strip().split("\n")

    def test_valid_headers(self):
        """Parse well-formed headers into map and verify successful completion"""
        payload = (
            "GET / HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 0\r\n"
            "\r\n"
        )
        out = self.run_tester(payload)
        self.assertEqual(out[0], "SUCCESS")
        # Your unit_tester prefixes printed headers with "H-"
        self.assertIn("H-Host: localhost", out)
        self.assertIn("H-Content-Type: text/html", out)
        self.assertIn("H-Content-Length: 0", out)

    def test_invalid_header_syntax(self):
        """Reject headers without a colon separator"""
        payload = (
            "GET / HTTP/1.1\r\n"
            "Host localhost\r\n"
            "\r\n"
        )
        out = self.run_tester(payload)
        self.assertEqual(out[0], "FAILED")

    def test_spaces_in_key(self):
        """Reject spaces inside the header key before the colon"""
        payload = (
            "GET / HTTP/1.1\r\n"
            "Bad Key: value\r\n"
            "\r\n"
        )
        out = self.run_tester(payload)
        self.assertEqual(out[0], "FAILED")