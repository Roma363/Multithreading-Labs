#!/usr/bin/env python3
"""
Load testing script for the HTTP server.
Run this after starting the server.

Usage (from WSL):
  python3 scripts/run_load_test.py --host=localhost --port=8080 --users=100 --time=60
"""

import argparse
import csv
import subprocess
import time
from datetime import datetime


def run_locust_test(host, port, users, duration, output_file):
    """Run Locust load test and capture results."""
    url = f"http://{host}:{port}"
    
    print(f"Starting load test: {users} users for {duration}s against {url}")
    print(f"Results will be saved to {output_file}")
    
    cmd = [
        "locust",
        "-f", "scripts/locustfile.py",
        "--headless",
        f"--users={users}",
        f"--spawn-rate={users}",
        f"--run-time={duration}s",
        f"--host={url}",
        "--csv=scripts/results",
        "--stop-timeout=10"
    ]
    
    start_time = datetime.now()
    try:
        result = subprocess.run(cmd, cwd=".", capture_output=False, text=True)
        end_time = datetime.now()
        
        print(f"\nTest completed in {(end_time - start_time).total_seconds():.2f}s")
        print("Check scripts/results_stats.csv for detailed results")
        
        return result.returncode == 0
    except Exception as e:
        print(f"Error running locust: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(description="Load test the HTTP server")
    parser.add_argument("--host", default="localhost", help="Server host")
    parser.add_argument("--port", type=int, default=8080, help="Server port")
    parser.add_argument("--users", type=int, default=100, help="Number of concurrent users")
    parser.add_argument("--time", type=int, default=60, help="Test duration in seconds")
    parser.add_argument("--output", default="scripts/results", help="Output file prefix")
    
    args = parser.parse_args()
    
    success = run_locust_test(args.host, args.port, args.users, args.time, args.output)
    exit(0 if success else 1)


if __name__ == "__main__":
    main()
