This document provides a short guide on how our \*\*Regression and Unit Testing Environment\*\* is structured and how to use it to verify the `RequestParser` class and general application stability without needing a live network layer.



\---



\### How the Environment Works



Our testing framework uses a \*\*hybrid design\*\* that combines a lightweight C++ test harness with Python's built-in `unittest` automation module.



1\. \*\*The Infrastructure Layer (`test\_00\_infra.py`)\*\*: Tests that our main `webserv` binary compiles successfully via `Makefile` and can launch without immediately crashing or throwing memory violations (like a segmentation fault).

2\. \*\*The C++ Integration Bridge (`unit\_tester.cpp`)\*\*: Since our `RequestParser` uses a stream-oriented incremental data feeder (`RequestParser::feed()`) and keeps internal validation logic private, Python cannot inspect it directly. The `unit\_tester.cpp` binary acts as an intermediary. It instantiates the parser, feeds it raw HTTP payloads passed from Python, and prints whether the parser reaches `STATE\_COMPLETE` or `STATE\_ERROR`.

3\. \*\*The Python Automation Engine (`run\_tests.py`)\*\*: Automates the compilation of both our main project and the `unit\_tester` tool. It then dynamically discovers all tests inside the `test\_cases/` directory, executes them, and displays real-time, color-coded tracking logs (`\[TEST PASSED]` / `\[TEST FAILED]`).



\---



\### Repository Directory Layout



Ensure your test-related files match this architectural blueprint inside the root of your workspace:



```text

├── Makefile                     # Main project Makefile

├── src/                         # Core C++ application files

│   └── parser/

│       └── RequestParser.cpp    # Actual parser implementation

└── tests/

&#x20;   ├── run\_tests.py             # Main test orchestration script

&#x20;   ├── unit\_tester.cpp          # C++ test bridge utility

&#x20;   └── test\_cases/

&#x20;       ├── \_\_init\_\_.py          # Python package initializer (Leave empty)

&#x20;       ├── test\_00\_infra.py     # Checks compilation \& startup stability

&#x20;       ├── test\_01\_request\_line.py # Validates Request Line tokens \& constraints

&#x20;       ├── test\_02\_headers.py   # Validates HTTP Header Maps and syntax

&#x20;       └── test\_03\_body.py      # Validates Content-Length and Chunked streams



```



\---



\### How to Run the Tests



\#### 1. Preparation



Before running the tests for the first time, make sure the primary Python orchestration runner script has execution permissions in your Unix system:



```bash

chmod +x tests/run\_tests.py



```



\#### 2. Execution



To run the entire suite of regression tests, simply execute the script from your terminal:



```bash

python3 tests/run\_tests.py



```



\#### What happens when you run it:



\* The engine drops into the root directory and executes `make fclean \&\& make` to perform a clean compilation check on our codebase.

\* It compiles the `unit\_tester` utility alongside our real `RequestParser.cpp` implementation.

\* It goes through every scenario inside the `test\_cases/` folder sequentially and prints a formatted summary. If all assertions pass, it exits with code `0`. If any regressions or errors are found, it outputs the exact point of failure and exits with code `1`.



\---



\### How to Add New Tests



When you implement a new feature or want to test an edge case (e.g., more HTTP body malformations or routing parameters), follow these steps:



1\. Look inside the `tests/test\_cases/` directory and find the file corresponding to the component you are working on (or create a new file named `test\_something.py`).

2\. Define your test cases inside a class inheriting from `unittest.TestCase`.

3\. Use the `run\_tester(raw\_payload)` helper to push custom string configurations to the C++ parser.



\#### Code Example for adding a test:



```python

def test\_duplicate\_content\_length\_rejected(self):

&#x20;   """Invalid HTTP Scenario: Reject requests sending duplicate Content-Length headers"""

&#x20;   payload = (

&#x20;       "POST /submit HTTP/1.1\\r\\n"

&#x20;       "Host: localhost\\r\\n"

&#x20;       "Content-Length: 5\\r\\n"

&#x20;       "Content-Length: 10\\r\\n\\r\\n"

&#x20;   )

&#x20;   out = self.run\_tester(payload)

&#x20;   # Ensure our parser catches this and moves into STATE\_ERROR

&#x20;   self.assertEqual(out\[0], "FAILED")



```



Every time you commit changes or prepare a pull request on your branch, run `python3 tests/run\_tests.py` to make sure your modifications haven't broken any previously written features!

