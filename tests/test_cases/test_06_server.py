import unittest
import subprocess

class TestServerCore(unittest.TestCase):

    def run_tester(self, component, *args):
        """Helper to invoke the compiled C++ engine and capture both stdout and stderr"""
        cmd = ["./unit_tester", component] + list(args)
        res = subprocess.run(
            cmd, 
            stdout=subprocess.PIPE, 
            stderr=subprocess.PIPE
        )
        return res.stdout.decode('utf-8'), res.stderr.decode('utf-8')

    def test_server_config_loader(self):
        """Verify that Server::loadConfig parses rules and returns true successfully"""
        out, err = self.run_tester("server", "config")
        
        self.assertIn("SUCCESS_CONFIG", out)
        # Verify the logs from Server.cpp
        self.assertIn("Loading config...", out)
        self.assertIn("Config loaded.", out)

    def test_server_connection_management(self):
        """Verify Server properly maps descriptors, cleans up instances, and prevents memory leaks"""
        out, err = self.run_tester("server", "connections")
        
        # 1. Verify the C++ tester didn't crash
        self.assertIn("SUCCESS_CONNECTIONS", out)
        self.assertIn("SERVER_DESTROYED", out)
        
        # 2. Verify Server.cpp logs
        self.assertIn("[Server] Mapping created for FD: 10", out)
        self.assertIn("[Server] Mapping created for FD: 20", out)
        self.assertIn("[Server] Cleaning up and removing connection for FD: 10", out)
        
        # 3. Verify Connection.cpp logs (Now captured in 'out' due to Logger::info)
        self.assertIn("New connection created on FD: 10", out)
        self.assertIn("New connection created on FD: 20", out)
        
        # Ensure manually cleaned up connection triggered the destructor
        self.assertIn("Closing connection on FD: 10", out)
        
        # MOST IMPORTANT: Ensure Server destructor cleaned up the orphan connection
        self.assertIn("Closing connection on FD: 20", out, 
            "Error: Server did not clean up remaining connections in its destructor (Memory Leak!)")