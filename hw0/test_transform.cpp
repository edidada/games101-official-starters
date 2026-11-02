#include "transform.hpp"
#include <iostream>
#include <random>

int main() {
    const auto result = transform_point({2, 1}, 45, {1, 2});
    const double root = std::sqrt(2.0);
    if ((result - Eigen::Vector3d(1 + root/2, 2 + 3*root/2, 1)).norm() > 1e-12)
        return 1;
    std::mt19937 random(101);
    std::uniform_real_distribution<double> coordinates(-100, 100), angles(-720, 720);
    for (int i = 0; i < 1000; ++i) {
        Eigen::Vector2d point(coordinates(random), coordinates(random));
        Eigen::Vector2d offset(coordinates(random), coordinates(random));
        const double angle = angles(random);
        const auto moved = transform_point(point, angle, offset);
        const Eigen::Vector2d rotated = moved.head<2>() - offset;
        const auto restored = transform_point(rotated, -angle, {0, 0});
        if (std::abs(rotated.norm() - point.norm()) > 1e-10 ||
            (restored.head<2>() - point).norm() > 1e-10 || moved.z() != 1)
            return 2;
    }
    std::cout << "Original assignment point and 1000 inverse/norm invariants passed\n";
}
