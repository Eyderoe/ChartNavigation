#ifndef CHARTNAVIGATION_AFFINETRANSFORMER_HPP
#define CHARTNAVIGATION_AFFINETRANSFORMER_HPP

#include <vector>
#include <Eigen/Dense>

template <typename R>
concept DataContainer = std::ranges::forward_range<R> &&
        std::same_as<std::ranges::range_value_t<R>, std::vector<double>>;


template <typename DataContainer>
std::pair<Eigen::Vector3d, Eigen::Vector3d> doAffine (DataContainer &&data);

class AffineTransformer {
    public:
        bool loadData (const std::vector<std::vector<double>> &dataList, double threshold);
        std::pair<double, double> transform (double latitude, double longitude);
        std::pair<double, std::vector<double>> evaluate (bool print = false);
    private:
        bool fitAffine ();

        std::vector<std::vector<double>> data{};
        Eigen::Vector3d paramsX{}, paramsY{};
};


/**
 * @brief 计算仿射变换参数
 * @param data 比如vector<vector<>> 其中每个元素为 {lati,longi,x,y}
 * @return x,y参数
 * @note RANSAC计算参数需要变换,所以独立出来
 */
template <typename DataContainer>
std::pair<Eigen::Vector3d, Eigen::Vector3d> doAffine (DataContainer &&data) {
    auto begin = data.begin();
    auto end = data.end();
    const int n = std::ranges::distance(begin, end);
    Eigen::MatrixXd A(n, 3);
    Eigen::VectorXd B_x(n), B_y(n);
    int counter{0};
    for (auto &item : data) {
        A(counter, 0) = item[1];
        A(counter, 1) = item[0];
        A(counter, 2) = 1;
        B_x(counter) = item[2];
        B_y(counter) = item[3];
        ++counter;
    }
    Eigen::Vector3d x = A.colPivHouseholderQr().solve(B_x); // 必须显示声明 Debug模式下Eigen有额外类型检查
    Eigen::Vector3d y = A.colPivHouseholderQr().solve(B_y);
    return {x, y};
}

#endif //CHARTNAVIGATION_AFFINETRANSFORMER_HPP
