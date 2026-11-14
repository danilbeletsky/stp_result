-- Демонстрационные данные (необязательно). Применять после init.sql:
--   psql -d pvz -v ON_ERROR_STOP=1 -1 -f sql/seed.sql
-- Повторный запуск не создаёт дубликатов.

INSERT INTO cells (code, size) VALUES
    ('A-01', 'S'), ('A-02', 'S'), ('A-03', 'S'),
    ('B-01', 'M'), ('B-02', 'M'),
    ('C-01', 'L')
ON CONFLICT (code) DO NOTHING;

INSERT INTO customers (full_name, phone) VALUES
    ('Иванов Иван',     '+79161234567'),
    ('Петрова Анна',    '+79035550101'),
    ('Сидоров Пётр',    '+79267770022')
ON CONFLICT (phone) DO NOTHING;

-- Один свежий заказ и один, у которого срок хранения (7 дней по умолчанию) уже истёк:
-- его вернёт фоновая проверка после запуска приложения.
INSERT INTO orders (tracking_code, customer_id, cell_id, parcel_size, pickup_code, received_at)
SELECT 'WB-100001', c.id, cl.id, 'S', '1234', now() - interval '1 day'
FROM customers c, cells cl
WHERE c.phone = '+79161234567' AND cl.code = 'A-01'
ON CONFLICT DO NOTHING;

INSERT INTO orders (tracking_code, customer_id, cell_id, parcel_size, pickup_code, received_at)
SELECT 'OZ-200002', c.id, cl.id, 'M', '5678', now() - interval '10 days'
FROM customers c, cells cl
WHERE c.phone = '+79035550101' AND cl.code = 'B-01'
ON CONFLICT DO NOTHING;
