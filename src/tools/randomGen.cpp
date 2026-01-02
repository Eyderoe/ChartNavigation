#include "randomGen.hpp"

/**
 * @brief 生成随机整数 [min,max]
 * @param min 最小
 * @param max 最大
 * @return 随机数
 */
int spawnInt (const int min, const int max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(min, max);
    return dis(gen);
}
