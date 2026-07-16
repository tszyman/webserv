import unittest
import subprocess
import os

class TestAutoindex(unittest.TestCase):

    def setUp(self):
        """Set up a temporary directory with a dummy file for the test"""
        self.test_dir = "temp_autoindex_dir"
        os.makedirs(self.test_dir, exist_ok=True)
        with open(os.path.join(self.test_dir, "test_file.txt"), "w") as f:
            f.write("hello")

    def tearDown(self):
        """Clean up the temporary directory after the test"""
        if os.path.exists(os.path.join(self.test_dir, "test_file.txt")):
            os.remove(os.path.join(self.test_dir, "test_file.txt"))
        if os.path.exists(self.test_dir):
            os.rmdir(self.test_dir)

    def run_tester(self, component, *args):
        """Helper to invoke the compiled C++ engine"""
        cmd = ["./unit_tester", component] + list(args)
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return res.stdout.decode('utf-8')

    def test_autoindex_generation(self):
        """Verify Autoindex generates a valid HTML directory listing"""
        out = self.run_tester("autoindex", self.test_dir, "/test_uri")
        
        self.assertIn("SUCCESS_AUTOINDEX", out)
        self.assertIn("HTTP/1.1 200 OK", out)
        self.assertIn("Index of /test_uri/", out)
        # Ensures the file is present in the HTML body
        self.assertIn("test_file.txt", out)