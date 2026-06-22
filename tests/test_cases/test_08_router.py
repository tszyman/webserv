import unittest
import subprocess

class TestRouter(unittest.TestCase):

    def run_tester(self, raw_http_payload):
        """Helper to invoke the compiled C++ engine for the Router component"""
        res = subprocess.run(
            ["./unit_tester", "router", raw_http_payload],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        return res.stdout.strip().split("\n")

    def test_router_404_not_found(self):
        """Verify the Router returns 404 when no Location block matches the URI"""
        payload = "GET /unknown/page.html HTTP/1.1\r\nHost: localhost\r\n\r\n"
        out = self.run_tester(payload)
        
        self.assertIn("SUCCESS_ROUTER", out)
        self.assertIn("STATUS: 404", out)

    def test_router_405_method_not_allowed(self):
        """Verify the Router returns 405 when the method is explicitly forbidden in config"""
        # Our C++ mock config only allows GET and DELETE on /images
        payload = "POST /images/cat.png HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n"
        out = self.run_tester(payload)
        
        self.assertIn("SUCCESS_ROUTER", out)
        self.assertIn("STATUS: 405", out)

    def test_router_valid_get(self):
        """Verify the Router successfully dispatches a valid GET request (Happy Path)"""
        # Should route to handleGet (which currently sets status 200 in our stub)
        payload = "GET /images/cat.png HTTP/1.1\r\nHost: localhost\r\n\r\n"
        out = self.run_tester(payload)
        
        self.assertIn("SUCCESS_ROUTER", out)
        self.assertIn("STATUS: 200", out)

    def test_router_valid_delete_404_file(self):
        """Verify the Router dispatches DELETE, but returns 404 if physical file is missing"""
        # This will hit handleDelete. Since '/var/www/data/delete_me.txt' doesn't 
        # actually exist on your hard drive, stat() will fail and return 404.
        payload = "DELETE /images/delete_me.txt HTTP/1.1\r\nHost: localhost\r\n\r\n"
        out = self.run_tester(payload)
        
        self.assertIn("SUCCESS_ROUTER", out)
        self.assertIn("STATUS: 404", out)