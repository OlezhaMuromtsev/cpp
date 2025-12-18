#include "Programming-Mentor.hpp"
#include <iostream>
#include <fstream>

const std::unordered_map<std::string, PM::REQUEST_TYPE> PM::inner_converter_ = {
    {"general", PM::REQUEST_TYPE::GENERAL_QUESTION},
    {"consultation", PM::REQUEST_TYPE::CODE_CONSULTATION},
    {"debug", PM::REQUEST_TYPE::CODE_DEBUGGING},
    {"request_type_determination", PM::REQUEST_TYPE::REQUEST_TYPE_DETERMINATION},
    {"unknown", PM::REQUEST_TYPE::UNKNOWN},
    {"compression", PM::REQUEST_TYPE::COMPRESSION}
};

void PM::printInfo()
{
    std::cout << "Здравствуйте! Я ваш персональный ментор по программированию." << '\n';
    std::cout << "Я могу ответить на любой ваш вопрос по коду, а также проанализировать написанные вами программы и посоветовать, что можно в них улучшить." << '\n';
    std::cout << "Для того, чтобы продолжить, введите ваше имя - возможно, мы с вами уже знакомы." << '\n';
}
bool PM::loadHistory(std::string *err) 
{
    if (!cfg_.history_path.has_value()) return false;
    std::string s;
    const std::string full_path = cfg_.history_path.value() + "/" + username_ + ".json";
    if (!readWholeFile(full_path, s, err)) {
        std::ofstream histfile {full_path};
        histfile << R"({ "requests": [], "context": ""})";
        histfile.close();
        history_ = json::parse(R"({ "requests": [], "context": ""})");
        return false;
    }
    try {
        history_ = json::parse(s);
        return true;
    } catch (const std::exception& e) {
        if (err) *err = std::string("History parse error: ") + e.what();
        history_ = json::parse(R"({ "requests": [], "context": ""})");
        return false;
    }
    return false;
}

void PM::determineRequestType(const std::string &request, std::string *err)
{
    promptBuilder(PM::REQUEST_TYPE::REQUEST_TYPE_DETERMINATION, err);
    request_ = request;
    auto resp = ask(err);
    if (!resp || resp->empty()) {
        if (err && !err->empty()) std::cerr << "Request failed: " << *err << '\n';
        return;
    }
    std::string key = *resp;
    auto it = inner_converter_.find(key);
    const auto type = (it != inner_converter_.end()) ? it->second : PM::REQUEST_TYPE::UNKNOWN;
    if (shouldCompress(type)) compressHistory(err);
    last_type_ = type;
    promptBuilder(type, err);
}

void PM::userIntroduction(std::string *err)
{
    std::cout << "Введите имя:" << '\n';
    while (!username_.size()) std::cin >> username_;
    std::cout << "Здравствуйте, " << username_ << "!" << '\n';
    if (!loadHistory(err)) {
        std::cout << "Приятно познакомиться. Какой у вас вопрос?" << '\n';
    } else {
        std::cout << "Похоже мы с вами уже работали. Какой у вас вопрос?" << '\n';
    }
}

std::string PM::getUserRequest(std::string *err)
{
    std::string line;
    std::string request;
    while (std::getline(std::cin, line)) {
        if (line == "END") {
            return request;
        }
        request += line + '\n';
    }
    return "";
}

