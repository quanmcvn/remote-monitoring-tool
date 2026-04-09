# Remote Monitoring Tool (project)

This is a Remote Monitoring Tool project for learning purposes, obviously not for anything production related. This aims to collect information about processes' resources (CPU, Memory, Disk, Network) and logs it to a remote server when things get out of hands (aka too much resources consumed).

## Build and run

Install [CMake](https://cmake.org/download/) and run:

```bash
cmake -S . -B build/
cmake --build build/
```

Run `server` in `build/src/server/server`, additionally pass args `--server-port=<port>` (defaults to `12345`).

Run `client` in `build/src/client/client`, additionally pass args `--server-ip=<ip>` and/or `--server-port=<port>` (defaults to `127.0.0.1` and `12345`). NOTE: Requires root in Linux or Administrator in Windows.

## Features

Input from server like so: `<client-id>:<message>`, `client-id` will be given by server (count from 1).

`message` is actually only `config <config-file>`, which will send `config-file` to client to update

Client will autonomously collect processes' information (CPU, Memory, Disk usage, Network usage) and send to server. Server will then send ack back to client so that client can move on to newer logs (similar to TCP ack). Client will also write logs to file locally to prevent losing logs from crashes, or just normal shutdown.

Supports multiple clients (as evident by `<client-id>:<message>` input from server). Also support Windows (tested on Windows 11 x64).

There are also some tests, can be ran by `ctest --test-dir build` after building.
