import unittest
import subprocess
import os

class TestConfigParser(unittest.TestCase):

    def setUp(self):
        """Dynamically create a mock configuration file with all new directives"""
        self.test_conf = "temp_full_test.conf"
        with open(self.test_conf, "w") as f:
            f.write("""
            # Full feature server config
            server {
                listen 9090;
                server_name test.local;
                client_max_body_size 2048;

                location /api {
                    root /var/www/api;
                    client_max_body_size 1024;
                    allowed_methods GET POST;
                    autoindex on;
                    upload_enable on;
                    upload_store /var/www/uploads;
                }
            }
            """)

    def tearDown(self):
        """Clean up the test config file after the test completes"""
        if os.path.exists(self.test_conf):
            os.remove(self.test_conf)

    def run_tester(self, config_path):
        """Helper to invoke the compiled C++ engine for config parsing"""
        res = subprocess.run(
            ["./unit_tester", "config_parser", config_path],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        return res.stdout.strip().split("\n")

    def test_full_config_directives(self):
        """Verify Config parser correctly extracts body sizes, autoindex, and upload flags"""
        out = self.run_tester(self.test_conf)
        
        self.assertIn("SUCCESS_CONFIG_PARSER", out)
        
        # Check Server block rules
        self.assertIn("Server Port: 9090", out)
        self.assertIn("Server Name: test.local", out)
        self.assertIn("Max Body Size: 2048", out)
        
        # Check Location block rules
        self.assertIn("Loc Path: /api", out)
        self.assertIn("Loc Root: /var/www/api", out)
        self.assertIn("Loc Max Body Size: 1024", out)
        self.assertIn("Autoindex: on", out)
        self.assertIn("Upload: on", out)
        self.assertIn("Upload Store: /var/www/uploads", out)
        
        # Check method parsing
        self.assertIn("Method GET: yes", out)
        self.assertIn("Method POST: yes", out)

    def test_invalid_syntax_recovery(self):
        """Verify Config parser catches syntax errors safely without crashing"""
        # Create a bad config missing a semicolon
        bad_conf = "bad.conf"
        with open(bad_conf, "w") as f:
            f.write("server { listen 8080  server_name crash; }")
            
        out = self.run_tester(bad_conf)
        os.remove(bad_conf)
        
        # Parser should have caught the exception and returned false (FAILED_CONFIG_PARSER)
        self.assertEqual(out[0], "FAILED_CONFIG_PARSER")