std::string PM::promptBuilder(const PM::REQUEST_TYPE &type,  std::string *err)
{
    std::stringstream ss;
    if ((type != PM::REQUEST_TYPE::REQUEST_TYPE_DETERMINATION) &&
         type != PM::REQUEST_TYPE::COMPRESSION) {
        ss << "Ты — ментор по программированию." << '\n';
        ss << "Ты общаешься с пользователем по имени " << username_ << '\n';
        ss << "Не здоровайся с пользователем, обращайся к нему на вы." << '\n';
        auto history = history_.at("requests").get<std::vector<std::vector<std::string>>>();
        auto context = history_.at("context").get<std::string>();
        if (context.size()) {
            ss << "Вот тот контекст информации о пользователе, что ты собрал на основе прошлого опыта общения с ним:" << '\n';
            ss << context;
            ss << "----------------------" << '\n';
        }
        if (history.size()) {
            ss << "Вот предыдущие сохранённые запросы пользователя и твои ответы:" << '\n';
            for (const auto& req_pair : history) {
                ss << "----------------------\n";
                ss << "Запрос пользователя: " << req_pair[0] << "\n"
                   << "Твой ответ на него: " << req_pair[1] << "\n";
            }
        }
        ss << "----------------------" << '\n';
    }
    switch (type) {
        case PM::REQUEST_TYPE::REQUEST_TYPE_DETERMINATION: {
            ss << "Ты - модуль ИИ-агента, определяющий тип запроса пользователя" << '\n';
            ss << "По полученному от пользователя запросу ты должен определить его тип и выдать в ответе только одно кодовое слово, соотвествующее типу запроса." << '\n';
            ss << "Существуют следующие типы запросов:" << '\n';
            ss << "Общие вопросы - вопросы по синтаксису языка программирования, алгоритмам, работе с компьютером и утилитами - все вопросы без привязки к уже написанной программе. Кодовое слово для данного типа - general" << '\n';
            ss << "Консультация по уже написанному коду - вопросы, связанные с улучшением работы уже написанной пользователем программы. Кодовое слово для данного типа - consultation" << '\n';
            ss << "Отладка уже написанной программы - вопросы касательно некорректно работающих или не работающих совсем частей программы пользователя. Кодовое слово для данного типа - debug" << '\n';
            ss << "Вопросы, не относяющие к типам, описанным выше - для них кодовое слово -  unknown" << '\n';
            ss << "В качестве ответа дай одно слово - кодовое слово, соответствующее типу запроса пользователя." << '\n';
            break;
        }
        case PM::REQUEST_TYPE::GENERAL_QUESTION: {
            ss << "Отвечай на следующий вопрос пользователя коротко." << '\n';
            ss << "В ответе приводи небольшие листинги с кодом - примерами использования интересных пользователю конструкций и алгоритмов." << '\n';
            ss << "Если есть возможность, постарайся объяснить тему пользователю без написания кода." << '\n';
            ss << "Данный выше контекст диалога с пользователем используй минимально - пользователю должно быть всё понятно и без него." << '\n';
            break;
        }
        case PM::REQUEST_TYPE::CODE_CONSULTATION: {
            ss << "Тебе необходимо будет проанализировать код пользователя и дать ему советы по его улучшению" << '\n';
            ss << "Обращай внимание пользователя на его ошибки и недоработки, следующим образом: укажи строчку кода, напиши конструкцию, которую нужно доработать, затем объясни, зачем необходимо данное исправление и приведи свой уже исправленный вариант" << '\n';
            ss << "Используя данный тебе выше контекст, укажи пользователю на те ошибки, которые он продолжает допускать даже после твоих замечаний, а также похвали его за те конструкции и приемы программирования, которые начали у него лучше получаться по сравнению с прошлыми его запросами" << '\n';
            ss << "При указании на ошибки пользователя, будь предельно вежлив" << '\n';
            ss << "Если в контексте выше пользователь объяснил, почему он не может принять твои исправления и замечания, прислушайся к нему" << '\n';
            break;
        }
        case PM::REQUEST_TYPE::CODE_DEBUGGING: {
            ss << "Тебе нужно произвести отладку кода пользователя" << '\n';
            ss << "Если ошибка синтаксическая - приведи вариант её устранения и объясни, зачем это нужно" << '\n';
            ss << "Старайся не переписывать значительно код пользователя - меняй только те места, что необходимо" << '\n';
            ss << "Если ошибка логическая - например, в логике работы алгоритма, исправь её и объясни, почему раньше программа могла работать некорректно" << '\n';
            ss << "На основе данного тебе выше контекста подметь, в каких местах пользователь до этого уже совершал схожие ошибки и укажи пользователю на это" << '\n';
            ss << "При указании на ошибки пользователя, будь предельно вежлив" << '\n';
            ss << "Отвечай коротко и максимально по существу" << '\n';
            break;
        }
        case PM::REQUEST_TYPE::UNKNOWN: {
            ss << "Запрос пользователя не является стандартным, действуй по ситуации." << '\n';
            ss << "Если вопрос не касается программирования и Computer Science, вежливо скажи пользователю, что не можешь ему помочь." << '\n';
            break;
        }
        case PM::REQUEST_TYPE::COMPRESSION: {
            ss << "Ты — модуль, ответственный за сжатие контекста для ИИ-агента, консультирующего программистов." << '\n';
            ss << "Ты получишь контекст и историю общения ИИ агента с пользователем." << '\n';
            ss << "Твоя задача - сжать полученную информацию, при этом сохранив следующее:" << '\n';
            ss << "1) Какие задачи, алгоритмы и темы ИИ-агент обсуждал с пользователем - пиши об этом максимально коротко" << '\n';
            ss << "2) Какие рекомендации ИИ-агент давал пользователю, отдельно укажи, к каким пользователь прислушался, а какие - проигнорировал и почему" << '\n';
            ss << "3) Что у пользователя получается делать на данный момент хорошо и в чём он разбирается" << '\n';
            ss << "4) В чём, по мнению, ИИ-агента пользователь слабоват и на какие моменты ему стоит обратить внимание" << '\n';
            ss << "5) Какую последнюю задачу ИИ-агент решал с пользователем, была ли она решена и если нет, то какие проблемы остались" << '\n';
            ss << "Ты пишешь системную информацию, поэтому отвечай по существу" << '\n';
            ss << "Ниже приведён контекст, который тебе необходимо сжать:" << '\n';
            ss << "----------------------" << '\n';
            auto history = history_.at("requests").get<std::vector<std::vector<std::string>>>();
            auto context = history_.at("context").get<std::string>();
            if (context.size()) {
                ss << "Вот тот контекст информации о пользователе, что ты собрал на основе прошлого опыта общения с ним:" << '\n';
                ss << context;
                ss << "----------------------" << '\n';
            }
            if (history.size()) {
                ss << "Вот предыдущие сохранённые запросы пользователя и твои ответы:" << '\n';
                for (const auto &request : history) {
                    ss << "----------------------" << '\n';
                    ss << "Запрос пользователя: " << request[0] << '\n' << "Твой ответ на него: " << request[1] << '\n';
                }
            }
            ss << "----------------------" << '\n';
            break;
        }
        default: break;
    }
    prompt_ = ss.str();
    return prompt_;
}

