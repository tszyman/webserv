import unittest
import subprocess
import os
import sys

class TestCgiModule(unittest.TestCase):

    def setUp(self):
        """Dynamically create a valid python CGI script before testing"""
        self.script_path = "dummy.py"
        with open(self.script_path, "w") as f:
            f.write("import sys\n")
            f.write("data = sys.stdin.read()\n")
            f.write("print('Content-Type: text/plain\\r\\n\\r\\n', end='')\n")
            f.write("print(f'CGI Response received: {data}')\n")
        
        # Give it execution permissions
        os.chmod(self.script_path, 0o755)

    def tearDown(self):
        """Clean up the test script after the test completes"""
        if os.path.exists(self.script_path):
            os.remove(self.script_path)

    def run_tester(self, component, *args):
        """Helper to invoke the compiled C++ engine"""
        cmd = ["./unit_tester", component] + list(args)
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return res.stdout.decode('utf-8')

    def test_cgi_execution_and_io(self):
        """Verify CgiHandler can spawn a process, pipe data in, and read data out"""
        
        # sys.executable securely gets the absolute path to the current python3 binary
        python_exec = sys.executable 
        
        out = self.run_tester("cgi", self.script_path, python_exec)
        
        # 1. Assert the initialization didn't fail
        self.assertNotIn("FAILED_CGI_INIT", out)
        self.assertIn("SUCCESS_CGI", out)
        
        # 2. Assert the CGI Headers and Body were correctly passed back through the pipe
        self.assertIn("Content-Type: text/plain\r\n\r\n", out)
        
        # 3. Assert the CGI correctly read "Hello" from sys.stdin
        self.assertIn("CGI Response received: Hello", out)