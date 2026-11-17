#include "Triangle.hpp"
#include "rasterizer.hpp"
#include <eigen3/Eigen/Eigen>
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <opencv2/opencv.hpp>

constexpr double MY_PI = 3.1415926;

Eigen::Matrix4f get_view_matrix(Eigen::Vector3f eye_pos)
{
    Eigen::Matrix4f view = Eigen::Matrix4f::Identity();

    Eigen::Matrix4f translate;
    translate << 1, 0, 0, -eye_pos[0], 0, 1, 0, -eye_pos[1], 0, 0, 1,
        -eye_pos[2], 0, 0, 0, 1;

    view = translate * view;

    return view;
}

// Column vectors; right-handed active rotation, with angles in degrees.
Eigen::Matrix4f get_rotation(Eigen::Vector3f axis, float angle)
{
    const double norm = axis.cast<double>().norm();
    if (!axis.allFinite() || !std::isfinite(angle) || !(norm > 0))
        throw std::invalid_argument("rotation requires a finite nonzero axis and finite angle");
    const Eigen::Vector3f u = (axis.cast<double>() / norm).cast<float>();
    const double radians = std::remainder(static_cast<double>(angle), 360.0) * std::acos(-1.0) / 180.0;
    const float c = std::cos(radians), s = std::sin(radians);
    Eigen::Matrix3f cross;
    cross << 0, -u.z(), u.y(), u.z(), 0, -u.x(), -u.y(), u.x(), 0;
    Eigen::Matrix4f rotation = Eigen::Matrix4f::Identity();
    rotation.block<3, 3>(0, 0) = c * Eigen::Matrix3f::Identity()
        + (1 - c) * u * u.transpose() + s * cross;
    return rotation;
}

Eigen::Matrix4f get_model_matrix(float rotation_angle)
{
    return get_rotation(Eigen::Vector3f::UnitZ(), rotation_angle);
}

Eigen::Matrix4f get_projection_matrix(float eye_fov, float aspect_ratio,
                                      float zNear, float zFar)
{
    if (!std::isfinite(eye_fov) || !std::isfinite(aspect_ratio)
        || !std::isfinite(zNear) || !std::isfinite(zFar)
        || eye_fov <= 0 || eye_fov >= 180 || aspect_ratio <= 0
        || zNear <= 0 || zFar <= zNear)
        throw std::invalid_argument("invalid perspective frustum");
    const double scale = 1.0 / std::tan(eye_fov * std::acos(-1.0) / 360.0);
    const double near = zNear, far = zFar;
    Eigen::Matrix4f projection = Eigen::Matrix4f::Zero();
    projection(0, 0) = scale / aspect_ratio;
    projection(1, 1) = scale;
    projection(2, 2) = -(far + near) / (far - near);
    projection(2, 3) = -2 * far * near / (far - near);
    projection(3, 2) = -1;
    return projection;
}

int main(int argc, const char** argv)
{
    try {
    float angle = 0;
    bool command_line = false;
    std::string filename = "output.png";

    if (argc != 1) {
        if ((argc != 3 && argc != 4) || std::string(argv[1]) != "-r")
            throw std::invalid_argument("usage: Rasterizer [-r degrees [output.png]]");
        command_line = true;
        size_t consumed = 0;
        angle = std::stof(argv[2], &consumed);
        if (consumed != std::string(argv[2]).size() || !std::isfinite(angle))
            throw std::invalid_argument("angle must be a finite number");
        if (argc == 4) filename = argv[3];
    }

    rst::rasterizer r(700, 700);

    Eigen::Vector3f eye_pos = {0, 0, 5};

    std::vector<Eigen::Vector3f> pos{{2, 0, -2}, {0, 2, -2}, {-2, 0, -2}};

    std::vector<Eigen::Vector3i> ind{{0, 1, 2}};

    auto pos_id = r.load_positions(pos);
    auto ind_id = r.load_indices(ind);

    int key = 0;
    int frame_count = 0;

    if (command_line) {
        r.clear(rst::Buffers::Color | rst::Buffers::Depth);

        r.set_model(get_model_matrix(angle));
        r.set_view(get_view_matrix(eye_pos));
        r.set_projection(get_projection_matrix(45, 1, 0.1, 50));

        r.draw(pos_id, ind_id, rst::Primitive::Triangle);
        cv::Mat image(700, 700, CV_32FC3, r.frame_buffer().data());
        image.convertTo(image, CV_8UC3, 1.0f);

        if (!cv::imwrite(filename, image)) throw std::runtime_error("image write failed");

        return 0;
    }

    while (key != 27) {
        r.clear(rst::Buffers::Color | rst::Buffers::Depth);

        r.set_model(get_model_matrix(angle));
        r.set_view(get_view_matrix(eye_pos));
        r.set_projection(get_projection_matrix(45, 1, 0.1, 50));

        r.draw(pos_id, ind_id, rst::Primitive::Triangle);

        cv::Mat image(700, 700, CV_32FC3, r.frame_buffer().data());
        image.convertTo(image, CV_8UC3, 1.0f);
        cv::imshow("image", image);
        key = cv::waitKey(10);

        std::cout << "frame count: " << frame_count++ << '\n';

        if (key == 'a') {
            angle += 10;
        }
        else if (key == 'd') {
            angle -= 10;
        }
    }

    return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
