import os
import time
from urllib.parse import quote_plus

import matplotlib.pyplot as plt
import pandas as pd
from sqlalchemy import create_engine, text

PRICE_COLORS = ("#00ff00", "#00ccff", "#ff66ff", "#ffcc00", "#66ffcc")
AVG_COLORS = ("#ff9900", "#ffaa55", "#ff77aa", "#ccaa66", "#88ddff")


def load_tickers():
    raw = os.getenv("TICKERS", "BTC/USDT")
    return [s.strip() for s in raw.split(",") if s.strip()]


def chart_filename(symbol: str) -> str:
    safe = symbol.replace("/", "_").replace("\\", "_")
    return f"/app/charts/live_{safe}.png"


def make_engine():
    user = os.getenv("POSTGRES_USER", "vmkzy")
    password = os.getenv("POSTGRES_PASSWORD", "")
    db = os.getenv("POSTGRES_DB", "crypto_db")
    host = os.getenv("DB_HOST", "localhost")
    url = (
        f"postgresql+psycopg2://{quote_plus(user)}:{quote_plus(password)}"
        f"@{host}:5432/{quote_plus(db)}"
    )
    return create_engine(url)


def plot_ticker(engine, symbol: str, price_color: str, avg_color: str):
    try:
        query = text(
            "SELECT created_at, price, avg_price FROM crypto_history "
            "WHERE ticker = :ticker ORDER BY created_at DESC LIMIT 100"
        )
        df = pd.read_sql(query, engine, params={"ticker": symbol})

        if df.empty:
            return

        df = df.sort_values("created_at")
        plt.figure(figsize=(10, 5))
        plt.style.use("dark_background")
        plt.plot(df["created_at"], df["price"], label=f"{symbol} price", color=price_color, linewidth=2)
        plt.plot(
            df["created_at"],
            df["avg_price"],
            label="Rolling avg (backend)",
            color=avg_color,
            linestyle="--",
        )
        plt.title(f"Live — {symbol}")
        plt.legend()
        plt.xticks(rotation=45)
        plt.tight_layout()

        out_path = chart_filename(symbol)
        plt.savefig(out_path)
        plt.close()
        print(f"Chart updated: {out_path}")
    except Exception as e:
        print(f"{symbol}: waiting for data or DB: {e}")


def plot_all(engine, tickers):
    for i, symbol in enumerate(tickers):
        plot_ticker(
            engine,
            symbol,
            PRICE_COLORS[i % len(PRICE_COLORS)],
            AVG_COLORS[i % len(AVG_COLORS)],
        )


if __name__ == "__main__":
    os.makedirs("/app/charts", exist_ok=True)
    tickers = load_tickers()
    print(f"Analytics for: {', '.join(tickers)}")
    engine = make_engine()
    while True:
        plot_all(engine, tickers)
        time.sleep(20)
