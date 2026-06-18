#!/usr/bin/env python3
import subprocess
import os
import sys
import time
import unittest

# ANSI Colors
GREEN = "\033[92m"
RED = "\033[91m"
YELLOW = "\033[93m"
RESET = "\033[0m"

# Global variable to track the live server process for full feature testing
SERVER_PROCESS = None

class CustomTestResult(unittest.TextTestResult):
    def addSuccess(self, test):
        super().addSuccess(test)
        print(f"{GREEN}[TEST PASSED]{RESET} {test._testMethodName}: {test._testMethodDoc or ''}")

    def addFailure(self, test, err):
        super().addFailure(test, err)
        print(f"{RED}[TEST FAILED]{RESET} {test._testMethodName}: {test._testMethodDoc or ''}")
        print(f"   -> Reason: {err[1]}")

    def addError(self, test, err):
        super().addError(test, err)
        print(f"{RED}[ERROR]{RESET} {test._testMethodName}")
        print(f"   -> {err[1]}")

def compile_main_project():
    print(f"{YELLOW}[*] Compiling main webserv project...{RESET}")
    os.chdir("..")
    subprocess.run(["make", "fclean"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    res = subprocess.run(["make"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    os.chdir("tests")
    return res.returncode == 0, res.stderr

def compile_unit_tester():
    print(f"{YELLOW}[*] Compiling C++ unit_tester tool...{RESET}")
    cmd = [
        "c++", "-Wall", "-Wextra", "-Werror", "-std=c++98",
        "-I../include",
        "unit_tester.cpp",
        "../src/parser/RequestParser.cpp",
        "../src/network/SocketEngine.cpp",
        "../src/network/Connection.cpp",
        "../src/http/StatusCodes.cpp",     # ADDED Przemek tests
        "../src/http/HttpResponse.cpp",    # ADDED Przemek tests
        "../src/http/HttpErrorPage.cpp",   # ADDED Przemek tests
        "../src/http/HttpRequest.cpp",     # ADDED Przemek tests
        "../src/network/EventLoop.cpp",    # DODANO: Plik Bartka
        "../src/core/Server.cpp",          # DODANO: Plik Bartka
        "../src/cgi/CgiHandler.cpp",       # CGI Handler
        "../src/cgi/CgiProcess.cpp",       # CGI Process
        "../src/cgi/CgiEnv.cpp",           # CGI Env
		"../src/utils/Logger.cpp",         # Logger
		"../src/routing/LocationConfig.cpp", # LocationConfig
		"../src/core/Config.cpp",         # Config Parser
		"../src/routing/Router.cpp",       # Routing
        "-o", "unit_tester"
    ]
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return res.returncode == 0, res.stderr

def start_main_server():
    """Helper used to start the actual server for full-feature integration testing"""
    global SERVER_PROCESS
    config_path = "default_test.conf"
    if not os.path.exists(config_path):
        with open(config_path, "w") as f:
            f.write("server {\n    listen 8080;\n    server_name localhost;\n}")
            
    SERVER_PROCESS = subprocess.Popen(
        ["../webserv", config_path],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL
    )
    time.sleep(0.2) # Allow server to boot up

def stop_main_server():
    """Helper to safely kill the live server process after integration testing"""
    global SERVER_PROCESS
    if SERVER_PROCESS and SERVER_PROCESS.poll() is None:
        SERVER_PROCESS.terminate()
        SERVER_PROCESS.wait()

if __name__ == "__main__":
    os.chdir(os.path.dirname(os.path.abspath(__file__)))

    main_compiled, main_err = compile_main_project()
    if not main_compiled:
        print(f"{RED}[FAIL] Main project compilation failed!{RESET}\n{main_err}")
        sys.exit(1)
    print(f"{GREEN}[OK] Main project compiled successfully.{RESET}")

    tester_compiled, tester_err = compile_unit_tester()
    if not tester_compiled:
       print(f"{RED}[FAIL] Unit tester compilation failed!{RESET}\n{tester_err}")
       sys.exit(1)
    print(f"{GREEN}[OK] Unit tester tool compiled successfully.{RESET}")

    try:
        print(f"\n{YELLOW}=== RUNNING REGRESSION TESTS ==={RESET}\n")
        
        loader = unittest.TestLoader()
        suite = loader.discover(start_dir="test_cases", pattern="test_*.py")
        
        runner = unittest.TextTestRunner(resultclass=CustomTestResult, verbosity=0)
        result = runner.run(suite)
        
        print(f"\n{YELLOW}========================================{RESET}")
        if result.wasSuccessful():
            print(f"{GREEN}SUCCESS: All regression tests ({result.testsRun}) passed!{RESET}")
            sys.exit(0)
        else:
            print(f"{RED}FAILURE: Regression tests failed! Total executed: {result.testsRun}{RESET}")
            sys.exit(1)
            
    except Exception as e:
        print(f"{RED}Unexpected testing error: {e}{RESET}")
        sys.exit(1)