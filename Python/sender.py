import time

import ccxt
import requests

exchange = ccxt.binance()
URL = "http://backend:8080/update"
POLL_INTERVAL = 2
REQUEST_TIMEOUT = 10
MAX_BACKOFF = 60


def fetch_price() -> float:
    ticker = exchange.fetch_ticker("BTC/USDT")
    return float(ticker["last"])


def post_update(price: float) -> None:
    payload = {"ticker": "BTC/USDT", "price": price}
    response = requests.post(URL, json=payload, timeout=REQUEST_TIMEOUT)
    response.raise_for_status()


def main() -> None:
    print("Starting REAL-TIME tracker...")
    backoff = float(POLL_INTERVAL)
    while True:
        try:
            price = fetch_price()
            post_update(price)
            print(f"Binance Price: {price} | Server: OK")
            backoff = float(POLL_INTERVAL)
        except Exception as e:
            wait = min(backoff, MAX_BACKOFF)
            print(f"Error: {e!r} (retry in {wait:.0f}s)")
            time.sleep(wait)
            backoff = min(backoff * 2, MAX_BACKOFF)
            continue
        time.sleep(POLL_INTERVAL)


if __name__ == "__main__":
    main()
