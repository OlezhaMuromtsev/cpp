#pragma once
#include <string>
#include <optional>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "AiAgent.h"

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
void userIntroduction(std::string *err = nullptr);
void determineRequestType(const std::string &request, std::string *err  = nullptr);
std::string promptBuilder(const PM::REQUEST_TYPE &type,  std::string *err  = nullptr);
std::string getUserRequest(std::string *err  = nullptr);
void saveSession();
void saveHistory(const std::optional<std::string> &answer, const std::string &request);
private:
bool loadHistory(std::string *err  = nullptr);
void compressHistory(std::string *err = nullptr);
bool shouldCompress(std::optional<PM::REQUEST_TYPE> nextType);
std::string username_;
json history_;
std::optional<PM::REQUEST_TYPE> last_type_ = std::nullopt;
};