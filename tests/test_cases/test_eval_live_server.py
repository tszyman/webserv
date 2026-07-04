import unittest
import socket
import sys
import os
import time

# Ensures the import of helper functions from the main run_tests.py script
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import run_tests

class TestEvaluationLiveServer(unittest.TestCase):
    """
    Test-Driven Development (TDD) Suite for 42 Evaluation Criteria.
    Most of these will fail until EventLoop fully integrates RequestParser and Router!
    """

    @classmethod
    def setUpClass(cls):
        """Starts the target server before tests on port 8080"""
        # Kill any hanging processes for safety
        run_tests.stop_main_server()
        run_tests.start_main_server()

    @classmethod
    def tearDownClass(cls):
        """Safely shuts down the server after all tests finish"""
        run_tests.stop_main_server()

    def send_raw_request(self, payload):
        """Sends a raw TCP/IP request to your server and returns the response"""
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            # Prevents the test from hanging indefinitely if the server doesn't send EOF
            s.settimeout(2.0) 
            s.connect(("127.0.0.1", 8080))
            s.sendall(payload.encode("utf-8"))
            response = s.recv(4096).decode("utf-8", errors="ignore")
            s.close()
            return response
        except Exception as e:
            return f"CONNECTION_ERROR: {e}"

    # ==========================================
    # EVAL: BASIC HTTP AND METHOD TESTS
    # ==========================================

    def test_eval_01_valid_get(self):
        """EVAL: Check a basic valid GET request returns HTTP 200"""
        req = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
        res = self.send_raw_request(req)
        self.assertTrue(res.startswith("HTTP/1.1 200"), f"Expected 200 OK, got: {res[:15]}")

    def test_eval_02_missing_host_header(self):
        """EVAL: HTTP/1.1 strictly requires Host header (Should return 400 Bad Request)"""
        req = "GET / HTTP/1.1\r\n\r\n"
        res = self.send_raw_request(req)
        # Current code returns "Hello Webserv". TDD goal: change code to return 400.
        self.assertTrue(res.startswith("HTTP/1.1 400"), f"Expected 400 Bad Request, got: {res[:15]}")

    def test_eval_03_unknown_method(self):
        """EVAL: Request with unknown method (Should return 405 Method Not Allowed or 501)"""
        req = "TROLL / HTTP/1.1\r\nHost: localhost\r\n\r\n"
        res = self.send_raw_request(req)
        # We expect RequestParser to throw STATE_ERROR, and EventLoop to send 405 or 400.
        is_4xx_or_5xx = res.startswith("HTTP/1.1 4") or res.startswith("HTTP/1.1 5")
        self.assertTrue(is_4xx_or_5xx, f"Expected Error Status, got: {res[:15]}")

    # ==========================================
    # EVAL: BODY & CHUNKED 
    # ==========================================

    def test_eval_04_post_too_large(self):
        """EVAL: POST with body larger than client_max_body_size (Should return 413)"""
        # Let's send a huge body that exceeds the limits defined in the config:
        huge_body = "A" * 50000 
        req = f"POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: {len(huge_body)}\r\n\r\n{huge_body}"
        res = self.send_raw_request(req)
        # This will force you to implement body size limits validation in EventLoop/Router.
        self.assertTrue(res.startswith("HTTP/1.1 413"), f"Expected 413 Payload Too Large, got: {res[:15]}")

    def test_eval_05_chunked_request(self):
        """EVAL: Check if server correctly unchunks valid Transfer-Encoding: chunked request"""
        req = (
            "POST / HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Transfer-Encoding: chunked\r\n\r\n"
            "5\r\nHello\r\n0\r\n\r\n"
        )
        res = self.send_raw_request(req)
        self.assertTrue(res.startswith("HTTP/1.1 200"), f"Expected 200 OK after chunking, got: {res[:15]}")

    # ==========================================
    # EVAL: VIRTUAL HOSTS, ROUTING & ERRORS
    # ==========================================

    def test_eval_06_not_found(self):
        """EVAL: Request for non-existent route (Should return 404 Not Found)"""
        req = "GET /i_dont_exist.html HTTP/1.1\r\nHost: localhost\r\n\r\n"
        res = self.send_raw_request(req)
        self.assertTrue(res.startswith("HTTP/1.1 404"), f"Expected 404 Not Found, got: {res[:15]}")

    def test_eval_07_virtual_hosts(self):
        """EVAL: Setup multiple servers with different hostnames"""
        # Sending a request with a specific Host header should route to a different server block
        req = "GET / HTTP/1.1\r\nHost: custom-server.local\r\n\r\n"
        res = self.send_raw_request(req)
        self.assertTrue(res.startswith("HTTP/1.1 200"), f"Virtual host routing failed: {res[:15]}")

    def test_eval_08_redirection(self):
        """EVAL: Try a redirected URL"""
        req = "GET /old-page HTTP/1.1\r\nHost: localhost\r\n\r\n"
        res = self.send_raw_request(req)
        is_redirect = res.startswith("HTTP/1.1 301") or res.startswith("HTTP/1.1 302")
        self.assertTrue(is_redirect, f"Expected 301/302 Redirection, got: {res[:15]}")
        self.assertIn("Location:", res, "Redirection response missing 'Location' header")

    def test_eval_09_autoindex(self):
        """EVAL: Try to list a directory"""
        req = "GET /images/ HTTP/1.1\r\nHost: localhost\r\n\r\n"
        res = self.send_raw_request(req)
        self.assertTrue(res.startswith("HTTP/1.1 200"), f"Expected 200 OK, got: {res[:15]}")
        self.assertIn("<title>Index of", res, "Response does not contain Autoindex HTML tags")

    # ==========================================
    # EVAL: UPLOADS & DELETE
    # ==========================================

    def test_eval_10_upload_file(self):
        """EVAL: Upload some file to the server and get it back"""
        boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW"
        body = (
            f"--{boundary}\r\n"
            "Content-Disposition: form-data; name=\"file\"; filename=\"test_upload.txt\"\r\n"
            "Content-Type: text/plain\r\n\r\n"
            "This is a test file content.\r\n"
            f"--{boundary}--\r\n"
        )
        req = (
            "POST /upload HTTP/1.1\r\n"
            "Host: localhost\r\n"
            f"Content-Type: multipart/form-data; boundary={boundary}\r\n"
            f"Content-Length: {len(body)}\r\n\r\n"
            f"{body}"
        )
        res = self.send_raw_request(req)
        # Server should return 201 Created or 200 OK upon successful upload
        is_success = res.startswith("HTTP/1.1 201") or res.startswith("HTTP/1.1 200")
        self.assertTrue(is_success, f"Expected successful upload status, got: {res[:15]}")

    def test_eval_11_delete_file(self):
        """EVAL: DELETE requests -> should work"""
        # Attempt to delete the file we just uploaded
        req = "DELETE /upload/test_upload.txt HTTP/1.1\r\nHost: localhost\r\n\r\n"
        res = self.send_raw_request(req)
        
        is_deleted = res.startswith("HTTP/1.1 202") or res.startswith("HTTP/1.1 204") or res.startswith("HTTP/1.1 200")
        self.assertTrue(is_deleted, f"Expected 200/202/204 on DELETE, got: {res[:15]}")

    def test_eval_12_delete_forbidden(self):
        """EVAL: Try to delete something without permission"""
        # Attempt to delete a file in a location where DELETE is not in allowed_methods
        req = "DELETE /images/cat.png HTTP/1.1\r\nHost: localhost\r\n\r\n"
        res = self.send_raw_request(req)
        
        self.assertTrue(res.startswith("HTTP/1.1 405"), f"Expected 405 Method Not Allowed, got: {res[:15]}")