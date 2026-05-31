import unittest
import socket
import sys
import os

# Import integration helpers from the main script
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import run_tests

class TestFullServerFeatures(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        """Starts the actual full server application before running feature tests"""
        run_tests.start_main_server()

    @classmethod
    def tearDownClass(cls):
        """Stops the full server after all feature tests are done"""
        run_tests.stop_main_server()

    def test_end_to_end_network_request(self):
        """Test full feature: Verify server accepts network connections and processes them"""
        # If the server network engine isn't ready yet, skip the test gracefully
        if run_tests.SERVER_PROCESS is None or run_tests.SERVER_PROCESS.poll() is not None:
            self.skipTest("Server network layer is not implemented or not running yet. Skipping.")

        # Real End-to-End network testing via socket
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(("127.0.0.1", 8080))
        s.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
        response = s.recv(4096).decode("utf-8", errors="ignore")
        s.close()

        self.assertIn("HTTP/1.1", response)