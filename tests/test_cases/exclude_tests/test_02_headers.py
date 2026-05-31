import unittest
import subprocess

class TestHeaders(unittest.TestCase):

    def run_tester(self, headers_list):
        """Helper to pass multiple headers to unit_tester"""
        cmd = ["./unit_tester", "header_line"] + headers_list
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
		if not request_line.endswith("\r\n"):
        	request_line += "\r\n"
        return res.stdout.strip().split("\n")

    def test_valid_headers(self):
        """Parse well-formed headers into map"""
        headers = ["Host: localhost", "Content-Type: text/html", "Content-Length: 42"]
        out = self.run_tester(headers)
        self.assertEqual(out[0], "SUCCESS")
        self.assertIn("Host: localhost", out)
        self.assertIn("Content-Type: text/html", out)
        self.assertIn("Content-Length: 42", out)

    def test_invalid_header_syntax(self):
        """Reject headers without a colon separator"""
        headers = ["Host localhost"]
        out = self.run_tester(headers)
        self.assertEqual(out[0], "FAILED")

    def test_spaces_in_key(self):
        """Reject spaces inside the header key before the colon"""
        headers = ["Bad Key: value"]
        out = self.run_tester(headers)
        self.assertEqual(out[0], "FAILED")