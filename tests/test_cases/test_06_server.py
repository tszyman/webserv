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
        
        # 1. Sprawdzamy czy tester C++ nie wywrócił się z błędem w połowie testu
        self.assertIn("SUCCESS_CONNECTIONS", out)
        self.assertIn("SERVER_DESTROYED", out)
        
        # 2. Weryfikujemy logi na standardowym wyjściu (std::cout) zarządzane przez Server.cpp
        # Serwer ma zarejestrować 2 połączenia i ubić 1.
        self.assertIn("[Server] Mapping created for FD: 10", out)
        self.assertIn("[Server] Mapping created for FD: 20", out)
        self.assertIn("[Server] Cleaning up and removing connection for FD: 10", out)
        
        # 3. Weryfikujemy logi na standardowym wyjściu błędu (std::cerr) zarządzane przez Connection.cpp
        self.assertIn("[Connection] New connection created on FD: 10", err)
        self.assertIn("[Connection] New connection created on FD: 20", err)
        
        # Upewniamy się, że ręcznie zwolnione połącznie wywołało destruktor Connection
        self.assertIn("[Connection] Closing connection on FD: 10", err)
        
        # NAJWAŻNIEJSZE: Upewniamy się, że destruktor klasy Server (~Server) posprzątał sierotę (c2 - FD: 20)
        self.assertIn("[Connection] Closing connection on FD: 20", err, 
            "Błąd: Serwer nie wyczyścił zaległych połączeń w swoim destruktorze (Memory Leak!)")