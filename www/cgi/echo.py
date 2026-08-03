import sys

body = sys.stdin.read()
sys.stdout.write("Content-Type: text/plain\r\n\r\n")
sys.stdout.write("webserv CGI demo\n")
sys.stdout.write("received bytes: %d\n" % len(body))
