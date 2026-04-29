# Lab 5 (Steps 1–2)

HTTP/1.1 static file server using raw sockets (blocking, multi-client).

## Build (WSL Ubuntu)

```bash
cmake -S . -B build
cmake --build build
```

## Run

```bash
./build/bin/lab_5 --port=8080 --root=www
```

Then open: http://localhost:8080/

## Notes
- Implemented GET parsing and HTTP/1.1 response.
- Root path `/` maps to `index.html` in the `www` folder.
- Additional pages and extended error handling will be added in later steps.
