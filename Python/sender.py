import os
import time

import ccxt
import requests

exchange = ccxt.binance()
URL = "http://backend:8080/update"
POLL_INTERVAL = 2
REQUEST_TIMEOUT = 10
MAX_BACKOFF = 60


def load_tickers():
    raw = os.getenv("TICKERS", "BTC/USDT")
    return [s.strip() for s in raw.split(",") if s.strip()]


def fetch_price(symbol: str) -> float:
    ticker = exchange.fetch_ticker(symbol)
    return float(ticker["last"])


def post_update(symbol: str, price: float) -> None:
    payload = {"ticker": symbol, "price": price}
    response = requests.post(URL, json=payload, timeout=REQUEST_TIMEOUT)
    response.raise_for_status()


def main() -> None:
    tickers = load_tickers()
    print(f"Starting REAL-TIME tracker for: {', '.join(tickers)}")
    backoff = float(POLL_INTERVAL)
    while True:
        try:
            for symbol in tickers:
                price = fetch_price(symbol)
                post_update(symbol, price)
                print(f"{symbol} | Binance: {price} | Server: OK")
                time.sleep(0.2)
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
