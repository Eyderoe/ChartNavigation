#include "sqliteHelper.hpp"

#include <iostream>
#include <ranges>
#include <stdexcept>
#include "stringProcess.hpp"

using namespace std;


Database::Database (const fs::path &dbPath, const bool foreignKey) {
    db = make_unique<SQLite::Database>(dbPath, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
    if (foreignKey)
        db->exec("PRAGMA foreign_keys = ON;");
    transaction = make_unique<SQLite::Transaction>(*db);
}

/**
 * @brief 获取某张表所有列的列名
 * @param tableName 表名
 * @return 列名 vector<string>
 */
const SQLiteAim& Database::getHeader (const string &tableName) {
    // 旧表
    if (const auto it = headers.find(tableName); it != headers.end())
        return it->second;
    // 新表
    vector<string> columnHeaders;
    const string sentence = "PRAGMA table_info('" + tableName + "')";
    SQLite::Statement query(*db, sentence);
    while (query.executeStep())
        columnHeaders.push_back(query.getColumn(1).getString());
    columnHeaders.shrink_to_fit();
    return headers.emplace(tableName, std::move(columnHeaders)).first->second; // emplace返回pair<iterator, bool>
}

/**
 * @brief 构造 where 语句
 * @param condition 查询条件
 * @return 语句
 */
string Database::whereSentence (const SQLiteDict &condition) {
    if (condition.empty())
        return {};
    vector<string> dictKeys;
    dictKeys.reserve(condition.size());
    for (const auto &key : condition | views::keys)
        dictKeys.emplace_back(key + "=?");
    return "where " + join(dictKeys, " and ") + ';';
}

/**
 * @brief 构造 select 语句
 * @param tableName 表名
 * @param aim 目标列
 * @param condition 条件
 * @return query 语句
 */
SQLite::Statement Database::get (const std::string &tableName, const SQLiteAim &aim, const SQLiteDict &condition) {
    // 查询 select xxx, xxx from xxx where x=? and x=?
    const SQLiteAim header{getHeader(tableName)};
    const string select{"select " + join(aim.empty() ? getHeader(tableName) : aim, ",")};
    const string from{" from " + tableName};
    const string where{whereSentence(condition)};
    // 绑定
    SQLite::Statement query(*db, select + from + where);
    int index{1};
    for (const auto &val : condition | views::values)
        bindValue(query, index++, val);
    return query;
}

/**
 * @brief 查询数据
 * @param tableName 表名
 * @param aim 目标列, 空默认全部列
 * @param condition 查询条件
 * @return 结果 vector<vector<variant>>
 */
SQLiteRows Database::getRecords (const string &tableName, const SQLiteAim &aim, const SQLiteDict &condition) {
    auto query = get(tableName, aim, condition);
    // 拿结果
    SQLiteRows rows;
    while (query.executeStep())
        rows.emplace_back(readResult(query));
    return rows;
}

/**
 * @brief 查询数据
 * @param tableName 表名
 * @param aim 目标列, 空默认全部列
 * @param condition 查询条件
 * @return 结果 vector<map<string,variant>>
 */
SQLiteDictRows Database::getDictRecords (const std::string &tableName, const SQLiteAim &aim,
                                         const SQLiteDict &condition) {
    const SQLiteAim header = getHeader(tableName);
    const SQLiteAim names = aim.empty() ? header : aim;
    auto query = get(tableName, aim, condition);
    // 拿结果
    SQLiteDictRows rows;
    while (query.executeStep())
        rows.emplace_back(readResult(query, names));
    return rows;
}

/**
 * @brief 执行参数化的只读查询
 * @param sql SQL 语句
 * @param parameters 按问号出现顺序绑定的参数
 * @return 查询结果
 */
SQLiteRows Database::getRecords (const std::string &sql, const SQLiteRow &parameters) const {
    SQLite::Statement query(*db, sql);
    for (int index = 0; index < static_cast<int>(parameters.size()); ++index)
        bindValue(query, index + 1, parameters[index]);
    SQLiteRows rows;
    while (query.executeStep())
        rows.emplace_back(readResult(query));
    return rows;
}

/**
 * @brief 添加数据
 * @param tableName 表名
 * @param rows 数据行
 * @param rowsName 指定列顺序
 */
void Database::addRecords (const std::string &tableName, const SQLiteRows &rows, const SQLiteAim &rowsName) {
    const SQLiteAim header = rowsName.empty() ? getHeader(tableName) : rowsName;
    const string insert = "insert into " + tableName + " (" + join(header, ",") + ") ";
    const string value = "values (" + join(vector<string>(header.size(), "?"), ",") + ')';
    SQLite::Statement query(*db, insert + value);
    for (const auto &row : rows) {
        if (row.size() != header.size())
            throw std::invalid_argument("row size (" + std::to_string(row.size()) + ") != column count ("
                                        + std::to_string(header.size()) + ")");
        for (int bindIndex = 1; bindIndex <= row.size(); ++bindIndex)
            bindValue(query, bindIndex, row[bindIndex - 1]);
        query.exec();
    }
}

/**
 * @brief 删除数据
 * @param tableName 表名
 * @param condition 查询条件
 */
void Database::deleteRecords (const std::string &tableName, const SQLiteDict &condition) const {
    // delete from xxx where x=? and x=?
    const string del{"delete from " + tableName + ' '};
    const string where = whereSentence(condition);
    // 绑定
    SQLite::Statement query(*db, del + where);
    int index{1};
    for (const auto &val : condition | views::values)
        bindValue(query, index++, val);
    query.exec();
}

void Database::commit () {
    transaction->commit();
    transaction = make_unique<SQLite::Transaction>(*db.get());
}

/**
 * @brief 修改数据
 * @param tableName 表名
 * @param values 修改目标
 * @param condition 查询条件
 */
void Database::changeRecords (const std::string &tableName, const SQLiteDict &values,
                              const SQLiteDict &condition) const {
    // update xxx set x=?, x=? where x=? and x=?
    const string update{"update " + tableName + ' '};
    vector<string> keys;
    keys.reserve(values.size());
    for (const auto &key : values | views::keys)
        keys.emplace_back(key + "=?");
    const string set{"set " + join(keys, ",") + ' '};
    const string where{whereSentence(condition)};
    // 绑定
    SQLite::Statement query(*db, update + set + where);
    int index{1};
    for (const auto &val : values | views::values)
        bindValue(query, index++, val);
    for (const auto &val : condition | views::values)
        bindValue(query, index++, val);
    query.exec();
}

/**
 * 绑定参数到语句
 * @param query 语句
 * @param index 索引
 * @param val 绑定值
 */
void Database::bindValue (SQLite::Statement &query, const int index, const SQLiteVal &val) {
    visit([&]<typename T>(T &&arg) constexpr {
        using decayed_t = std::decay_t<T>;
        if constexpr (is_same_v<decayed_t, int64_t> || is_same_v<decayed_t, double> || is_same_v<
            decayed_t, std::string>) {
            query.bind(index, arg);
        } else if constexpr (is_same_v<decayed_t, std::vector<byte>>) {
            query.bind(index, arg.data(), static_cast<int>(arg.size()));
        } else if constexpr (is_same_v<decayed_t, std::monostate>) {
            query.bind(index);
        } else {
            std::cerr << "Unknown type: " << typeid(decayed_t).name() << std::endl;
        }
    }, val);
}

/**
 * @brief 从语句中读取一行值
 * @param query 语句
 * @return 一行值
 */
SQLiteRow Database::readResult (const SQLite::Statement &query) {
    const int colCount = query.getColumnCount();
    SQLiteRow row;
    row.reserve(colCount);
    for (int i = 0; i < colCount; ++i) {
        const auto &col = query.getColumn(i);

        if (col.isInteger()) {
            row.emplace_back(col.getInt64());
        } else if (col.isFloat()) {
            row.emplace_back(col.getDouble());
        } else if (col.isText()) {
            row.emplace_back(col.getString());
        } else if (col.isBlob()) {
            const int blobSize = col.getBytes();
            const byte *blobData = static_cast<const byte*>(col.getBlob());
            row.emplace_back(std::vector<byte>(blobData, blobData + blobSize));
        } else {
            row.emplace_back(null);
        }
    }
    return row;
}

/**
 * @brief 从语句中读取一行值
 * @param query 语句
 * @param header 列名
 * @return 一行值
 */
SQLiteDictRow Database::readResult (const SQLite::Statement &query, const SQLiteAim &header) {
    const int colCount = query.getColumnCount();
    SQLiteDictRow row;
    for (int i = 0; i < colCount; ++i) {
        const auto &col = query.getColumn(i);

        if (col.isInteger()) {
            row[header[i]] = col.getInt64();
        } else if (col.isFloat()) {
            row[header[i]] = col.getDouble();
        } else if (col.isText()) {
            row[header[i]] = col.getString();
        } else if (col.isBlob()) {
            const int blobSize = col.getBytes();
            const byte *blobData = static_cast<const byte*>(col.getBlob());
            row[header[i]] = std::vector<byte>(blobData, blobData + blobSize);
        } else {
            row[header[i]] = null;
        }
    }
    return row;
}
