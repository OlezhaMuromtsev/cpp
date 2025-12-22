#include "Programming-Mentor.hpp"
#include <iostream>

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
    std::cout << "<Mentor>: Здравствуйте! Я ваш персональный ментор по программированию." << '\n';
    std::cout << "<Mentor>: Я могу ответить на любой ваш вопрос по коду, а также проанализировать написанные вами программы и посоветовать, что можно в них улучшить." << '\n';
    std::cout << "<Mentor>: Для того, чтобы продолжить, введите ваше имя - возможно, мы с вами уже знакомы." << '\n';
}

void PM::openDB()
{
    db.open(cfg_.history_path);
    db.createTables();
}



bool PM::loadHistory(const std::string &name, std::string *err) 
{
    user_ = db.getUserByName(name);
    if (user_.user_id != -1) {
        return true;
    }
    db.addUser(user_);
    return false;
}

void PM::saveRequest(const std::string &request, const std::string &response)
{
    Request rqst;
    rqst.request = request;
    rqst.response = response;
    db.addRequest(user_, rqst);
    if (shouldCompress(std::nullopt)) compressHistory();
}

void PM::determineRequestType(const std::string &request, std::string *err)
{
    promptBuilder(PM::REQUEST_TYPE::REQUEST_TYPE_DETERMINATION, request, err);
    auto resp = ask(err);
    if (!resp || resp->empty()) {
        if (err && !err->empty()) std::cerr << "Request failed: " << *err << '\n';
        last_type_ = PM::REQUEST_TYPE::UNKNOWN;
        return;
    }
    std::string key = *resp;
    key.erase(0, key.find_first_not_of(" \t\n\r"));
    key.erase(key.find_last_not_of(" \t\n\r") + 1);
    auto it = inner_converter_.find(key);
    const auto type = (it != inner_converter_.end()) ? it->second : PM::REQUEST_TYPE::UNKNOWN;
    last_type_ = type;
    promptBuilder(type, request, err);
}

bool PM::userIntroduction(std::string *err)
{
    std::cout << "Введите имя:" << '\n';
    std::string name;
    while (!name.size() && !std::cin.eof()) std::cin >> name;
    if (std::cin.eof()) return false;
    std::cout << "<Mentor>: Здравствуйте, " << name << "!" << '\n';
    if (!loadHistory(name, err)) {
        std::cout << "<Mentor>: Приятно познакомиться. Какой у вас вопрос?" << '\n';
    } else {
        std::cout << "<Mentor>: Похоже мы с вами уже работали. Какой у вас вопрос?" << '\n';
    }
    return true;
}

std::string PM::getUserRequest(std::string *err)
{
    std::string line;
    std::string request;
    std::cout << "<" + user_.name + ">: ";
    std::cout.flush();
    while (std::getline(std::cin, line)) {
        if (line == "END") {
            return processFileReferences(request, err);
        }
        request += line + '\n';
    }
    return "";
}

std::string PM::processFileReferences(const std::string& request, std::string* err) {
    std::string result = request;
    size_t pos = 0;
    
    while ((pos = result.find("<", pos)) != std::string::npos) {
        size_t end_pos = result.find(">", pos);
        if (end_pos == std::string::npos) {
            break;
        }
        
        std::string filename = result.substr(pos + 1, end_pos - pos - 1);
        
        if (filename.find('.') != std::string::npos || 
            filename.find('/') != std::string::npos) {
            
            std::string file_content;
            if (readWholeFile(filename, file_content, err) && !file_content.empty()) {
                std::string replacement = "=== СОДЕРЖИМОЕ ФАЙЛА " + filename + " ===\n" + file_content + "\n=== КОНЕЦ ФАЙЛА " + filename + " ===";
                result.replace(pos, end_pos - pos + 1, replacement);
                pos += replacement.length();
            } else {
                pos = end_pos + 1;
            }
        } else {
            pos = end_pos + 1;
        }
    }
    
    return result;
}

