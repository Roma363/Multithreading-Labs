# Lab 5: Non-blocking HTTP Server

Event-driven HTTP/1.1 static file server using raw sockets on Windows (WinSock + `select`).

## Build

```bash
cd lab_5
cmake -S . -B build
cmake --build build
```

## Run

```bash
.\build\bin\lab_5 --port=8080 --root=www
```

Open: http://localhost:8080/ or http://localhost:8080/page2.html

## Load Testing (Locust)

**Methodology:** Use Locust to generate GET traffic for `/`, `/page2.html`, and a missing page to validate 404 handling. The load script(s) live under `lab_5/test/` and should be launched from there.

**Run and export:**

```powershell
cd lab_5\test
locust -f locustfile.py --host http://localhost:8080
```

Export the results to `lab_5/test/result.csv`.
