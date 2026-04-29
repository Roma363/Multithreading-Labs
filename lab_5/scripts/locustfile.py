from locust import HttpUser, task, between
import random


class WebServerUser(HttpUser):
    wait_time = between(0.1, 0.5)

    @task(3)
    def get_index(self):
        """Load the main page."""
        self.client.get("/")

    @task(2)
    def get_second_page(self):
        """Load the second page."""
        self.client.get("/second_page.html")

    @task(1)
    def get_nonexistent(self):
        """Try to access non-existent page (404 test)."""
        self.client.get("/nonexistent_" + str(random.randint(1, 100)) + ".html")
