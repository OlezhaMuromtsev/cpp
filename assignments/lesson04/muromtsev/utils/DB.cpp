#include <iostream>
#include <string>
#include <vector>
#include <sqlite3.h>
#include <iomanip>
#include "DB.hpp"


UserDB::~UserDB() {
    if (db) {
        sqlite3_close(db);
    }
}

void UserDB::open(const std::string& dbName)
{
    int rc = sqlite3_open(dbName.c_str(), &db);
    
    if (rc != SQLITE_OK) {
        std::cerr << "Can not open database with history: " << sqlite3_errmsg(db) << std::endl;
        db = nullptr;
    }
}

void UserDB::createTables()
{
    const char* sql_create = R"(
        CREATE TABLE IF NOT EXISTS users (
            user_id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE
        );
        
        CREATE TABLE IF NOT EXISTS requests (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            request TEXT NOT NULL,
            response TEXT NOT NULL,
            FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE
        );
        
        CREATE TABLE IF NOT EXISTS context (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL UNIQUE,
            context TEXT NOT NULL,
            FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE
        );
    )";
    
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql_create, nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        std::cerr << "Create tables error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }
}


User UserDB::getUserByName(const std::string& name)
{
    User user;
    user.name = name;
    user.user_id = -1;
    
    const char* sql = "SELECT user_id FROM users WHERE name = ?;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            user.user_id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    
    return user;
}

bool UserDB::addUser(User& user)
{
    const std::string sql = "INSERT INTO users (name) VALUES (?);";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        std::cerr << "Error with SQL in addUser: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, user.name.c_str(), -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    bool success = (rc == SQLITE_DONE);
    
    if (success) {
        user.user_id = sqlite3_last_insert_rowid(db);
    } else {
        std::cerr << "Error in addUser: " << sqlite3_errmsg(db) << std::endl;
    }
    
    sqlite3_finalize(stmt);
    return success;
}

bool UserDB::addContext(const User &user, Context &context)
{
    const std::string sql = R"(
        INSERT OR REPLACE INTO context (user_id, context) 
        VALUES (?, ?);
    )";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        std::cerr << "Error with SQL in addContext: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    sqlite3_bind_int(stmt, 1, user.user_id);
    sqlite3_bind_text(stmt, 2, context.context.c_str(), -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    bool success = (rc == SQLITE_DONE);
    
    if (success) {
        context.uid = sqlite3_last_insert_rowid(db);
        context.user_id = user.user_id;
    } else {
        std::cerr << "Error in addContext: " << sqlite3_errmsg(db) << std::endl;
    }
    
    sqlite3_finalize(stmt);
    return success;
}

bool UserDB::addRequest(const User &user, Request &request)
{
    const std::string sql = R"(
        INSERT INTO requests (user_id, request, response) 
        VALUES (?, ?, ?);
    )";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        std::cerr << "Error with SQL in addRequest: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    sqlite3_bind_int(stmt, 1, user.user_id);
    sqlite3_bind_text(stmt, 2, request.request.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, request.response.c_str(), -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    bool success = (rc == SQLITE_DONE);
    
    if (success) {
        request.uid = sqlite3_last_insert_rowid(db);
        request.user_id = user.user_id;
    } else {
        std::cerr << "Error in addRequest: " << sqlite3_errmsg(db) << std::endl;
    }
    
    sqlite3_finalize(stmt);
    return success;
}
    
std::vector<Request> UserDB::getAllRequestsOfUser(const User &user)
{
    std::vector<Request> requests;
    const char* sql = "SELECT * FROM requests WHERE user_id = (?);";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        std::cerr << "Error with SQL in GetAllRequests: " << sqlite3_errmsg(db) << std::endl;
        return requests;
    }
    sqlite3_bind_int(stmt, 1, user.user_id);
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        Request request;
        request.uid = sqlite3_column_int(stmt, 0);
        request.user_id = sqlite3_column_int(stmt, 1);
        request.request = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        request.response = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        requests.push_back(request);
    }
    
    sqlite3_finalize(stmt);
    return requests;
}

Context UserDB::getContextByUser(const User &user)
{
    Context context;
    const char* sql = "SELECT * FROM context WHERE user_id = (?);";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        std::cerr << "Error with SQL in getContextByUser: " << sqlite3_errmsg(db) << std::endl;
        context.uid = -1;
        context.user_id = -1;
        return context;
    }
    sqlite3_bind_int(stmt, 1, user.user_id);
    if ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        context.uid = sqlite3_column_int(stmt, 0);
        context.user_id = sqlite3_column_int(stmt, 1);
        context.context = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    } else {
        context.uid = -1;
        context.user_id = -1;
    }
    sqlite3_finalize(stmt);
    return context;
}

bool UserDB::executeSQL(const std::string& sql)
{
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        std::cerr << "SQL procede Error in ExecuteSQL: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    
    return true;
}

bool UserDB::deleteRequestsOfUser(const User &user)
{
    const std::string sql = "DELETE FROM requests WHERE user_id = ?;";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        std::cerr << "Error with SQL in deleteRequestsOfUser: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, user.user_id);
    
    rc = sqlite3_step(stmt);
    bool success = (rc == SQLITE_DONE);
    
    if (!success) {
        std::cerr << "Error in deleteRequestsOfUser: " << sqlite3_errmsg(db) << std::endl;
    }
    
    sqlite3_finalize(stmt);
    return success;
}