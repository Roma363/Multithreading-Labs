from locust import HttpUser, task, between

class Lab5LoadTester(HttpUser):
    # Затримка між діями одного користувача (від 1 до 3 секунд)
    wait_time = between(1, 3)

    # Цифра в дужках — це "вага" завдання.
    # task(3) означає, що це завдання буде виконуватися в 3 рази частіше за task(1).
    # Адже в реальності люди частіше ходять по існуючих сторінках.

    @task(3)
    def load_index_page(self):
        """Тестування головної сторінки"""
        self.client.get("/")

    @task(3)
    def load_second_page(self):
        """Тестування другої сторінки"""
        self.client.get("/page2.html")

    @task(1)
    def test_404_not_found(self):
        """Тестування коректної обробки неіснуючої сторінки"""
        # catch_response=True дозволяє нам самостійно вирішити, чи був запит успішним
        with self.client.get("/fake_page.html", catch_response=True) as response:
            if response.status_code == 404:
                # Сервер правильно повернув 404, тому для нас це "успішний" тест
                response.success()
            else:
                # Якщо сервер повернув щось інше (наприклад, 200 або впав), фіксуємо помилку
                response.failure(f"Expected 404, but got {response.status_code}")

