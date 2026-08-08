#include "affineTransformer.hpp"
#include "randomGen.hpp"

#include <algorithm>
#include <format>
#include <iostream>
#include <numbers>
#include <ranges>
#include "geographic.hpp"


std::vector<int> findAbnormal_RANSAC (std::vector<std::vector<double>> &values, double threshold);
constexpr int combination (int n);
std::vector<std::array<int, 3>> spawnByCombine (int num);
std::vector<std::array<int, 3>> spawnByRandom (int num);

/**
 * @brief 计算组合数 C(n,3)
 * @return 组合数
 */
constexpr int combination (const int n) {
    return n < 3 ? 0 : n * (n - 1) * (n - 2) / 6;
}

/**
 * @brief 返回组合数对应可能组合 C(num,3)
 * @param num 元素个数
 * @return 组合
 */
std::vector<std::array<int, 3>> spawnByCombine (const int num) {
    std::vector<std::array<int, 3>> results;
    results.reserve(combination(num));
    for (int i = 0; i < num - 2; ++i)
        for (int j = i + 1; j < num - 1; ++j)
            for (int k = j + 1; k < num; ++k)
                results.push_back({{i, j, k}});
    return results;
}

/**
 * @brief 随机生成200个组合
 * @param num 元素个数
 * @return 组合
 */
std::vector<std::array<int, 3>> spawnByRandom (const int num) {
    std::vector<std::array<int, 3>> combines;
    combines.reserve(200);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, num - 1);
    while (combines.size() < 200) {
        const int a = dist(gen);
        const int b = dist(gen);
        const int c = dist(gen);
        if (a == b || b == c || a == c)
            continue;
        combines.push_back({a, b, c});
    }
    return combines;
}

/**
 * @brief 基于RANSAC算法筛选异常值
 * @param values 原始值列表
 * @param threshold 异常阈值
 * @return 异常值位置列表
 */
std::vector<int> findAbnormal_RANSAC (std::vector<std::vector<double>> &values, const double threshold) {
    if (values.size() < 3)
        return {};
    const size_t n = values.size(); // 数据量
    size_t maxInnerCount = 0; // 最大内点数量
    std::set<int> bestInnerIdxes; // 最优内点索引
    // 获取可能组合
    std::vector<std::array<int, 3>> idxPairs;
    if (values.size() <= 14)
        idxPairs = spawnByCombine(static_cast<int>(values.size()));
    else
        idxPairs = spawnByRandom(static_cast<int>(values.size()));
    for (const auto &samples : idxPairs) {
        // 仿射变换
        auto view = samples | std::views::transform([&](const int j) -> std::vector<double>& { return values[j]; });
        auto [pX,pY] = doAffine(view);
        // 计算内点
        std::set<int> currentInnerIdxes;
        for (int j = 0; j < n; ++j) {
            const double lon = values[j][1], lat = values[j][0];
            const double xT = values[j][2], yT = values[j][3];
            const double xP = pX(0) * lon + pX(1) * lat + pX(2);
            const double yP = pY(0) * lon + pY(1) * lat + pY(2);
            if (std::pow(xP - xT, 2) + std::pow(yP - yT, 2) < std::pow(threshold, 2))
                currentInnerIdxes.insert(j);
        }
        // 更新最优
        if (currentInnerIdxes.size() > maxInnerCount) {
            maxInnerCount = currentInnerIdxes.size();
            bestInnerIdxes = currentInnerIdxes;
        }
        // 高覆盖率
        if (static_cast<double>(maxInnerCount) > static_cast<double>(n) * 0.9)
            break;
    }
    // 找出离群点索引
    std::vector<int> abnormalValues;
    for (int i = 0; i < n; ++i) {
        if (!bestInnerIdxes.contains(i))
            abnormalValues.push_back(i);
    }
    return abnormalValues;
}

/**
 * @brief 加载数据
 * @param dataList [[纬度,经度,x,y], ...]
 * @param threshold 离群阈值
 * @return 数据是否可用
 */
bool AffineTransformer::loadData (const std::vector<std::vector<double>> &dataList, const double threshold) {
    affine = false;
    data = dataList;
    // 第一次变换
    if (!fitAffine())
        return false;
    auto idxes = findAbnormal_RANSAC(data, threshold);
    // 第二次变换
    std::ranges::sort(idxes, std::ranges::greater{});
    for (const auto idx : idxes)
        data.erase(data.begin() + idx);
    affine = true;
    return fitAffine();
}

/**
 * @brief 转换经纬度至平面坐标系
 * @param latitude 纬度
 * @param longitude 经度
 * @return [x,y]
 */
std::pair<double, double> AffineTransformer::transform (const double latitude, const double longitude) {
    double x = paramsX(0) * longitude + paramsX(1) * latitude + paramsX(2);
    double y = paramsY(0) * longitude + paramsY(1) * latitude + paramsY(2);
    return {x, y};
}

