-- Схема базы данных пункта выдачи заказов.
-- Скрипт можно применять повторно: существующие таблицы и данные не трогаются.
--   psql -d pvz -v ON_ERROR_STOP=1 -1 -f sql/init.sql
-- (-1 выполняет весь файл одной транзакцией)

-- Клиенты. Телефон хранится в нормализованном виде и однозначно определяет клиента.
CREATE TABLE IF NOT EXISTS customers (
    id         BIGSERIAL    PRIMARY KEY,
    full_name  VARCHAR(100) NOT NULL CHECK (length(btrim(full_name)) > 0),
    phone      VARCHAR(12)  NOT NULL UNIQUE CHECK (phone ~ '^\+7[0-9]{10}$'),
    created_at TIMESTAMPTZ  NOT NULL DEFAULT now()
);

-- Ячейки хранения. Занятость не хранится: ячейка занята, если в ней лежит заказ со статусом 'stored'.
CREATE TABLE IF NOT EXISTS cells (
    id   BIGSERIAL   PRIMARY KEY,
    code VARCHAR(10) NOT NULL UNIQUE CHECK (code ~ '^[A-Z0-9-]+$'),
    size CHAR(1)     NOT NULL CHECK (size IN ('S', 'M', 'L'))
);

-- Заказы.
--   stored   — лежит в ячейке, ждёт клиента
--   issued   — выдан клиенту
--   returned — возвращён отправителю
CREATE TABLE IF NOT EXISTS orders (
    id            BIGSERIAL   PRIMARY KEY,
    tracking_code VARCHAR(30) NOT NULL UNIQUE CHECK (tracking_code ~ '^[A-Z0-9-]{4,30}$'),
    customer_id   BIGINT      NOT NULL REFERENCES customers (id) ON DELETE RESTRICT,
    -- Ячейку можно удалить из справочника, когда в ней ничего нет; в старых заказах ссылка обнулится.
    cell_id       BIGINT      REFERENCES cells (id) ON DELETE SET NULL,
    parcel_size   CHAR(1)     NOT NULL CHECK (parcel_size IN ('S', 'M', 'L')),
    pickup_code   CHAR(4)     NOT NULL CHECK (pickup_code ~ '^[0-9]{4}$'),
    status        VARCHAR(10) NOT NULL DEFAULT 'stored'
                              CHECK (status IN ('stored', 'issued', 'returned')),
    received_at   TIMESTAMPTZ NOT NULL DEFAULT now(),
    closed_at     TIMESTAMPTZ,

    CONSTRAINT orders_closed_at_matches_status CHECK ((status = 'stored') = (closed_at IS NULL)),
    CONSTRAINT orders_stored_has_cell CHECK (status <> 'stored' OR cell_id IS NOT NULL)
);

-- В одной ячейке не может одновременно лежать два заказа.
CREATE UNIQUE INDEX IF NOT EXISTS uq_orders_occupied_cell
    ON orders (cell_id) WHERE status = 'stored';

CREATE INDEX IF NOT EXISTS ix_orders_customer
    ON orders (customer_id);

-- Для фоновой проверки просроченных заказов.
CREATE INDEX IF NOT EXISTS ix_orders_stored_received_at
    ON orders (received_at) WHERE status = 'stored';
