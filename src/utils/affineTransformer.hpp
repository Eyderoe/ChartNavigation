#ifndef CHARTNAVIGATION_AFFINETRANSFORMER_HPP
#define CHARTNAVIGATION_AFFINETRANSFORMER_HPP

#include <vector>
#include <Eigen/Dense>


template <typename DataContainer>
std::pair<Eigen::Vector3d, Eigen::Vector3d> doAffine (DataContainer &&data);

class AffineTransformer {
    public:
        bool loadData (const std::vector<std::vector<double>> &dataList);
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
    int n{0}, counter{0};
    for (auto &temp : data)
        ++n;
    Eigen::MatrixXd A(n, 3);
    Eigen::VectorXd B_x(n), B_y(n);
    for (auto &item : data) {
        A(counter, 0) = item[1];
        A(counter, 1) = item[0];
        A(counter, 2) = 1;
        B_x(counter) = item[2];
        B_y(counter) = item[3];
        ++counter;
    }
    auto x = A.colPivHouseholderQr().solve(B_x);
    auto y = A.colPivHouseholderQr().solve(B_y);
    return {x, y};
}

#endif //CHARTNAVIGATION_AFFINETRANSFORMER_HPP
