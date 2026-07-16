import unittest
import subprocess

class TestNetworkUnits(unittest.TestCase):

    def run_socket_tester(self, port):
        """Helper to invoke the compiled C++ engine bridge utility for sockets"""
        res = subprocess.run(
            ["./unit_tester", "socket", str(port)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        return res.stdout.strip()

    def test_socket_engine_initialization(self):
        """Verify that SocketEngine successfully instantiates with a port and runs init()"""
        
        # Test initialization on a standard test port
        output = self.run_socket_tester(8080)
        
        self.assertIn("SUCCESS_SOCKET_INIT", output, 
            f"SocketEngine failed to initialize. Output was: {output}")

    def test_socket_engine_privilege_failure(self):
        """Verify that SocketEngine gracefully fails when binding to restricted ports (< 1024)"""
        # Testing port 80 without sudo privileges should fail and return FAILED_SOCKET_INIT
        output = self.run_socket_tester(80)
        
        self.assertEqual(output, "FAILED_SOCKET_INIT", 
            "SocketEngine unexpectedly succeeded binding to a restricted port, or failed to catch the error.")