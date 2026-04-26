# Real Time Crypto Analytics

Мониторинг криптовалют в реальном времени с хранением истории в PostgreSQL, визуализацией графиков и веб-дашбордом.

## Что делает проект

Проект состоит из нескольких сервисов, которые работают вместе:

- `ingestor` получает цены с Binance через `ccxt`.
- `backend` принимает обновления, сохраняет их в PostgreSQL и отдаёт HTTP API.
- `analytics` строит PNG-графики по данным из базы.
- `db` хранит историю цен и рассчитанные средние значения.

На странице дашборда показываются последняя цена, среднее значение и статус движения рынка для каждого тикера. Графики вынесены на отдельную страницу, чтобы их было удобно просматривать без постоянного обновления.

## Возможности

- Получение цен в реальном времени по одному или нескольким тикерам.
- Сохранение истории в PostgreSQL.
- Расчёт rolling average на стороне backend по последним 100 значениям.
- Веб-страница `/dashboard` с таблицей и картами графиков.
- JSON API для просмотра истории.
- Автоматическое построение графиков в PNG.

## Технологии

- Python 3.9
- C++17
- PostgreSQL 16
- Docker и Docker Compose
- Binance API через `ccxt`
- Backend HTTP-сервер на `Crow`
- Аналитика и графики: `pandas`, `matplotlib`, `SQLAlchemy`

## Архитектура

1. `Python/sender.py` опрашивает Binance и отправляет цену в `POST /update`.
2. `main.cpp` принимает данные, вычисляет среднее значение, пишет запись в `crypto_history`.
3. `Python/main.py` читает историю из базы и сохраняет графики в `/app/charts`.
4. `GET /dashboard` рендерит только таблицу и обновляется каждую секунду.
5. `GET /charts-view` показывает отдельную страницу с графиками без автообновления.

## Структура проекта

- `main.cpp` - C++ backend с HTTP-эндпоинтами.
- `Python/sender.py` - отправка рыночных данных в backend.
- `Python/main.py` - генерация графиков и аналитика.
- `init-db.sql` - создание таблицы и индекса.
- `docker-compose.yml` - запуск всего стека.
- `Dockerfile.backend` - сборка backend-сервиса.
- `Dockerfile.ingestor` - образ для Python-сервисов.

## Требования

- Docker
- Docker Compose

Для локального запуска без Docker понадобятся совместимые версии Python 3.9+ и компилятор C++17 с библиотекой `pqxx`, но основной способ запуска - через Docker Compose.

## Быстрый старт

1. Склонируйте репозиторий.
2. Создайте файл `.env` в корне проекта.
3. Заполните переменные окружения.
4. Запустите сервисы.

```bash
docker compose up --build
```

Если у вас старая версия Compose, можно использовать:

```bash
docker-compose up --build
```

После запуска откройте:

- Dashboard: `http://localhost:8080/dashboard`
- Charts: `http://localhost:8080/charts-view`
- History API: `http://localhost:8080/history`

## Переменные окружения

Пример `.env`:

```env
POSTGRES_USER=vmkzy
POSTGRES_PASSWORD=your_password
POSTGRES_DB=crypto_db
TICKERS=BTC/USDT,ETH/USDT,SOL/USDT
```

Описание переменных:

- `POSTGRES_USER` - пользователь PostgreSQL.
- `POSTGRES_PASSWORD` - пароль PostgreSQL.
- `POSTGRES_DB` - имя базы данных.
- `TICKERS` - список тикеров через запятую, например `BTC/USDT,ETH/USDT`.

Если `TICKERS` не задан, используется `BTC/USDT`.

## Порты

- `8080` - backend и веб-дашборд.
- `5432` - PostgreSQL.

## API

### `POST /update`

Принимает обновление цены и сохраняет его в базу.

Пример тела запроса:

```json
{
   "ticker": "BTC/USDT",
   "price": 65432.12
}
```

Ответ:

- `200 OK` - запись успешно сохранена.
- `400 Bad Request` - некорректный JSON.
- `500 Internal Server Error` - ошибка базы данных.

### `GET /history`

Возвращает последние 10 записей из `crypto_history` в JSON.

### `GET /dashboard`

HTML-страница с таблицей текущих значений. Обновляется каждую секунду.

### `GET /charts-view`

Отдельная HTML-страница с графиками без автообновления.

### `GET /charts/<name>`

Отдаёт PNG-файл графика из каталога `/app/charts`.

## База данных

При старте выполняется `init-db.sql`, который создаёт таблицу:

- `crypto_history` - тикер, цена, среднее значение и время записи.

Также создаётся индекс по `(ticker, created_at DESC)` для быстрого чтения последних значений.

## Как работает аналитика

Сервис `analytics` запускает `Python/main.py` и каждые 20 секунд:

- читает последние 100 записей по каждому тикеру;
- строит график цены и rolling average;
- сохраняет PNG в `/app/charts`.

Dashboard автоматически подхватывает эти изображения.

## Полезные заметки

- Если данные не появляются сразу, дождитесь первого цикла `ingestor` и `analytics`.
- Для нескольких тикеров задавайте их одним списком в `TICKERS`.
- Графики и история зависят от доступности PostgreSQL и сети до Binance.

## Пример сценария запуска

```bash
git clone https://github.com/vmkzy/Real-Time-Crypto-Analytics.git
cd Real-Time-Crypto-Analytics
copy NUL .env
docker compose up --build
```

После этого откройте `http://localhost:8080/dashboard`.

