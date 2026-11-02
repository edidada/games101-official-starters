#pragma once
#include <eigen3/Eigen/Dense>
#include <cmath>

inline Eigen::Vector3d transform_point(const Eigen::Vector2d& point,
                                     double degrees, const Eigen::Vector2d& offset) {
    const double radians = degrees * std::acos(-1.0) / 180.0;
    const double c = std::cos(radians), s = std::sin(radians);
    Eigen::Matrix3d rotation, translation;
    rotation << c, -s, 0, s, c, 0, 0, 0, 1;
    translation << 1, 0, offset.x(), 0, 1, offset.y(), 0, 0, 1;
    return translation * rotation * Eigen::Vector3d(point.x(), point.y(), 1);
}
