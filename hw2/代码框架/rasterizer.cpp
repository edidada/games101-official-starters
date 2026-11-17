// clang-format off
//
// Created by goksu on 4/6/19.
//

#include <algorithm>
#include <vector>
#include "rasterizer.hpp"
#include <opencv2/opencv.hpp>
#include <math.h>
#include <stdexcept>


rst::pos_buf_id rst::rasterizer::load_positions(const std::vector<Eigen::Vector3f> &positions)
{
    auto id = get_next_id();
    pos_buf.emplace(id, positions);

    return {id};
}

rst::ind_buf_id rst::rasterizer::load_indices(const std::vector<Eigen::Vector3i> &indices)
{
    auto id = get_next_id();
    ind_buf.emplace(id, indices);

    return {id};
}

rst::col_buf_id rst::rasterizer::load_colors(const std::vector<Eigen::Vector3f> &cols)
{
    auto id = get_next_id();
    col_buf.emplace(id, cols);

    return {id};
}

auto to_vec4(const Eigen::Vector3f& v3, float w = 1.0f)
{
    return Vector4f(v3.x(), v3.y(), v3.z(), w);
}


static double edge(const Vector3f& a, const Vector3f& b, double x, double y)
{
    return (double(b.x())-a.x())*(y-a.y())-(double(b.y())-a.y())*(x-a.x());
}
static bool insideTriangle(float x, float y, const Vector3f* v)
{
    const double area = edge(v[0], v[1], v[2].x(), v[2].y());
    if (area == 0) return false;
    for (int i = 0; i < 3; ++i) {
        const auto& a = v[area > 0 ? i : (i+1)%3];
        const auto& b = v[area > 0 ? (i+1)%3 : i];
        const double e = edge(a, b, x, y);
        const double dx = double(b.x())-a.x(), dy = double(b.y())-a.y();
        // Half-open boundary: a shared edge belongs to exactly one triangle.
        if (e < 0 || (e == 0 && !(dy > 0 || (dy == 0 && dx < 0)))) return false;
    }
    return true;
}

static std::tuple<float, float, float> computeBarycentric2D(float x, float y, const Vector3f* v)
{
    float c1 = (x*(v[1].y() - v[2].y()) + (v[2].x() - v[1].x())*y + v[1].x()*v[2].y() - v[2].x()*v[1].y()) / (v[0].x()*(v[1].y() - v[2].y()) + (v[2].x() - v[1].x())*v[0].y() + v[1].x()*v[2].y() - v[2].x()*v[1].y());
    float c2 = (x*(v[2].y() - v[0].y()) + (v[0].x() - v[2].x())*y + v[2].x()*v[0].y() - v[0].x()*v[2].y()) / (v[1].x()*(v[2].y() - v[0].y()) + (v[0].x() - v[2].x())*v[1].y() + v[2].x()*v[0].y() - v[0].x()*v[2].y());
    float c3 = (x*(v[0].y() - v[1].y()) + (v[1].x() - v[0].x())*y + v[0].x()*v[1].y() - v[1].x()*v[0].y()) / (v[2].x()*(v[0].y() - v[1].y()) + (v[1].x() - v[0].x())*v[2].y() + v[0].x()*v[1].y() - v[1].x()*v[0].y());
    return {c1,c2,c3};
}

void rst::rasterizer::draw(pos_buf_id pos_buffer, ind_buf_id ind_buffer, col_buf_id col_buffer, Primitive type)
{
    if (type != Primitive::Triangle) throw std::invalid_argument("triangle primitive required");
    auto& buf = pos_buf.at(pos_buffer.pos_id);
    auto& ind = ind_buf.at(ind_buffer.ind_id);
    auto& col = col_buf.at(col_buffer.col_id);

    float f1 = (50 - 0.1) / 2.0;
    float f2 = (50 + 0.1) / 2.0;

    Eigen::Matrix4f mvp = projection * view * model;
    for (auto& i : ind)
    {
        Triangle t;
        Eigen::Vector4f v[] = {
                mvp * to_vec4(buf[i[0]], 1.0f),
                mvp * to_vec4(buf[i[1]], 1.0f),
                mvp * to_vec4(buf[i[2]], 1.0f)
        };
        if (!v[0].allFinite() || !v[1].allFinite() || !v[2].allFinite()
            || v[0].w() <= 0 || v[1].w() <= 0 || v[2].w() <= 0) continue;
        //Homogeneous division
        for (auto& vec : v) {
            vec /= vec.w();
        }
        //Viewport transformation
        for (auto & vert : v)
        {
            vert.x() = 0.5*width*(vert.x()+1.0);
            vert.y() = 0.5*height*(vert.y()+1.0);
            vert.z() = vert.z() * f1 + f2;
        }

        for (int i = 0; i < 3; ++i)
        {
            t.setVertex(i, v[i].head<3>());
            t.setVertex(i, v[i].head<3>());
            t.setVertex(i, v[i].head<3>());
        }

        auto col_x = col[i[0]];
        auto col_y = col[i[1]];
        auto col_z = col[i[2]];

        t.setColor(0, col_x[0], col_x[1], col_x[2]);
        t.setColor(1, col_y[0], col_y[1], col_y[2]);
        t.setColor(2, col_z[0], col_z[1], col_z[2]);

        rasterize_triangle(t);
    }
}

