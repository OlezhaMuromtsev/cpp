#include "AiAgent.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <cstring>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

using nlohmann::json;

// --------- utils IO ----------
bool AiAgent::readWholeFile(const std::string& path, std::string& out, std::string* err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { if (err) *err = "Cannot open file: " + path; return false; }
    std::ostringstream ss; ss << f.rdbuf();
    out = std::move(ss).str();
    return true;
}

// --------- JSON loaders ----------
bool AiAgent::loadConfig(const std::string& path, std::string* err) {
    std::string s;
    if (!readWholeFile(path, s, err)) return false;
    try {
        auto j = json::parse(s);
        cfg_.type = j.at("type").get<std::string>();
        if (cfg_.type == "hosted") {
            cfg_.host   = j.at("host").get<std::string>();
            if (j.contains("port")) cfg_.port = j.at("port").get<std::string>();
            cfg_.api_key = j.at("api_key").get<std::string>();
            if (j.contains("history_path")) cfg_.history_path = j.at("history_path").get<std::string>();
            if (j.contains("max_saved_requests")) cfg_.max_requests = j.at("max_saved_requests").get<size_t>();
            if (j.contains("max_saved_bytes")) cfg_.max_history_bytes = j.at("max_saved_bytes").get<size_t>();
        } else if (cfg_.type == "local") {
            cfg_.host   = j.at("host").get<std::string>();
            if (j.contains("port")) cfg_.port = j.at("port").get<std::string>();
            cfg_.model = j.at("model").get<std::string>();
            if (j.contains("history_path")) cfg_.history_path = j.at("history_path").get<std::string>();
            if (j.contains("max_saved_requests")) cfg_.max_requests = j.at("max_saved_requests").get<size_t>();
            if (j.contains("max_saved_bytes")) cfg_.max_history_bytes = j.at("max_saved_bytes").get<size_t>();
            cfg_.max_tokens = j.at("max_tokens").get<size_t>();
            cfg_.temp = j.at("temperature").get<double>();
            cfg_.top_p = j.at("top_p").get<double>();
        } else {
            if (err) *err = "Wrong config type!";
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        if (err) *err = std::string("Config parse error: ") + e.what();
        return false;
    }
}

bool AiAgent::loadPrompt(const std::string& path, std::string* err) {
    std::string s;
    if (!readWholeFile(path, s, err)) return false;
    try {
        // Допускаем, что файл — либо строка JSON, либо объект с ключом "prompt"
        json j = json::parse(s);
        if (j.is_string()) {
            prompt_ = j.get<std::string>();
        } else if (j.is_object()) {
            prompt_ = j.at("prompt").get<std::string>();
        } else {
            if (err) *err = "Prompt JSON must be string or object with key 'prompt'";
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        if (err) *err = std::string("Prompt parse error: ") + e.what();
        return false;
    }
}

// ------- Простейший разбор JSON: ожидаем { "text": "<строка>" } -------
std::string AiAgent::extractTextFromJsonBody(const std::string& body) const{
    // Если вместе с HTTP-хедерами — отрежем их
    const auto p = body.find("\r\n\r\n");
    const std::string json_part = (p != std::string::npos) ? body.substr(p + 4) : body;
    if (cfg_.type == "hosted") {
        try {
            auto j = json::parse(json_part);
            return j.at("text").get<std::string>();
        } catch (...) {
            return {};
        }
    } else if (cfg_.type == "local") {
        try {
            auto j = json::parse(json_part);
            return j.at("choices").get<std::vector<json>>()[0].at("message").get<json>().at("content").get<std::string>();
        } catch (...) {
            return {};
        }
    }
    return {};
}


// -------- Низкоуровневый HTTPS POST на /api/generate --------
std::optional<std::string> AiAgent::httpsPostGenerate(
        const AiConfig& cfg, const std::vector<json>& jsonBody, std::string* err) const {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) { if (err) *err = "SSL_CTX_new failed"; return std::nullopt; }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { if (err) *err = "socket failed"; SSL_CTX_free(ctx); return std::nullopt; }

    struct addrinfo hints = {}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(cfg.host.c_str(), cfg.port.c_str(), &hints, &res) != 0) {
        if (err) *err = "getaddrinfo failed";
        close(sock); SSL_CTX_free(ctx); return std::nullopt;
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        if (err) *err = "connect failed";
        freeaddrinfo(res); close(sock); SSL_CTX_free(ctx); return std::nullopt;
    }
    freeaddrinfo(res);
    std::ostringstream req;
    std::string response;
    if (cfg.type == "hosted") {
        SSL* ssl = SSL_new(ctx);
        SSL_set_fd(ssl, sock);
        if (SSL_connect(ssl) <= 0) {
            if (err) *err = "SSL_connect failed";
            SSL_free(ssl); close(sock); SSL_CTX_free(ctx); return std::nullopt;
        }
        req << "POST /api/generate HTTP/1.1\r\n"
        << "Host: " << cfg.host << "\r\n"
        << "Content-Type: application/json\r\n"
        << "Connection: close\r\n";
        if (!cfg.api_key.value().empty()) req << "x-api-key: " << cfg.api_key.value() << "\r\n";
        req << "Content-Length: " << jsonBody.size() << "\r\n\r\n"
            << jsonBody;
            const std::string request_str = req.str();
        if (SSL_write(ssl, request_str.c_str(), (int)request_str.size()) <= 0) {
            if (err) *err = "SSL_write failed";
            SSL_free(ssl); close(sock); SSL_CTX_free(ctx); return std::nullopt;
        }
        char buf[4096];
        int bytes;
        while ((bytes = SSL_read(ssl, buf, sizeof(buf)-1)) > 0) {
            buf[bytes] = '\0';
            response += buf;
        }

        SSL_free(ssl);
        close(sock);
        SSL_CTX_free(ctx);
    } else if (cfg.type == "local") {
        json request_json;
        if (!cfg.model.value().empty()) {
            request_json["model"] = cfg.model.value();
        } else {
            request_json["model"] = "local-gguf"; // значение по умолчанию
        }
        
        request_json["messages"] = jsonBody;
        if (cfg.max_tokens.has_value()) {
            request_json["max_tokens"] = *cfg.max_tokens;
        }
        if (cfg.temp.has_value()) {
            request_json["temperature"] = *cfg.temp;
        }
        if (cfg.top_p.has_value()) {
            request_json["top_p"] = *cfg.top_p;
        }
        std::string request_body = request_json.dump();
        req << "POST /v1/chat/completions HTTP/1.1\r\n"
            << "Host: " << cfg.host << ":" << cfg.port << "\r\n"
            << "Content-Type: application/json\r\n"
            << "Connection: close\r\n"
            << "Content-Length: " << request_body.size() << "\r\n\r\n"
            << request_body;
        const std::string request_str = req.str();
        std::cout << request_str << std::endl;
        fflush(stdout);
        if (send(sock, request_str.c_str(), request_str.size(), 0) <= 0) {
            if (err) *err = "send failed";
            close(sock); SSL_CTX_free(ctx); return std::nullopt;
        }
        
        // Чтение через recv вместо SSL_read
        char buf[4096];
        int bytes;
        while ((bytes = recv(sock, buf, sizeof(buf)-1, 0)) > 0) {
            buf[bytes] = '\0';
            response += buf;
        }
        
        close(sock);
        SSL_CTX_free(ctx);
    }
    std::cout << response << std::endl;
    fflush(stdout);
    std::string text = extractTextFromJsonBody(response);
    if (text.empty()) {
        if (err) *err = "Cannot extract \"text\" from JSON response";
        return std::nullopt;
    }
    return text;
}

std::optional<std::string> AiAgent::ask(std::string* outErr) const {
    if (cfg_.host.empty()) {
        if (outErr) *outErr = "Host not configured";
        return std::nullopt;
    }
    if (cfg_.port.empty()) {
        if (outErr) *outErr = "Port not configured";
        return std::nullopt;
    }
    if (prompt_.empty()) {
        if (outErr) *outErr = "Prompt is empty (load it first)";
        return std::nullopt;
    }
    std::vector<json> payload;
    if (cfg_.type == "hosted") {
        payload = { {"prompt", prompt_} };
    } else if (cfg_.type == "local") {
        payload.push_back({
            {"role", "system"},
            {"content", prompt_}
        });
        payload.push_back({
            {"role", "user"},
            {"content", request_}
        });
    }

    return httpsPostGenerate(cfg_, payload, outErr);
}