std::string PM::promptBuilderHosted(const PM::REQUEST_TYPE &type, const std::string &request, std::string *err)
{
    auto history = db.getAllRequestsOfUser(user_);
    auto context = db.getContextByUser(user_).context;
    std::stringstream ss;
    if ((type != PM::REQUEST_TYPE::REQUEST_TYPE_DETERMINATION) &&
         type != PM::REQUEST_TYPE::COMPRESSION) {
        ss << "Ты — опытный ментор по программированию." << '\n';
        ss << "Твой ученик: " << user_.name << ". Общайся на 'вы'." << '\n';
        ss << "Не используй приветствия. Переходи сразу к делу." << '\n';

        if (context.size()) {
            ss << "КОНТЕКСТ ОБ УЧЕНИКЕ (из прошлых сессий):" << '\n';
            ss << context << '\n';
            ss << "---" << '\n';
        }
        if (history.size()) {
            ss << "ИСТОРИЯ ДИАЛОГА:" << '\n';
            for (const auto& req_pair : history) {
                ss << "---" << '\n';
                ss << "ВОПРОС: " << req_pair.request << "\n"
                   << "ОТВЕТ: " << req_pair.response << "\n";
            }
            ss << "---" << '\n';
        }
    }
    
    switch (type) {
        case PM::REQUEST_TYPE::REQUEST_TYPE_DETERMINATION: {
            ss << "Анализируй запрос пользователя. Определи тип. Ответь ОДНИМ словом." << '\n';
            ss << "ТИПЫ:" << '\n';
            ss << "1. general — общие вопросы (синтаксис, алгоритмы, утилиты) без привязки к конкретному коду" << '\n';
            ss << "2. consultation — улучшение существующего кода пользователя" << '\n';
            ss << "3. debug — исправление ошибок в работающей программе" << '\n';
            ss << "4. unknown — всё остальное" << '\n';
            ss << "ПРАВИЛО: Только одно слово: general, consultation, debug или unknown" << '\n';
            ss << "НЕТ пояснений. НЕТ кавычек." << '\n';
            ss << "ЗАПРОС:" << '\n';
            ss << request;
            break;
        }
        case PM::REQUEST_TYPE::GENERAL_QUESTION: {
            ss << "ОТВЕЧАЙ КОРОТКО И ЯСНО." << '\n';
            ss << "Инструкции:" << '\n';
            ss << "1. Сначала объясни теорию простыми словами" << '\n';
            ss << "2. Если нужно — добавь минимальный пример кода" << '\n';
            ss << "3. Избегай сложных терминов без объяснений" << '\n';
            ss << "4. Контекст выше используй только если критично необходимо" << '\n';
            ss << "ВОПРОС:" << '\n';
            ss << request;
            break;
        }
        case PM::REQUEST_TYPE::CODE_CONSULTATION: {
            ss << "АНАЛИЗ КОДА. ДАЙ РЕКОМЕНДАЦИИ." << '\n';
            ss << "Формат для каждой правки:" << '\n';
            ss << "1. [Строка X]: что не так" << '\n';
            ss << "2. Почему это важно" << '\n';
            ss << "3. Исправленный вариант" << '\n';
            ss << "ДОПОЛНИТЕЛЬНО:" << '\n';
            ss << "- Отметь повторяющиеся ошибки из контекста" << '\n';
            ss << "- Похвали за улучшения" << '\n';
            ss << "- Учти прошлые возражения ученика" << '\n';
            ss << "- Будь вежлив и конструктивен" << '\n';
            ss << "КОД ДЛЯ АНАЛИЗА:" << '\n';
            ss << request;
            break;
        }
        case PM::REQUEST_TYPE::CODE_DEBUGGING: {
            ss << "НАЙДИ И ИСПРАВЬ ОШИБКИ." << '\n';
            ss << "ПРИОРИТЕТЫ:" << '\n';
            ss << "1. Сначала синтаксические ошибки" << '\n';
            ss << "2. Затем логические ошибки" << '\n';
            ss << "ПРАВИЛА:" << '\n';
            ss << "- Меняй только проблемные места" << '\n';
            ss << "- Объясни причину каждой ошибки" << '\n';
            ss << "- Укажи на повторяющиеся ошибки из истории" << '\n';
            ss << "- Будь вежлив" << '\n';
            ss << "- Отвечай кратко, по делу" << '\n';
            ss << "КОД С ОШИБКАМИ:" << '\n';
            ss << request;
            break;
        }
        case PM::REQUEST_TYPE::UNKNOWN: {
            ss << "Анализируй запрос." << '\n';
            ss << "ЕСЛИ запрос о программировании или IT — помоги в рамках своей экспертизы" << '\n';
            ss << "ЕСЛИ запрос НЕ о программировании — вежливо откажись" << '\n';
            ss << "Фраза для отказа: 'Извините, я могу помочь только с вопросами программирования и Computer Science.'" << '\n';
            ss << "ЗАПРОС:" << '\n';
            ss << request;
            break;
        }
        case PM::REQUEST_TYPE::COMPRESSION: {
            ss << "Ты — система сжатия контекста." << '\n';
            ss << "СЖАТЬ диалог ментора и ученика." << '\n';
            ss << "СОХРАНИТЬ:" << '\n';
            ss << "1. Темы и алгоритмы (кратко)" << '\n';
            ss << "2. Рекомендации ментора" << '\n';
            ss << "   - Принятые учеником" << '\n';
            ss << "   - Отклонённые (и причина)" << '\n';
            ss << "3. Сильные стороны ученика" << '\n';
            ss << "4. Слабые стороны (что улучшить)" << '\n';
            ss << "5. Последняя задача: статус и проблемы" << '\n';
            ss << "ФОРМАТ: краткие пункты, только факты" << '\n';
            ss << "РАЗМЕР: не более " << cfg_.max_history.value_or(1024) / 2 << " символов" << '\n';
            ss << "КОНТЕКСТ ДЛЯ СЖАТИЯ:" << '\n';
            ss << "---" << '\n';
            if (context.size()) {
                ss << "КОНТЕКСТ УЧЕНИКА:" << '\n';
                ss << context << '\n';
                ss << "---" << '\n';
            }
            if (history.size()) {
                ss << "ИСТОРИЯ ДИАЛОГОВ:" << '\n';
                for (const auto &request : history) {
                    ss << "---" << '\n';
                    ss << "ВОПРОС: " << request.request << '\n';
                    ss << "ОТВЕТ: " << request.response << '\n';
                }
            }
            ss << "---" << '\n';
            break;
        }
        default: break;
    }
    if ((type != PM::REQUEST_TYPE::REQUEST_TYPE_DETERMINATION) &&
        (type != PM::REQUEST_TYPE::COMPRESSION)) {
        ss << "ТРЕБОВАНИЯ К ОТВЕТУ:" << '\n';
        ss << "- Только текст (без Markdown)" << '\n';
        ss << "- Без лишних знаков препинания" << '\n';
        ss << "- Максимум 1000 символов" << '\n';
        ss << "- Прямой и четкий ответ" << '\n';
    }
    prompt_ = ss.str();
    return prompt_;
}