//Screen space rasterization
void rst::rasterizer::rasterize_triangle(const Triangle& t) {
    if (!t.v[0].allFinite() || !t.v[1].allFinite() || !t.v[2].allFinite()) return;
    if (edge(t.v[0],t.v[1],t.v[2].x(),t.v[2].y()) == 0) return;
    const float left = std::min({t.v[0].x(),t.v[1].x(),t.v[2].x()});
    const float right = std::max({t.v[0].x(),t.v[1].x(),t.v[2].x()});
    const float bottom = std::min({t.v[0].y(),t.v[1].y(),t.v[2].y()});
    const float top = std::max({t.v[0].y(),t.v[1].y(),t.v[2].y()});
    const int xmin = int(std::floor(std::clamp(left,0.f,float(width))));
    const int xmax = int(std::ceil(std::clamp(right,0.f,float(width))))-1;
    const int ymin = int(std::floor(std::clamp(bottom,0.f,float(height))));
    const int ymax = int(std::ceil(std::clamp(top,0.f,float(height))))-1;
    const Eigen::Vector2f offsets[4] = {{.25f,.25f},{.75f,.25f},{.25f,.75f},{.75f,.75f}};
    for (int y=ymin; y<=ymax; ++y) for (int x=xmin; x<=xmax; ++x) {
        const int pixel = get_index(x,y);
        for (int sample=0; sample<sample_count; ++sample) {
            const float sx = x+(sample_count==1?.5f:offsets[sample].x());
            const float sy = y+(sample_count==1?.5f:offsets[sample].y());
            if (!insideTriangle(sx,sy,t.v)) continue;
            const auto [a,b,c] = computeBarycentric2D(sx,sy,t.v);
            const float z = a*t.v[0].z()+b*t.v[1].z()+c*t.v[2].z();
            const int index = pixel*sample_count+sample;
            if (std::isfinite(z) && z<depth_buf[index]) {
                depth_buf[index]=z; sample_colors[index]=t.getColor();
            }
        }
        Eigen::Vector3f sum = Eigen::Vector3f::Zero();
        for (int sample=0; sample<sample_count; ++sample) sum+=sample_colors[pixel*sample_count+sample];
        frame_buf[pixel]=sum/float(sample_count);
    }
}

void rst::rasterizer::set_model(const Eigen::Matrix4f& m)
{
    model = m;
}

void rst::rasterizer::set_view(const Eigen::Matrix4f& v)
{
    view = v;
}

void rst::rasterizer::set_projection(const Eigen::Matrix4f& p)
{
    projection = p;
}

void rst::rasterizer::clear(rst::Buffers buff)
{
    if ((buff & rst::Buffers::Color) == rst::Buffers::Color)
    {
        std::fill(frame_buf.begin(), frame_buf.end(), Eigen::Vector3f{0, 0, 0});
        std::fill(sample_colors.begin(), sample_colors.end(), Eigen::Vector3f{0,0,0});
    }
    if ((buff & rst::Buffers::Depth) == rst::Buffers::Depth)
    {
        std::fill(depth_buf.begin(), depth_buf.end(), std::numeric_limits<float>::infinity());
    }
}

rst::rasterizer::rasterizer(int w, int h, int samples) : width(w), height(h), sample_count(samples)
{
    if (w<=0 || h<=0 || (samples!=1 && samples!=4)) throw std::invalid_argument("invalid rasterizer dimensions/samples");
    frame_buf.resize(size_t(w)*h);
    depth_buf.resize(size_t(w)*h*samples);
    sample_colors.resize(size_t(w)*h*samples);
    model=view=projection=Eigen::Matrix4f::Identity();
    clear(Buffers::Color | Buffers::Depth);
}

int rst::rasterizer::get_index(int x, int y)
{
    return (height-1-y)*width + x;
}

void rst::rasterizer::set_pixel(const Eigen::Vector3f& point, const Eigen::Vector3f& color)
{
    //old index: auto ind = point.y() + point.x() * width;
    if (!point.allFinite() || point.x()<0 || point.x()>=width || point.y()<0 || point.y()>=height) return;
    auto ind = get_index(int(point.x()),int(point.y()));
    frame_buf[ind] = color;

}

// clang-format on