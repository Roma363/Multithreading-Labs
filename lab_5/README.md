# Lab 5

## Збірка (CMake)

```powershell
cmake -S "D:\Multithreading-Labs" -B "D:\Multithreading-Labs\build" -G Ninja
cmake --build "D:\Multithreading-Labs\build"
```

## Запуск

```powershell
"D:\Multithreading-Labs\build\lab_5_server.exe"
```
Open: http://localhost:8080/ or http://localhost:8080/second_page.html

## Тестування

Тестування проводиться через locust (Python):

```powershell
cd ../tests
locust -f locustfile.py
```
