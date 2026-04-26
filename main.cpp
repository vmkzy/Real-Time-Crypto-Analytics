#include "crow_all.h"
#include <pqxx/pqxx>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

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

std::vector<std::string> load_tickers() {
    std::vector<std::string> out;
    std::string raw = env_var("TICKERS", "BTC/USDT");
    std::stringstream ss(raw);
    std::string item;
    while (std::getline(ss, item, ',')) {
        size_t start = item.find_first_not_of(" \t");
        if (start == std::string::npos)
            continue;
        size_t end = item.find_last_not_of(" \t");
        out.push_back(item.substr(start, end - start + 1));
    }
    if (out.empty())
        out.push_back("BTC/USDT");
    return out;
}

std::string chart_filename_for_symbol(std::string const& symbol) {
    std::string safe;
    safe.reserve(symbol.size());
    for (char c : symbol) {
        if (c == '/' || c == '\\')
            safe += '_';
        else
            safe += c;
    }
    return "live_" + safe + ".png";
}

bool safe_chart_basename(std::string const& name) {
    if (name.size() < 10)
        return false;
    if (name.rfind("live_", 0) != 0)
        return false;
    if (name.size() < 9 || name.substr(name.size() - 4) != ".png")
        return false;
    for (size_t i = 5; i < name.size() - 4; ++i) {
        unsigned char c = static_cast<unsigned char>(name[i]);
        if (!(std::isalnum(c) || name[i] == '_'))
            return false;
    }
    return true;
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
    html << "<!DOCTYPE html><html><head><meta charset='utf-8'><meta http-equiv='refresh' content='1'>"
            "<title>Live Crypto Dashboard</title>"
            "<style>"
            "body{font-family:sans-serif;background:#121212;color:#fff;text-align:center;padding-top:50px;}"
            "table{margin:auto;width:80%;border-collapse:collapse;background:#1e1e1e;"
            "box-shadow:0 4px 15px rgba(0,0,0,0.5);}"
            "th,td{padding:15px;border:1px solid #333;}"
            "th{background:#252525;color:#aaa;text-transform:uppercase;font-size:12px;}"
            ".topbar{display:flex;justify-content:center;gap:12px;flex-wrap:wrap;margin-bottom:24px;}"
            ".navlink{display:inline-block;padding:10px 16px;border-radius:999px;background:#2a2a2a;color:#fff;text-decoration:none;font-weight:bold;}"
            ".navlink:hover{background:#3a3a3a;}"
            "</style></head><body>"
            "<h1>Live Crypto Monitor</h1>"
            "<div class='topbar'><a class='navlink' href='/dashboard'>Table</a><a class='navlink' href='/charts-view'>Charts</a></div>"
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

crow::response charts_page() {
    std::ostringstream html;
    html << "<!DOCTYPE html><html><head><meta charset='utf-8'>"
            "<title>Live Crypto Charts</title>"
            "<style>"
            "body{font-family:sans-serif;background:#121212;color:#fff;margin:0;padding:32px 20px 64px;}"
            "h1{text-align:center;margin:0 0 10px;}"
            ".subtitle{text-align:center;color:#888;margin:0 0 24px;}"
            ".topbar{display:flex;justify-content:center;gap:12px;flex-wrap:wrap;margin:0 0 28px;}"
            ".navlink{display:inline-block;padding:10px 16px;border-radius:999px;background:#2a2a2a;color:#fff;text-decoration:none;font-weight:bold;}"
            ".navlink:hover{background:#3a3a3a;}"
            ".charts{display:flex;flex-wrap:wrap;justify-content:center;gap:24px;max-width:1200px;margin:0 auto;}"
            ".chart-card{background:#1a1a1a;padding:16px;border-radius:12px;box-shadow:0 2px 12px rgba(0,0,0,0.35);}"
            ".chart-img{max-width:min(480px,92vw);height:auto;border-radius:8px;display:block;margin-top:8px;}"
            "</style></head><body>"
            "<h1>Live Crypto Charts</h1>"
            "<p class='subtitle'>This page stays static so the charts are easier to inspect.</p>"
            "<div class='topbar'><a class='navlink' href='/dashboard'>Table</a><a class='navlink' href='/charts-view'>Charts</a></div>"
            "<div class='charts'>";

    for (std::string const& sym : load_tickers()) {
        std::string fname = chart_filename_for_symbol(sym);
        html << "<div class='chart-card'><p style='color:#cfcfcf;font-weight:bold;margin:0 0 4px;'>" << sym
             << "</p><img class='chart-img' src='/charts/" << fname << "' alt='" << sym << "' loading='lazy' /></div>";
    }

    html << "</div></body></html>";
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

    CROW_ROUTE(app, "/charts-view")([]() {
        return charts_page();
    });

    CROW_ROUTE(app, "/charts/<string>")([](std::string const& name) {
        if (!safe_chart_basename(name))
            return crow::response(404);
        std::string path = "/app/charts/" + name;
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs)
            return crow::response(404);
        std::string body((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        crow::response res(std::move(body));
        res.set_header("Content-Type", "image/png");
        res.set_header("Cache-Control", "no-cache");
        return res;
    });

    app.port(8080).multithreaded().run();
    return 0;
}