std::string PM::promptBuilderLocal(const PM::REQUEST_TYPE &type, const std::string &request, std::string *err)
{
    auto history = db.getAllRequestsOfUser(user_);
    auto context = db.getContextByUser(user_).context;
    std::stringstream ss;
    if ((type != PM::REQUEST_TYPE::REQUEST_TYPE_DETERMINATION) &&
         type != PM::REQUEST_TYPE::COMPRESSION) {
        ss << "Ты — опытный ментор по программированию." << '\n';
        ss << "Твой ученик: " << user_.name << ". Общайся на 'вы'." << '\n';
        ss << "Не используй приветствия. Переходи сразу к делу." << '\n';

        if (context.size()) {
            ss << "КОНТЕКСТ ОБ УЧЕНИКЕ (из прошлых сессий):" << '\n';
            ss << context << '\n';
            ss << "---" << '\n';
        }
        if (history.size()) {
            for (const auto& req_pair : history) {
                history_.emplace_back(req_pair.request, req_pair.response);
            }
        }
    }
    
    switch (type) {
        case PM::REQUEST_TYPE::REQUEST_TYPE_DETERMINATION: {
            ss << "Анализируй запрос пользователя. Определи тип. Ответь ОДНИМ словом." << '\n';
            ss << "ТИПЫ:" << '\n';
            ss << "1. general — общие вопросы (синтаксис, алгоритмы, утилиты) без привязки к конкретному коду" << '\n';
            ss << "2. consultation — улучшение существующего кода пользователя" << '\n';
            ss << "3. debug — исправление ошибок в работающей программе" << '\n';
            ss << "4. unknown — всё остальное" << '\n';
            ss << "ПРАВИЛО: Только одно слово: general, consultation, debug или unknown" << '\n';
            ss << "НЕТ пояснений. НЕТ кавычек." << '\n';
            break;
        }
        case PM::REQUEST_TYPE::GENERAL_QUESTION: {
            ss << "ОТВЕЧАЙ КОРОТКО И ЯСНО." << '\n';
            ss << "Инструкции:" << '\n';
            ss << "1. Сначала объясни теорию простыми словами" << '\n';
            ss << "2. Если нужно — добавь минимальный пример кода" << '\n';
            ss << "3. Избегай сложных терминов без объяснений" << '\n';
            ss << "4. Контекст выше используй только если критично необходимо" << '\n';
            break;
        }
        case PM::REQUEST_TYPE::CODE_CONSULTATION: {
            ss << "АНАЛИЗ КОДА. ДАЙ РЕКОМЕНДАЦИИ." << '\n';
            ss << "Формат для каждой правки:" << '\n';
            ss << "1. [Строка X]: что не так" << '\n';
            ss << "2. Почему это важно" << '\n';
            ss << "3. Исправленный вариант" << '\n';
            ss << "ДОПОЛНИТЕЛЬНО:" << '\n';
            ss << "- Отметь повторяющиеся ошибки из контекста" << '\n';
            ss << "- Похвали за улучшения" << '\n';
            ss << "- Учти прошлые возражения ученика" << '\n';
            ss << "- Будь вежлив и конструктивен" << '\n';
            break;
        }
        case PM::REQUEST_TYPE::CODE_DEBUGGING: {
            ss << "НАЙДИ И ИСПРАВЬ ОШИБКИ." << '\n';
            ss << "ПРИОРИТЕТЫ:" << '\n';
            ss << "1. Сначала синтаксические ошибки" << '\n';
            ss << "2. Затем логические ошибки" << '\n';
            ss << "ПРАВИЛА:" << '\n';
            ss << "- Меняй только проблемные места" << '\n';
            ss << "- Объясни причину каждой ошибки" << '\n';
            ss << "- Укажи на повторяющиеся ошибки из истории" << '\n';
            ss << "- Будь вежлив" << '\n';
            ss << "- Отвечай кратко, по делу" << '\n';
            break;
        }
        case PM::REQUEST_TYPE::UNKNOWN: {
            ss << "Анализируй запрос." << '\n';
            ss << "ЕСЛИ запрос о программировании или IT — помоги в рамках своей экспертизы" << '\n';
            ss << "ЕСЛИ запрос НЕ о программировании — вежливо откажись" << '\n';
            ss << "Фраза для отказа: 'Извините, я могу помочь только с вопросами программирования и Computer Science.'" << '\n';
            break;
        }
        case PM::REQUEST_TYPE::COMPRESSION: {
            ss << "Ты — система сжатия контекста." << '\n';
            ss << "СЖАТЬ диалог ментора и ученика." << '\n';
            ss << "СОХРАНИТЬ:" << '\n';
            ss << "1. Темы и алгоритмы (кратко)" << '\n';
            ss << "2. Рекомендации ментора" << '\n';
            ss << "   - Принятые учеником" << '\n';
            ss << "   - Отклонённые (и причина)" << '\n';
            ss << "3. Сильные стороны ученика" << '\n';
            ss << "4. Слабые стороны (что улучшить)" << '\n';
            ss << "5. Последняя задача: статус и проблемы" << '\n';
            ss << "ФОРМАТ: краткие пункты, только факты" << '\n';
            ss << "РАЗМЕР: не более " << cfg_.max_history.value_or(1024) / 2 << " символов" << '\n';
            ss << "КОНТЕКСТ ДЛЯ СЖАТИЯ:" << '\n';
            ss << "---" << '\n';
            if (context.size()) {
                ss << "КОНТЕКСТ УЧЕНИКА:" << '\n';
                ss << context << '\n';
                ss << "---" << '\n';
            }
            if (history.size()) {
                for (const auto& req_pair : history) {
                    history_.emplace_back(req_pair.request, req_pair.response);
                }
            }
            break;
        }
        default: break;
    }
    if ((type != PM::REQUEST_TYPE::REQUEST_TYPE_DETERMINATION) &&
        (type != PM::REQUEST_TYPE::COMPRESSION)) {
        ss << "ТРЕБОВАНИЯ К ОТВЕТУ:" << '\n';
        ss << "- Только текст (без Markdown)" << '\n';
        ss << "- Без лишних знаков препинания" << '\n';
        ss << "- Максимум 1000 символов" << '\n';
        ss << "- Прямой и четкий ответ" << '\n';
    }
    prompt_ = ss.str();
    request_ = request;
    return prompt_;
}

