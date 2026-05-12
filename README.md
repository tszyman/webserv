
# Project: webserv

## Team

- Bartek
- Tomek
- Przemek

## Description

## Goals

## Worksplit

We've identified the following blocks and assigned as follows:

- Socket Engine (Bartek)
- Event Loop (Bartek)
- HTTP Parser (Tomek)
- CGI Logic (Tomek)
- Static files & HTTP responses  (Bartek)
- Uploads (Przemek)
- Directory listing (Przemek)
- Default error pages (Przemek)
- Routing & Methods (Tomek)
- Config-dependent behavior (Tomek)
- Stability & testing (Bartek, Przemek, Tomek)
- Docs (Bartek, Przemek, Tomek)

## Tech details
### Directory structure and function mapping
#### Networking (Bartek)
- `network/` ---> Socket Engine, Event Loop, Non‑blocking, IO Multiplexing
#### Parsing (Tomek)
- `parser/` ---> Parser, Request Line, Header Map, Body Handling
#### HTTP (Bartek)
- `http/` ---> HTTP response builder, Status codes
#### CGI (Tomek)
- `cgi/` ---> Pipe Setup, Fork & Exec, Env Variables, CGI Logic
#### Filesystem (Przemek)
- `filesystem/` ---> Uploads, Directory listing, Static file serving
#### Routing (Tomek)
- `routing/` ---> Routing & methods, Config‑dependent behavior
#### Utils
- `utils/` ---> Additional helper files
#### WWW
- `www/` ---> HTTP Server contents
## AI usage

## TODO

[ ] all
