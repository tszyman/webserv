import unittest
import os
import subprocess
import time

class TestInfrastructure(unittest.TestCase):

    def test_01_binary_exists(self):
        """Verify that the main webserv binary exists and is executable"""
        binary_path = "../webserv"
        self.assertTrue(os.path.exists(binary_path), f"'{binary_path}' not found!")
        self.assertTrue(os.access(binary_path, os.X_OK), f"'{binary_path}' is not executable!")

    def test_02_basic_execution_no_crash(self):
        """Verify that running webserv stays alive without immediate crash"""
        # Start the server as a background process to avoid blocking the script
        process = subprocess.Popen(
            ["../webserv", "../config/default.conf"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL
        )
        
        # Give it a brief moment to run and hit any potential setup crashes
        time.sleep(0.2)
        
        # Check if the process terminated prematurely (poll() returns exit code if terminated)
        exit_code = process.poll()
        
        self.assertIsNone(exit_code, f"Server exited immediately on startup with code {exit_code}")
        
        # Clean up the background process safely
        process.terminate()
        process.wait()
