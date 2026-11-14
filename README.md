# Пункт выдачи заказов (ПВЗ)

[![PVZ CI](https://github.com/danilbeletsky/stp_result/actions/workflows/ci.yml/badge.svg)](https://github.com/danilbeletsky/stp_result/actions/workflows/ci.yml)

Консольное приложение на C++20 для оператора пункта выдачи: приём посылок в ячейки,
выдача по коду из SMS, возврат отправителю, учёт клиентов и ячеек.
Данные хранятся в PostgreSQL. Фоновый поток раз в минуту возвращает отправителю заказы,
у которых истёк срок хранения.

## Зависимости

| Что | Версия | macOS (Homebrew) | Ubuntu/Debian |
|---|---|---|---|
| Компилятор C++20 | Clang 15+ / GCC 11+ | Xcode Command Line Tools | `g++` |
| CMake | 3.20+ | `brew install cmake` | `apt install cmake` |
| PostgreSQL | 13+ | `brew install postgresql@16` | `apt install postgresql` |
| libpqxx | 7.10+ (проверено на 8.0) | `brew install libpqxx` | `apt install libpqxx-dev` |
| GoogleTest | любая | `brew install googletest` | `apt install libgtest-dev` |

GoogleTest необязателен: если он не установлен, CMake скачает его сам.

## Сборка

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Без тестов: добавьте `-DPVZ_BUILD_TESTS=OFF`.

## База данных

Создать базу и применить скрипт инициализации:

```bash
createdb pvz
psql -d pvz -v ON_ERROR_STOP=1 -1 -f sql/init.sql
```

Скрипт можно запускать повторно — он не удаляет существующие данные.
Демонстрационные данные (6 ячеек, 3 клиента, 2 заказа, один из них уже просрочен):

```bash
psql -d pvz -v ON_ERROR_STOP=1 -1 -f sql/seed.sql
```

## Настройка подключения

Через переменные окружения:

```bash
export PVZ_DB_HOST=localhost
export PVZ_DB_PORT=5432
export PVZ_DB_NAME=pvz
export PVZ_DB_USER=postgres
export PVZ_DB_PASSWORD=secret
```

или через файл `pvz.conf` в текущем каталоге (либо `--config <путь>`):

```bash
cp pvz.conf.example pvz.conf
```

Переменные окружения важнее файла. Все параметры:

| Параметр | По умолчанию | Назначение |
|---|---|---|
| `PVZ_DB_HOST` | `localhost` | хост или каталог сокета (например `/tmp`) |
| `PVZ_DB_PORT` | `5432` | порт |
| `PVZ_DB_NAME` | `pvz` | имя базы |
| `PVZ_DB_USER` | пользователь ОС | пользователь PostgreSQL |
| `PVZ_DB_PASSWORD` | пусто | пароль |
| `PVZ_STORAGE_DAYS` | `7` | срок хранения заказа, дней |
| `PVZ_CHECK_INTERVAL_SEC` | `60` | период фоновой проверки, секунд |

## Запуск

```bash
./build/pvz
```

Коды выхода: `0` — нормальное завершение, `1` — неверные аргументы,
`2` — ошибка конфигурации, `3` — нет подключения к базе.

Типичный сценарий: «Ячейки → Добавить», «Принять заказ» (клиента можно зарегистрировать
прямо при приёме), программа выбирает самую маленькую подходящую свободную ячейку и
выдаёт 4-значный код. «Выдать заказ» — по трек-номеру и этому коду.
Пустая строка в любом вопросе отменяет текущую операцию.

## Тесты

```bash
ctest --test-dir build --output-on-failure
```

Модульные тесты (домен, сервисы на репозиториях в памяти, фоновый поток, конфигурация)
работают без PostgreSQL. Интеграционные тесты репозиториев запускаются на **отдельной**
базе — они пересоздают в ней таблицы:

```bash
createdb pvz_test
PVZ_TEST_DB_NAME=pvz_test ctest --test-dir build --output-on-failure
```

## CI

GitHub Actions ([.github/workflows/ci.yml](.github/workflows/ci.yml)) запускается на каждый
push и pull request, два задания идут параллельно:

| Задание | Компилятор | PostgreSQL | libpqxx |
|---|---|---|---|
| `ubuntu-latest` | GCC | сервисный контейнер `postgres:16` | собирается из исходников ([ci/install-libpqxx.sh](ci/install-libpqxx.sh)), кэшируется |
| `macos-latest` | Apple Clang | Homebrew, запускается на runner | Homebrew |

Шаги: сборка с `-DCMAKE_COMPILE_WARNING_AS_ERROR=ON` → все тесты, включая интеграционные
на настоящей базе → smoke-тест готового приложения ([ci/smoke-test.sh](ci/smoke-test.sh)):
применяет `init.sql` и `seed.sql`, проходит сценарий в меню и проверяет код выхода 3 без базы.
Отчёт тестов (JUnit) и собранный `pvz` выкладываются в артефакты запуска.

## Устройство

```
src/
├── domain/       сущности: Customer, Cell, Order, ParcelSize — правила и проверки
├── repository/   интерфейсы доступа к данным и ошибки хранилища
│   └── postgres/ реализация на libpqxx — единственное место с SQL
├── service/      бизнес-логика: CustomerService, CellService, OrderService, PeriodicWorker
├── config/       чтение настроек из файла и окружения
├── cli/          консольное меню и ввод
└── main.cpp      сборка объектов и передача зависимостей
sql/              init.sql — схема, seed.sql — демо-данные
tests/            GoogleTest
ci/               скрипты для CI: сборка libpqxx и smoke-тест
```

Зависимости направлены внутрь: `cli → service → domain`, `repository/postgres → domain`.
Каждый слой — отдельная библиотека в CMake, и только `pvz_postgres` линкуется с libpqxx.
Сервисы получают репозитории через конструктор в виде интерфейсов, поэтому в тестах
вместо PostgreSQL подставляются реализации в памяти.

Фоновая проверка (`PeriodicWorker`) работает в своём `std::thread` и со своим соединением
с базой: соединение libpqxx нельзя использовать из двух потоков. Остановка — флаг под
мьютексом + `condition_variable` + `join()`, поэтому выход не ждёт конца интервала.
Если оператор и фоновый поток одновременно закрывают один заказ, побеждает первый:
`UPDATE ... WHERE status = 'stored'` не перезапишет уже закрытый заказ.
