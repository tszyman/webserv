import unittest
import subprocess

class TestServerCore(unittest.TestCase):

    def run_tester(self, component, *args, **kwargs):
        """Helper to invoke the compiled C++ engine and capture both stdout and stderr"""
        cwd = kwargs.get("cwd", ".")
        binary = "./unit_tester" if cwd == "." else "./tests/unit_tester"
        cmd = [binary, component] + list(args)
        res = subprocess.run(
            cmd, cwd=cwd,
            stdout=subprocess.PIPE, 
            stderr=subprocess.PIPE
        )
        return res.stdout.decode('utf-8'), res.stderr.decode('utf-8')

    def test_server_config_loader(self):
        """Verify that Server::loadConfig parses rules and returns true successfully"""
        # Server::loadConfig() uses config/default.conf relative to the project
        # root, not the tests/ directory used by the main runner.
        out, err = self.run_tester("server", "config", cwd="..")
        
        self.assertIn("SUCCESS_CONFIG", out)
        # Verify the logs from Server.cpp
        self.assertIn("Loading config from: config/default.conf", out)
        self.assertIn("Config loaded successfully.", out)

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
