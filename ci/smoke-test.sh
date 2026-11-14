#!/usr/bin/env bash
# Сквозная проверка готового приложения на настоящей базе:
# схема + демо-данные, затем сценарий в меню через stdin.
#   ci/smoke-test.sh <путь к pvz> <имя базы>
# Подключение берётся из переменных PG* (для psql) и PVZ_DB_* (для приложения).
set -euo pipefail

APP="${1:?укажите путь к исполняемому файлу pvz}"
DB="${2:?укажите имя базы}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

psql -d "$DB" -v ON_ERROR_STOP=1 -q -1 -f "$ROOT/sql/init.sql"
psql -d "$DB" -v ON_ERROR_STOP=1 -q -1 -f "$ROOT/sql/seed.sql"
# Повторное применение не должно падать.
psql -d "$DB" -v ON_ERROR_STOP=1 -q -1 -f "$ROOT/sql/init.sql" 2>/dev/null

# 7 — вернуть просроченные (фоновый поток мог успеть первым, поэтому смотрим на итог),
# 4 — карточка OZ-200002, 2 — выдать WB-100001 с кодом 1234, 6 — список на хранении, 0 — выход.
OUTPUT="$(printf '7\n4\nOZ-200002\n2\nWB-100001\n1234\n6\n0\n' | PVZ_DB_NAME="$DB" "$APP")"
echo "$OUTPUT"

check() {
    if ! grep -qF "$1" <<<"$OUTPUT"; then
        echo "::error::в выводе нет строки: $1"
        exit 1
    fi
}
check "Статус:  возвращён отправителю"  # OZ-200002 из seed.sql принят 10 дней назад
check "Заберите посылку из ячейки A-01"
check "На хранении ничего нет."
check "До свидания!"

# Без доступной базы приложение должно завершиться с кодом 3 и понятным сообщением.
set +e
echo 0 | PVZ_DB_HOST=127.0.0.1 PVZ_DB_PORT=1 "$APP" >/dev/null 2>"$ROOT/smoke-error.txt"
CODE=$?
set -e
grep -q "Не удалось подключиться к PostgreSQL" "$ROOT/smoke-error.txt"
rm -f "$ROOT/smoke-error.txt"
if [ "$CODE" -ne 3 ]; then
    echo "::error::ожидался код выхода 3 без базы, получен $CODE"
    exit 1
fi
echo "Smoke-тест пройден"
