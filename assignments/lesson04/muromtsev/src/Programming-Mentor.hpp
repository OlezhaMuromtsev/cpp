#pragma once
#include <string>
#include <optional>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "AiAgent.h"
#include "../utils/DB.hpp"

using json = nlohmann::json;

class PM final : public AiAgent {

enum REQUEST_TYPE {
    GENERAL_QUESTION,
    CODE_CONSULTATION,
    CODE_DEBUGGING, 
    REQUEST_TYPE_DETERMINATION,
    UNKNOWN,
    COMPRESSION
};
const static std::unordered_map<std::string, REQUEST_TYPE> inner_converter_;

public:
void printInfo();
void openDB();
bool userIntroduction(std::string *err = nullptr);
void determineRequestType(const std::string &request, std::string *err  = nullptr);
std::string promptBuilder(const PM::REQUEST_TYPE &type, const std::string &request, std::string *err);
std::string getUserRequest(std::string *err  = nullptr);
void saveRequest(const std::string &request, const std::string &response);
private:
bool loadHistory(const std::string &name, std::string *err);
void compressHistory(std::string *err = nullptr);
bool shouldCompress(std::optional<PM::REQUEST_TYPE> nextType);
User user_;
UserDB db;
std::optional<PM::REQUEST_TYPE> last_type_ = std::nullopt;
};