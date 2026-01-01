#ifndef CHARTNAVIGATION_AFFINETRANSFORMER_HPP
#define CHARTNAVIGATION_AFFINETRANSFORMER_HPP

#include <vector>
#include <Eigen/Dense>


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


#endif //CHARTNAVIGATION_AFFINETRANSFORMER_HPP
