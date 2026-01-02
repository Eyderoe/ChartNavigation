#include "affineTransformer.hpp"
#include "tools/randomGen.hpp"

#include <algorithm>
#include <format>
#include <iostream>
#include <ranges>


std::vector<int> findAbnormal_IQR (const std::vector<double> &values);
std::vector<int> findAbnormal_MAD (const std::vector<double> &values);
std::vector<int> findAbnormal_RANSAC (std::vector<std::vector<double>> &values, double threshold = 10);


double percentile (const Eigen::VectorXd &vec, const double q) {
    Eigen::VectorXd sorted{vec};
    std::sort(sorted.data(), sorted.data() + sorted.size());
    const int idx = static_cast<int>(q * (static_cast<int>(sorted.size()) - 1));
    return sorted(idx);
}

/**
 * @brief 采取IQR方法筛选异常值
 * @param values 值列表
 * @return 异常值位置列表
 */
std::vector<int> findAbnormal_IQR (const std::vector<double> &values) {
    Eigen::VectorXd data(values.size());
    for (int i = 0; i < values.size(); ++i)
        data(i) = values[i];
    // 计算参数
    const double Q1 = percentile(data, 0.25); // 第一四分位数
    const double Q3 = percentile(data, 0.75); // 第三四分位数
    const double IQR = Q3 - Q1;
    const double lowerBound = Q1 - 1.5 * IQR;
    const double upperBound = Q3 + 1.5 * IQR;
    // 查找异常值的索引
    std::vector<int> abnormalValues;
    for (size_t i = 0; i < values.size(); ++i) {
        if ((values[i] < lowerBound) || (values[i] > upperBound))
            abnormalValues.emplace_back(i);
    }
    return abnormalValues;
}

double median (const Eigen::VectorXd &data) {
    Eigen::VectorXd sortedData = data;
    std::sort(sortedData.data(), sortedData.data() + sortedData.size());
    if (const long long size = sortedData.size(); size % 2 == 0) {
        return (sortedData(size / 2 - 1) + sortedData(size / 2)) / 2.0;
    } else {
        return sortedData(size / 2);
    }
}

double mad (const Eigen::VectorXd &data) {
    const double med = median(data);
    Eigen::VectorXd absDev(data.size());
    for (int i = 0; i < data.size(); ++i) {
        absDev(i) = std::abs(data(i) - med);
    }
    return median(absDev);
}

/**
 * @brief 基于MAD算法筛选异常值
 * @param values 值列表
 * @return 异常值位置列表
 */
std::vector<int> findAbnormal_MAD (const std::vector<double> &values) {
    Eigen::VectorXd data(values.size());
    for (int i = 0; i < values.size(); ++i)
        data(i) = values[i];
    // 计算 MAD 参数
    const double med = median(data);
    const double madValue = mad(data);
    constexpr double threshold = 3.0;
    // 查找异常值的索引
    std::vector<int> abnormalValues;
    for (size_t i = 0; i < values.size(); ++i) {
        if (std::abs(values[i] - med) > threshold * madValue) {
            abnormalValues.push_back(static_cast<int>(i));
        }
    }
    return abnormalValues;
}

/**
 * @brief 基于RANSAC算法筛选异常值
 * @param values 原始值列表(非MAD/IRQ误差值列表)
 * @param threshold 异常阈值
 * @return 异常值位置列表
 */
std::vector<int> findAbnormal_RANSAC (std::vector<std::vector<double>> &values, double threshold) {
    const int n = static_cast<int>(values.size()); // 数据量
    constexpr int iterations = 200; // 迭代次数
    int maxInliersCount = -1;
    std::vector<int> bestInlierIndices;
    // 迭代循环
    for (int i = 0; i < iterations; ++i) {
        // 随机选择点作为样本
        std::set<int> samples;
        while (samples.size() < 3)
            samples.insert(spawnInt(0, n - 1));
        // 仿射变换
        auto view = std::views::iota(size_t{0}, values.size())
                | std::views::filter([&](size_t i) { return samples.contains(i); })
                | std::views::transform([&](size_t i) -> std::vector<double>& { return values[i]; });
        auto [pX,pY] = doAffine(view);

        // 3. 验证所有点，计算内点数量
        std::vector<int> currentInlierIndices;
        for (int j = 0; j < n; ++j) {
            double lon = values[j][1], lat = values[j][0];
            double xT = values[j][2], yT = values[j][3];

            double xP = pX(0) * lon + pX(1) * lat + pX(2);
            double yP = pY(0) * lon + pY(1) * lat + pY(2);

            double dist = std::sqrt(std::pow(xP - xT, 2) + std::pow(yP - yT, 2));
            if (dist < threshold) {
                currentInlierIndices.push_back(j);
            }
        }

        // 4. 更新最优模型
        if (static_cast<int>(currentInlierIndices.size()) > maxInliersCount) {
            maxInliersCount = static_cast<int>(currentInlierIndices.size());
            bestInlierIndices = currentInlierIndices;
        }

        // 如果已经覆盖了 95% 的点，认为已经找到最优解，提前退出
        if (maxInliersCount > n * 0.95) break;
    }

    // 5. 找出所有的离群点索引
    std::vector<int> abnormalValues;
    std::vector<bool> isInlier(n, false);
    for (int idx : bestInlierIndices) {
        isInlier[idx] = true;
    }

    for (int i = 0; i < n; ++i) {
        if (!isInlier[i]) {
            abnormalValues.push_back(i);
        }
    }

    return abnormalValues;
}

/**
 * @brief 加载数据
 * @param dataList [[纬度,经度,x,y], ...]
 * @return 数据是否可用
 */
bool AffineTransformer::loadData (const std::vector<std::vector<double>> &dataList) {
    data = dataList;
    // 第一次变换
    if (!fitAffine())
        return false;
    auto [_, errors] = evaluate();
    auto idxes = findAbnormal_MAD(errors);
    // auto idxes = findAbnormal_RANSAC(data);
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
