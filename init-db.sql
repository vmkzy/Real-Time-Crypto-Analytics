CREATE TABLE IF NOT EXISTS crypto_history (
    id SERIAL PRIMARY KEY,
    ticker TEXT NOT NULL,
    price DOUBLE PRECISION NOT NULL,
    avg_price DOUBLE PRECISION NOT NULL,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_crypto_history_ticker_created
    ON crypto_history (ticker, created_at DESC);