void PM::saveSession()
{
    if (!cfg_.history_path) return;
    const std::string full_path = *cfg_.history_path + "/" + username_ + ".json";
    std::ofstream histfile(full_path, std::ios::trunc);
    if (!histfile) {
        std::cerr << "Failed to open history file: " << full_path << '\n';
        return;
    }
    histfile << history_.dump(4);
}

void PM::saveHistory(const std::optional<std::string>& answer, const std::string &request)
{
    history_["requests"].push_back({request, answer.value_or("")});
    if (shouldCompress(std::nullopt)) compressHistory(nullptr);
}

void PM::compressHistory(std::string *err)
{
    promptBuilder(PM::inner_converter_.at("compression"), err);
    auto resp = ask(err);
    if (!resp || resp->empty()) {
        if (err && !err->empty()) std::cerr << "Request failed: " << *err << '\n';
        return;
    }
    std::string compressed_history = *resp;
    if (compressed_history.size()) {
        history_["context"] = compressed_history;
        history_["requests"] = std::vector<std::vector<std::string>>();
        saveSession();
    }
}

bool PM::shouldCompress(std::optional<PM::REQUEST_TYPE> nextType) {
    const auto reqs = history_.at("requests").get<std::vector<std::vector<std::string>>>();
    if (reqs.size() >= cfg_.max_requests.value_or(20)) return true;
    const size_t bytes = history_.dump(0).size();
    if (bytes >= cfg_.max_history_bytes.value_or(256 * 1024)) return true;
    if (last_type_.has_value() && nextType.has_value() && last_type_.value() != nextType.value()) return true;
    return false;
}