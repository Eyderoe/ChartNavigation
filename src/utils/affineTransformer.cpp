#include "affineTransformer.hpp"


double percentile (const Eigen::VectorXd &vec, const double q) {
    Eigen::VectorXd sorted{vec};
    std::sort(sorted.data(), sorted.data() + sorted.size());
    const int idx = static_cast<int>(q) * (static_cast<int>(sorted.size()) - 1);
    return sorted(idx);
}

/**
 * @brief 采取IQR方法筛选异常值
 * @param values 值列表
 * @return 异常值列表
 */
std::vector<int> findAbnormal (const std::vector<double> &values) {
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
    auto idxes = findAbnormal(errors);
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
    const size_t n = data.size();
    if (n < 3)
        return false;
    Eigen::MatrixXd A(n, 3);
    Eigen::VectorXd B_x(n), B_y(n);
    for (int i = 0; i < n; ++i) {
        A(i, 0) = data[i][1];
        A(i, 1) = data[i][0];
        A(i, 2) = 1;
        B_x(i) = data[i][2];
        B_y(i) = data[i][3];
    }
    paramsX = A.colPivHouseholderQr().solve(B_x);
    paramsY = A.colPivHouseholderQr().solve(B_y);
    return true;
}
