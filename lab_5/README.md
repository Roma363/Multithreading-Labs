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

## Load Testing

Install Locust:
```bash
pip install -r requirements.txt
```

Run test:
```bash
python3 scripts/run_load_test.py --host=localhost --port=8080 --users=100 --time=60
```

Results saved to `scripts/results_stats.csv`.


