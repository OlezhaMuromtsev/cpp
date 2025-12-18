#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

using nlohmann::json;

struct AiConfig {
    std::string type;
    std::string host;
    std::optional<std::string> model = std::nullopt;
    std::string port = "443";
    std::optional<std::string> api_key = std::nullopt;
    std::optional<std::string> history_path = std::nullopt;
    std::optional<size_t> max_requests = std::nullopt;
    std::optional<size_t> max_history_bytes = std::nullopt;
    std::optional<size_t> max_tokens = std::nullopt;
    std::optional<double> temp = std::nullopt;
    std::optional<double> top_p = std::nullopt;
};

class AiAgent {
public:
    // Загрузить конфиг (host, port, api_key) из JSON-файла
    bool loadConfig(const std::string& path, std::string* err = nullptr);

    // Загрузить промпт из JSON-файла (принимает либо строку, либо объект с ключом "prompt")
    bool loadPrompt(const std::string& path, std::string* err = nullptr);

    // Выполнить запрос и вернуть распарсенный "text" из ответа
    // Возвращает std::nullopt при ошибке (описание в outErr, если передан)
    std::optional<std::string> ask(std::string* outErr = nullptr) const;

    // Явно задать промпт программно (не из файла)
    void setPrompt(std::string p) { prompt_ = std::move(p); }


protected:
    // ---- низкоуровневые помощники ----
    std::optional<std::string> httpsPostGenerate(
        const AiConfig& cfg, const std::vector<json>& jsonBody, std::string* err) const;

    // Простой разбор JSON: ожидаем { "text": "<строка>" }
    std::string extractTextFromJsonBody(const std::string& body) const;
    static bool readWholeFile(const std::string& path, std::string& out, std::string* err);
    AiConfig cfg_;
    std::string prompt_;
    std::string request_;
};
