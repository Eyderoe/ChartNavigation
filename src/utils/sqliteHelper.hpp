#ifndef CHARTNAVIGATION_SQLITEHELPER_HPP
#define CHARTNAVIGATION_SQLITEHELPER_HPP


#include <SQLiteCpp/SQLiteCpp.h>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>


namespace fs = std::filesystem;

using SQLiteVal = std::variant<std::monostate, int64_t, double, std::string, std::vector<std::byte>>; // sqlite 单元格类型
using SQLiteDict = std::map<std::string, SQLiteVal>; // sqlite 键值对
using SQLiteAim = std::vector<std::string>; // sqlite 获取目标
using SQLiteRow = std::vector<SQLiteVal>; // sqlite 一行
using SQLiteDictRow = std::map<std::string, SQLiteVal>; // sqlite 一行
using SQLiteRows = std::vector<SQLiteRow>; // sqlite 多行
using SQLiteDictRows = std::vector<SQLiteDictRow>; // sqlite 多行

constexpr auto null = std::monostate{};


class Database {
    static void bindValue (SQLite::Statement &query, int index, const SQLiteVal &val);
    static SQLiteRow readResult (const SQLite::Statement &query);
    static SQLiteDictRow readResult (const SQLite::Statement &query, const SQLiteAim &header);
    static std::string whereSentence (const SQLiteDict &condition);
    public:
        explicit Database (const fs::path &dbPath, bool foreignKey = true);
        [[nodiscard]] SQLiteRows getRecords (const std::string &tableName, const SQLiteAim &aim = {},
                                             const SQLiteDict &condition = {});
        [[nodiscard]] SQLiteDictRows getDictRecords (const std::string &tableName, const SQLiteAim &aim = {},
                                                     const SQLiteDict &condition = {});
        [[nodiscard]] SQLiteRows getRecords (const std::string &sql, const SQLiteRow &parameters = {}) const;
        void addRecords (const std::string &tableName, const SQLiteRows &rows, const SQLiteAim &rowsName = {});
        void changeRecords (const std::string &tableName, const SQLiteDict &values,
                            const SQLiteDict &condition = {}) const;
        void deleteRecords (const std::string &tableName, const SQLiteDict &condition = {}) const;
        void commit();
    private:
        std::unique_ptr<SQLite::Database> db; ///< 数据库
        std::unique_ptr<SQLite::Transaction> transaction;
        std::unordered_map<std::string, SQLiteAim> headers; ///< 每张表所有的列
        [[nodiscard]] const SQLiteAim& getHeader (const std::string &tableName);
        [[nodiscard]] SQLite::Statement get (const std::string &tableName, const SQLiteAim &aim,
                                             const SQLiteDict &condition);
};



#endif //CHARTNAVIGATION_SQLITEHELPER_HPP
