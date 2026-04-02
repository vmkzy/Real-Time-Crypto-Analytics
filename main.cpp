#include "crow_all.h"
#include <pqxx/pqxx>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string env_var(const char* name, const char* fallback) {
    const char* v = std::getenv(name);
    if (v && *v)
        return std::string(v);
    return fallback ? std::string(fallback) : std::string();
}

std::string pg_conn_str() {
    std::string host = env_var("DB_HOST", "db");
    std::string dbname = env_var("POSTGRES_DB", "crypto_db");
    std::string user = env_var("POSTGRES_USER", "vmkzy");
    std::string password = env_var("POSTGRES_PASSWORD", nullptr);
    return "host=" + host + " port=5432 dbname=" + dbname + " user=" + user +
           " password=" + password;
}

double json_price(const crow::json::rvalue& j) {
    if (!j.has("price"))
        return 0.0;
    const auto& p = j["price"];
    try {
        return p.d();
    }
    catch (...) {
        return static_cast<double>(p.i());
    }
}

crow::response dashboard_page(pqxx::result const& res) {
    std::ostringstream html;
    html << "<!DOCTYPE html><html><head><meta charset='utf-8'><meta http-equiv='refresh' content='2'>"
            "<title>Live Crypto</title>"
            "<style>"
            "body{font-family:sans-serif;background:#121212;color:#fff;text-align:center;padding-top:50px;}"
            "table{margin:auto;width:80%;border-collapse:collapse;background:#1e1e1e;"
            "box-shadow:0 4px 15px rgba(0,0,0,0.5);}"
            "th,td{padding:15px;border:1px solid #333;}"
            "th{background:#252525;color:#aaa;text-transform:uppercase;font-size:12px;}"
            "</style></head><body>"
            "<h1>Live Crypto Monitor</h1>"
            "<table><tr><th>Ticker</th><th>Price</th><th>Average</th><th>Status</th></tr>";

    for (auto const& row : res) {
        std::string ticker = row[0].as<std::string>();
        double last_p = row[1].as<double>();
        double avg_p = row[2].as<double>();
        bool up = (last_p >= avg_p);
        const char* color = up ? "#00ff00" : "#ff4444";
        const char* status = up ? "UP" : "DOWN";

        html << "<tr><td style='font-weight:bold;'>" << ticker << "</td>"
             << "<td>$" << std::to_string(last_p).substr(0, 8) << "</td>"
             << "<td style='color:#888;'>$" << std::to_string(avg_p).substr(0, 8) << "</td>"
             << "<td style='color:" << color << ";font-weight:bold;'>" << status << "</td></tr>";
    }

    html << "</table>";
    if (res.empty())
        html << "<p style='color:#666;margin-top:20px;'>Waiting for data from ingestor...</p>";
    html << "</body></html>";
    return crow::response(html.str());
}

}

int main() {
    crow::SimpleApp app;

    CROW_ROUTE(app, "/update").methods(crow::HTTPMethod::Post)([](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body)
            return crow::response(400, "Invalid JSON");

        std::string ticker = body.has("ticker") ? std::string(body["ticker"].s()) : "UNKNOWN";
        double price = json_price(body);

        try {
            pqxx::connection c(pg_conn_str());
            pqxx::work w(c);

            pqxx::result r = w.exec_params(
                "SELECT COALESCE(AVG(price), $1) FROM ("
                "  SELECT price FROM crypto_history WHERE ticker = $2 "
                "  ORDER BY created_at DESC LIMIT 100"
                ") AS last_prices",
                price,
                ticker);

            double avg_val = r[0][0].as<double>();

            w.exec_params(
                "INSERT INTO crypto_history (ticker, price, avg_price) VALUES ($1, $2, $3)",
                ticker,
                price,
                avg_val);

            w.exec("DELETE FROM crypto_history WHERE created_at < NOW() - INTERVAL '1 hour'");
            w.commit();

            std::cout << "SUCCESS: " << ticker << " | Price: " << price << " | Avg: " << avg_val
                      << " saved.\n";
            return crow::response(200, "OK");
        }
        catch (const std::exception& e) {
            std::cerr << "DATABASE ERROR: " << e.what() << std::endl;
            return crow::response(500, e.what());
        }
    });

    CROW_ROUTE(app, "/history")([]() {
        try {
            pqxx::connection c(pg_conn_str());
            pqxx::nontransaction n(c);
            pqxx::result res = n.exec(
                "SELECT ticker, price, created_at FROM crypto_history "
                "ORDER BY created_at DESC LIMIT 10");

            crow::json::wvalue out;
            int i = 0;
            for (auto const& row : res) {
                out[i]["ticker"] = row[0].as<std::string>();
                out[i]["price"] = row[1].as<double>();
                out[i]["time"] = row[2].as<std::string>();
                ++i;
            }
            return crow::response(out);
        }
        catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
    });

    CROW_ROUTE(app, "/dashboard")([]() {
        try {
            pqxx::connection c(pg_conn_str());
            pqxx::nontransaction n(c);
            pqxx::result res = n.exec(
                "SELECT DISTINCT ON (ticker) ticker, price, avg_price "
                "FROM crypto_history "
                "ORDER BY ticker, created_at DESC");
            return dashboard_page(res);
        }
        catch (const std::exception& e) {
            return crow::response(500, std::string("DB Error: ") + e.what());
        }
    });

    app.port(8080).multithreaded().run();
    return 0;
}
