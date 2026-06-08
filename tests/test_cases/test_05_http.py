import unittest
import subprocess

class TestHttpModule(unittest.TestCase):

    def run_tester(self, component, *args):
        """Helper to invoke the compiled C++ engine for HTTP components"""
        cmd = ["./unit_tester", component] + list(args)

        # Usuwamy text=True oraz newline=''
        res = subprocess.run(
            cmd, 
            stdout=subprocess.PIPE, 
            stderr=subprocess.PIPE
        )
        # Ręcznie dekodujemy surowe bajty do stringa. 
        # Dzięki temu \r\n przetrwa nienaruszone dokładnie tak, jak wypluł to kod w C++.
        return res.stdout.decode('utf-8')

    # ==========================================
    # HttpStatus TESTS
    # ==========================================

    def test_status_200_ok(self):
        """Verify HttpStatus correctly identifies 200 OK"""
        out = self.run_tester("http_status", "200")
        self.assertIn("Phrase: OK", out)
        self.assertIn("Known: Yes", out)
        self.assertIn("Error: No", out)

    def test_status_404_not_found(self):
        """Verify HttpStatus correctly identifies 404 Not Found"""
        out = self.run_tester("http_status", "404")
        self.assertIn("Phrase: Not Found", out)
        self.assertIn("Known: Yes", out)
        self.assertIn("Error: Yes", out)

    def test_status_unknown_code(self):
        """Verify HttpStatus defaults safely on unknown/invalid status codes"""
        out = self.run_tester("http_status", "999")
        self.assertIn("Phrase: Internal Server Error", out)
        self.assertIn("Known: No", out)
        self.assertIn("Error: No", out)

    # ==========================================
    # HttpResponse TESTS
    # ==========================================

    def test_http_response_formatting(self):
        """Verify HttpResponse properly assembles the raw HTTP/1.1 payload string"""
        out = self.run_tester("http_response")
        
        # TERAZ JESTEŚMY BEZPIECZNI: Asercje rygorystycznie sprawdzają \r\n
        self.assertIn("HTTP/1.1 201 Created\r\n", out)
        self.assertIn("Content-Type: text/plain\r\n", out)
        self.assertIn("Content-Length: 25\r\n", out)
        self.assertIn("Connection: close\r\n", out)
        
        # Znakomity test sprawdzający, czy nagłówki i ciało są oddzielone dwoma CRLF
        self.assertIn("\r\n\r\nCreated user successfully", out)

    # ==========================================
    # HttpErrorPage TESTS
    # ==========================================

    def test_error_page_generation(self):
        """Verify ErrorPage generates valid HTML documents for specific errors"""
        out = self.run_tester("http_error", "403")
        self.assertIn("HTTP/1.1 403 Forbidden\r\n", out)
        self.assertIn("Content-Type: text/html\r\n", out)
        self.assertIn("<!DOCTYPE html>", out)
        self.assertIn("<title>403 Forbidden</title>", out)

    def test_error_page_fallback(self):
        """Verify ErrorPage normalizes unknown error codes to 500 Internal Server Error"""
        out = self.run_tester("http_error", "499")
        self.assertIn("HTTP/1.1 500 Internal Server Error\r\n", out)
        self.assertIn("<title>500 Internal Server Error</title>", out)