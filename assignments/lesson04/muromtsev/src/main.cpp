#include "Programming-Mentor.hpp"
#include <iostream>

int main(int argc, char **argv) {
    PM agent;
    std::string err;
    if (argc > 1) {
        err.clear();
        if (!agent.loadConfig(argv[1], &err)) {
            std::cerr << "Config error: " << err << "\n";
            return 1;
        }
    } else {
        err.clear();
        if (!agent.loadConfig("../config.json", &err)) {
            std::cerr << "Config error: " << err << "\n";
            return 1;
        }
    }
    agent.openDB();
    agent.printInfo();
    agent.userIntroduction(&err);
    std::cout << "<Mentor>: Жду ваш запрос... Введите 'END' для окончания ввода" << std::endl;
    std::string request = agent.getUserRequest(&err);
    while (request.size()) {
        std::cout << "<Mentor>: Обрабатываю ваш запрос..." << std::endl;
        agent.determineRequestType(request, &err);
        auto resp = agent.ask(&err);
        if (!resp) {
            std::cerr << "Request failed: " << err << "\n";
            return 2;
        }
        std::cout << *resp << "\n";
        agent.saveRequest(request, *resp);
        std::cout << "<Mentor>: Жду ваш запрос... Введите 'END' для окончания ввода" << std::endl;
        request = agent.getUserRequest(&err);
    }
    return 0;
}
