from locust import HttpUser, task, between

class ServerTester(HttpUser):
    wait_time = between(0.1, 1.0)

    @task(2)
    def load_index(self):
        self.client.get("/")

    @task(1) 
    def load_page2(self):
        self.client.get("/page2.html")
        
    @task(1)
    def load_404(self):
        with self.client.get("/404", catch_response=True) as response:
            if response.status_code == 404:
                response.success()