void PM::compressHistory(std::string *err)
{
    std::cout << "<Mentor>: Происходит сжатие контекста..." << std::endl;
    auto history = db.getAllRequestsOfUser(user_);
    promptBuilder(PM::COMPRESSION, "Сожми предложенную тебе историю запросов и ответов и выведи", err);
    auto resp = ask(err);
    if (!resp || resp->empty()) {
        if (err && !err->empty()) std::cerr << "Request failed: " << *err << '\n';
        std::cout << "<Mentor>: Ошибка при сжатии контекста" << std::endl;
        return;
    }
    
    Context context = db.getContextByUser(user_);
    context.context = *resp;
    
    if (context.context.size()) {
        db.addContext(user_, context);
        if (history.size() > 1) {
            db.deleteRequestsOfUser(user_);
            if (!history.empty()) {
                db.addRequest(user_, history.back());
            }
        }
    }
    std::cout << "<Mentor>: Сжатие контекста завершено!" << std::endl;
}

bool PM::shouldCompress(const std::optional<PM::REQUEST_TYPE> &nextType) {
    const auto reqs = db.getAllRequestsOfUser(user_);
    if (reqs.size() >= cfg_.max_requests.value_or(10)) return true;
    size_t syms = 0;
    for (const auto &req : reqs) syms += (req.request.size() + req.response.size());
    if (syms >= cfg_.max_history.value_or(1024)) return true;
    if (last_type_.has_value() && nextType.has_value() 
        && last_type_.value() != nextType.value()
        && last_type_.value() != PM::REQUEST_TYPE::REQUEST_TYPE_DETERMINATION
        && nextType.value() != PM::REQUEST_TYPE::REQUEST_TYPE_DETERMINATION) {
        return true;
    }
    
    return false;
}