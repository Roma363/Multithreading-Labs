# Lab 4 (Step 2)

Client-server app for Lab 1 task with application-level protocol: DATA + START + STATUS/RESULT.
Сервер обробляє кілька клієнтів одночасно (кожне підключення у власному потоці).

## Build (WSL / POSIX)

```bash
cmake -S . -B build
cmake --build build
```

## Run

Start server:

```bash
./build/bin/lab_4_server
```

Run C++ client (default values):

```bash
./build/bin/lab_4_client
```

Run Go client (default values):

```bash
cd lab_4/go_client
go run .
```

## Protocol (Step 2)

Message header (all network order):
- `uint32 magic` = `0x4D4D5831` ("MMX1")
- `uint16 version` = `1`
- `uint16 type`
- `uint32 length` (payload bytes)

Command types:
- `MSG_DATA = 1`
- `MSG_START = 2`
- `MSG_STATUS = 3`

Response types:
- `MSG_DATA_OK = 101`
- `MSG_START_OK = 102`
- `MSG_STATUS_RESP = 103`
- `MSG_ERROR = 200`

Payloads:
- `MSG_DATA`: `uint32 rows`, `uint32 cols`, `uint32 numThreads`, then `rows*cols` of `int32` values
- `MSG_START`: empty
- `MSG_STATUS`: empty
- `MSG_STATUS_RESP`: `uint32 status`, `int32 min`, `int32 max`, `uint32 error`
- `MSG_ERROR`: `uint32 code`

Status codes:
- `STATUS_IDLE = 0`, `STATUS_READY = 1`, `STATUS_RUNNING = 2`, `STATUS_DONE = 3`, `STATUS_ERROR = 4`

Error codes:
- `ERR_INVALID_PAYLOAD = 1`, `ERR_NO_DATA = 2`, `ERR_ALREADY_RUNNING = 3`, `ERR_INTERNAL = 4`
