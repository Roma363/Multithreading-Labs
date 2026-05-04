# Lab 5: Non-blocking HTTP Server

Event-driven HTTP/1.1 static file server using raw sockets and Linux epoll.

## Build

```bash
cd lab_5
cmake -S . -B build
cmake --build build
```

## Run

```bash
./build/bin/lab_5 --port=8080 --root=www
```

Open: http://localhost:8080/ or http://localhost:8080/second_page.html


