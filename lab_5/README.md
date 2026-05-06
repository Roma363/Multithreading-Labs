# Lab 5 (HTTP server)

Ця лабораторна — багатопотоковий HTTP сервер на WinSock + Win32 threads. Статичні файли — у public/.

## Структура папок

```
lab_5/
  CMakeLists.txt
  README.md
  src/           # основний код сервера
    server.cpp
    cache.cpp
  include/       # заголовочні файли
    cache.h
  public/        # статичні файли для HTTP
    index.html
    page2.html
  tests/         # всі тести (locust, майбутні автотести)
    locustfile.py
```

## Збірка (CMake)

```powershell
cmake -S "D:\Multithreading-Labs" -B "D:\Multithreading-Labs\build" -G Ninja
cmake --build "D:\Multithreading-Labs\build"
```

## Запуск

```powershell
"D:\Multithreading-Labs\build\lab_5_server.exe"
```

## Тестування

Тестування проводиться через locust (Python):

```powershell
cd ../tests
locust -f locustfile.py
```

## Примітки
- Після збірки public/ автоматично копіюється у спільну build/.
- Сервер WinSock+Win32, кросплатформеності немає.