/**
 * @brief 转换经纬度至平面坐标系
 * @param [latitude,longitude]
 * @return [x,y]
 */
std::pair<double, double> AffineTransformer::transform (const std::pair<double, double> &loc) {
    double x = paramsX(0) * loc.second + paramsX(1) * loc.first + paramsX(2);
    double y = paramsY(0) * loc.second + paramsY(1) * loc.first + paramsY(2);
    return {x, y};
}

/**
 * @brief 评估仿射变换效果
 * @param print 是否输出至控制台
 * @return 均方根误差,误差列表
 */
std::pair<double, std::vector<double>> AffineTransformer::accEvaluate (const bool print) {
    double totalError{}, sumSquaredError{}, maxError{};
    double minError = std::numeric_limits<double>::infinity();
    const int n = static_cast<int>(data.size());
    std::vector<double> errors;
    errors.reserve(n);
    for (const auto &row : data) {
        const double lat{row[0]}, lon{row[1]}, xTrue{row[2]}, yTrue{row[3]};
        auto [xPred, yPred] = transform(lat, lon);
        const double dx = xPred - xTrue;
        const double dy = yPred - yTrue;
        double error = std::sqrt(dx * dx + dy * dy);
        errors.emplace_back(error);
        totalError += error;
        sumSquaredError += dx * dx + dy * dy;
        maxError = std::max(maxError, error);
        minError = std::min(minError, error);
    }
    double meanError = totalError / n;
    double rmsError = std::sqrt(sumSquaredError / n);
    if (print) {
        std::cout << std::format("Mean error: {:.2f}", meanError) << std::endl;
        std::cout << std::format("RMS error: {:.2f}", rmsError) << std::endl;
        std::cout << std::format("error range: ({:.2f}, {:.2f})", minError, maxError) << std::endl;
    }
    return {rmsError, errors};
}

/**
 * @brief 计算仿射变换矩阵的奇异值来评估转换质量
 * @return 奇异值列表
 * @note 尺度不一致造成该方法不可用,储备代码(好的变换最大奇异除最小奇异约为一).不如实际构造正方形后转换
 */
std::vector<double> AffineTransformer::singularEvaluate () {
    Eigen::Matrix2d linearMatrix;
    linearMatrix << paramsX(0), paramsX(1), paramsY(0), paramsY(1);
    const Eigen::JacobiSVD<Eigen::Matrix2d> svd(linearMatrix);
    Eigen::Vector2d sv = svd.singularValues();
    return {sv(0), sv(1)};
}

/**
 * @brief 模拟正方形点来评估转换质量
 * @return 转换质量
 * @note 抛开Clang能有更好的写法
 */
AffineQuality AffineTransformer::squareEvaluate () {
    if (!affine)
        return AffineQuality::inop;
    auto latView = data | std::views::transform([](const auto &row) { return row[0]; });
    double lat = std::accumulate(latView.begin(), latView.end(), 0.0) / data.size();
    auto lonView = data | std::views::transform([](const auto &row) { return row[1]; });
    double lon = std::accumulate(lonView.begin(), lonView.end(), 0.0) / data.size();
    // 顺时针 A B C D
    Point2D a{lat, lon};
    Point2D b = pointBearingDistance(a, 180, 10);
    Point2D c = pointBearingDistance(b, 270, 10);
    Point2D d = pointBearingDistance(c, 360, 10);
    a = transform(a);
    b = transform(b);
    c = transform(c);
    d = transform(d);
    // 距离 边e 对角线 d
    const double e1 = distanceGeometry(a, b);
    const double e2 = distanceGeometry(b, c);
    const double e3 = distanceGeometry(c, d);
    const double e4 = distanceGeometry(d, a);
    const double d1 = distanceGeometry(a, c);
    const double d2 = distanceGeometry(b, d);
    // 评估分级
    constexpr double toleranceGood = 0.01, toleranceMid = 0.05; // 1% 5% 以内的误差
    const double avgE = (e1 + e2 + e3 + e4) / 4.0;
    const double varE = (pow(e1 - avgE, 2) + pow(e2 - avgE, 2) + pow(e3 - avgE, 2) + pow(e4 - avgE, 2)) / 4.0;
    const double sideError = sqrt(varE) / avgE;
    const double diagRatio1 = d1 / avgE;
    const double diagRatio2 = d2 / avgE;
    const double idealDiag = sqrt(2.0);
    const double diagError = (std::abs(diagRatio1 - idealDiag) + std::abs(diagRatio2 - idealDiag)) / 2.0;
    const double totalError = sideError + diagError;
    if (totalError < toleranceGood)
        return AffineQuality::good;
    else if (totalError < toleranceMid)
        return AffineQuality::fine;
    else
        return AffineQuality::bad;
}

/**
 * @brief 计算仿射变换参数
 * @return 数据是否可用
 */
bool AffineTransformer::fitAffine () {
    if (data.size() < 3)
        return false;
    auto [x,y] = doAffine(data);
    paramsX = x;
    paramsY = y;
    return true;
}
