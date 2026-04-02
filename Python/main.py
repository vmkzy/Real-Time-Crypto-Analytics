import os
import time
from urllib.parse import quote_plus

import matplotlib.pyplot as plt
import pandas as pd
from sqlalchemy import create_engine


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


def plot_crypto(engine):
    try:
        query = (
            "SELECT created_at, price, avg_price FROM crypto_history "
            "WHERE ticker = 'BTC/USDT' ORDER BY created_at DESC LIMIT 100"
        )
        df = pd.read_sql(query, engine)

        if not df.empty:
            df = df.sort_values('created_at')
            plt.figure(figsize=(10, 5))
            plt.style.use('dark_background')
            plt.plot(df['created_at'], df['price'], label='BTC Price', color='#00ff00', linewidth=2)
            plt.plot(df['created_at'], df['avg_price'], label='SMA (C++ Calc)', color='#ff9900', linestyle='--')
            plt.title('Live Bitcoin Analytics')
            plt.legend()
            plt.xticks(rotation=45)
            plt.tight_layout()

            plt.savefig('/app/charts/live_chart.png')
            plt.close()
            print("Chart updated successfully.")
    except Exception as e:
        print(f"Waiting for data or DB: {e}")


if __name__ == "__main__":
    os.makedirs('/app/charts', exist_ok=True)
    engine = make_engine()
    while True:
        plot_crypto(engine)
        time.sleep(20)
