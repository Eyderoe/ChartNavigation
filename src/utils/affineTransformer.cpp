#include "affineTransformer.hpp"
#include "tools/randomGen.hpp"

#include <algorithm>
#include <format>
#include <iostream>
#include <ranges>


std::vector<int> findAbnormal_RANSAC (std::vector<std::vector<double>> &values, double threshold);


/**
 * @brief 基于RANSAC算法筛选异常值
 * @param values 原始值列表
 * @param threshold 异常阈值
 * @return 异常值位置列表
 * @note 迭代次数增加意义较小,且在release下耗时较少
 */
std::vector<int> findAbnormal_RANSAC (std::vector<std::vector<double>> &values, const double threshold) {
    const size_t n = values.size(); // 数据量
    constexpr int iterations = 200; // 迭代次数
    size_t maxInnerCount = 0; // 最大内点数量
    std::set<int> bestInnerIdxes; // 最优内点索引
    // 迭代循环
    for (int i = 0; i < iterations; ++i) {
        // 随机选择点作为样本
        std::set<int> samples;
        while (samples.size() < 3)
            samples.insert(spawnInt(0, static_cast<int>(n) - 1));
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
        // 覆盖95%
        if (static_cast<double>(maxInnerCount) > static_cast<double>(n) * 0.95)
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
 * @param threshold
 * @return 数据是否可用
 */
bool AffineTransformer::loadData (const std::vector<std::vector<double>> &dataList, double threshold) {
    data = dataList;
    // 第一次变换
    if (!fitAffine())
        return false;
    auto idxes = findAbnormal_RANSAC(data, threshold);
    // 第二次变换
    std::ranges::sort(idxes, std::ranges::greater{});
    for (const auto idx : idxes)
        data.erase(data.begin() + idx);
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
 * @brief 评估仿射变换效果
 * @param print 是否输出至控制台
 * @return 均方根误差,误差列表
 */
std::pair<double, std::vector<double>> AffineTransformer::evaluate (const bool print) {
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
