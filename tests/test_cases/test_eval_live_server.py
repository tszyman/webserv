"""Black-box tests for the real webserv executable.

The fixture deliberately owns its files, configuration and port.  This keeps
the tests reproducible and prevents a developer's local www/ directory or a
previous webserv process from changing their result.
"""

import os
import shutil
import socket
import sys
import tempfile
import time
import unittest

sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import run_tests


class TestEvaluationLiveServer(unittest.TestCase):
    PORT = None
    server = None
    work_dir = None

    @classmethod
    def setUpClass(cls):
        cls.PORT = cls.find_available_port()
        cls.work_dir = tempfile.mkdtemp(prefix="webserv-live-", dir="/tmp")
        cls.root = os.path.join(cls.work_dir, "site")
        cls.vhost_root = os.path.join(cls.work_dir, "vhost")
        os.makedirs(os.path.join(cls.root, "listing"))
        os.makedirs(cls.vhost_root)
        with open(os.path.join(cls.root, "index.html"), "w") as page:
            page.write("local test index\n")
        with open(os.path.join(cls.root, "listing", "visible.txt"), "w") as page:
            page.write("listed\n")
        with open(os.path.join(cls.root, "delete-me.txt"), "w") as page:
            page.write("remove me\n")
        with open(os.path.join(cls.vhost_root, "index.html"), "w") as page:
            page.write("virtual host index\n")

        cls.config_path = os.path.join(cls.work_dir, "live.conf")
        with open(cls.config_path, "w") as config:
            config.write("""server {
    listen %d;
    server_name localhost;
    client_max_body_size 64;
    location / {
        root %s;
        allowed_methods GET POST;
    }
    location /listing {
        root %s/listing;
        allowed_methods GET;
        autoindex on;
    }
    location /post {
        root %s;
        allowed_methods POST;
    }
}
server {
    listen %d;
    server_name custom-server.local;
    location / {
        root %s;
        allowed_methods GET;
    }
}
""" % (cls.PORT, cls.root, cls.root, cls.root, cls.PORT, cls.vhost_root))

        cls.server = run_tests.start_main_server(cls.config_path)
        cls.wait_until_listening()

    @classmethod
    def tearDownClass(cls):
        run_tests.stop_main_server()
        if cls.work_dir:
            shutil.rmtree(cls.work_dir)

    @classmethod
    def find_available_port(cls):
        probe = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            probe.bind(("127.0.0.1", 0))
            return probe.getsockname()[1]
        finally:
            probe.close()

    @classmethod
    def wait_until_listening(cls):
        deadline = time.time() + 3
        while time.time() < deadline:
            if cls.server.poll() is not None:
                output = cls.server.stdout.read() if cls.server.stdout else ""
                raise RuntimeError("webserv exited during startup:\n%s" % output)
            try:
                probe = socket.create_connection(("127.0.0.1", cls.PORT), timeout=0.1)
                probe.close()
                return
            except socket.error:
                time.sleep(0.05)
        raise RuntimeError("webserv did not listen on port %d" % cls.PORT)

    def send_raw_request(self, payload):
        """Return the entire response; a single recv() is not reliable TCP I/O."""
        client = socket.create_connection(("127.0.0.1", self.PORT), timeout=2)
        client.settimeout(2)
        if "\r\nConnection:" not in payload:
            payload = payload.replace("\r\n\r\n", "\r\nConnection: close\r\n\r\n", 1)
        client.sendall(payload.encode("ascii"))
        chunks = []
        try:
            while True:
                try:
                    chunk = client.recv(4096)
                except socket.timeout:
                    return "CONNECTION_TIMEOUT: server did not send a response within 2 seconds"
                if not chunk:
                    break
                chunks.append(chunk)
        finally:
            client.close()
        return b"".join(chunks).decode("utf-8", errors="replace")

    def test_valid_get(self):
        response = self.send_raw_request("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
        self.assertTrue(response.startswith("HTTP/1.1 200"), response)
        self.assertIn("local test index", response)

    def test_unknown_method_is_rejected(self):
        response = self.send_raw_request("TROLL / HTTP/1.1\r\nHost: localhost\r\n\r\n")
        self.assertTrue(response.startswith("HTTP/1.1 400"), response)

    def test_post_too_large(self):
        body = "A" * 65
        response = self.send_raw_request(
            "POST /post HTTP/1.1\r\nHost: localhost\r\nContent-Length: 65\r\n\r\n" + body)
        self.assertTrue(response.startswith("HTTP/1.1 413"), response)

    def test_chunked_post(self):
        response = self.send_raw_request(
            "POST /post HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n"
            "5\r\nHello\r\n0\r\n\r\n")
        self.assertTrue(response.startswith("HTTP/1.1 200"), response)

    def test_not_found(self):
        response = self.send_raw_request("GET /missing HTTP/1.1\r\nHost: localhost\r\n\r\n")
        self.assertTrue(response.startswith("HTTP/1.1 404"), response)

    def test_virtual_host(self):
        response = self.send_raw_request("GET / HTTP/1.1\r\nHost: custom-server.local\r\n\r\n")
        self.assertTrue(response.startswith("HTTP/1.1 200"), response)
        self.assertIn("virtual host index", response)

    def test_autoindex(self):
        response = self.send_raw_request("GET /listing/ HTTP/1.1\r\nHost: localhost\r\n\r\n")
        self.assertTrue(response.startswith("HTTP/1.1 200"), response)
        self.assertIn("visible.txt", response)

    def test_delete_is_forbidden_by_location_policy(self):
        response = self.send_raw_request("DELETE /delete-me.txt HTTP/1.1\r\nHost: localhost\r\n\r\n")
        self.assertTrue(response.startswith("HTTP/1.1 405"), response)
