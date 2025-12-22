#pragma once
#include <string>
#include <vector>
#include <sqlite3.h>



struct User {
    std::string name;
    int user_id;
};
struct Request {
    std::string request;
    std::string response;
    int uid;
    int user_id;
};
struct Context {
    std::string context;
    int uid;
    int user_id;
};

class UserDB {
private:
    sqlite3* db = nullptr;
    char* errMsg = nullptr;
public:
    ~UserDB();
    void open(const std::string& dbName);
    void createTables();
    User getUserByName(const std::string& name);
    Context getContextByUser(const User &user);
    bool addUser(User& user);
    bool addContext(const User &user, Context &context);
    bool addRequest(const User &user, Request &request);
    std::vector<Request> getAllRequestsOfUser(const User &user);
    bool deleteRequestsOfUser(const User &user);
    bool executeSQL(const std::string& sql);
};