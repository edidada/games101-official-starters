#include <chrono>
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <opencv2/opencv.hpp>

std::vector<cv::Point2f> control_points;

void mouse_handler(int event, int x, int y, int flags, void *userdata) 
{
    if (event == cv::EVENT_LBUTTONDOWN && control_points.size() < 4) 
    {
        std::cout << "Left button of the mouse is clicked - position (" << x << ", "
        << y << ")" << '\n';
        control_points.emplace_back(x, y);
    }     
}

void naive_bezier(const std::vector<cv::Point2f> &points, cv::Mat &window) 
{
    auto &p_0 = points[0];
    auto &p_1 = points[1];
    auto &p_2 = points[2];
    auto &p_3 = points[3];

    for (int step=0;step<=1000;++step) {
        double t=step/1000.0;
        auto point = std::pow(1-t,3)*p_0+3*t*std::pow(1-t,2)*p_1+3*t*t*(1-t)*p_2+t*t*t*p_3;
        if (point.x>=0 && point.x<window.cols && point.y>=0 && point.y<window.rows)
            window.at<cv::Vec3b>(int(point.y),int(point.x))[2]=255;
    }
}

cv::Point2f recursive_bezier(const std::vector<cv::Point2f>& points, float t)
{
    if (points.empty() || !std::isfinite(t) || t<0 || t>1)
        throw std::invalid_argument("nonempty control polygon and t in [0,1] required");
    for (const auto& p:points) if (!std::isfinite(p.x) || !std::isfinite(p.y))
        throw std::invalid_argument("finite control points required");
    if (points.size()==1) return points[0];
    std::vector<cv::Point2f> next;
    next.reserve(points.size()-1);
    for (size_t i=1;i<points.size();++i) next.push_back((1-t)*points[i-1]+t*points[i]);
    return recursive_bezier(next,t);
}

void bezier(const std::vector<cv::Point2f>& points, cv::Mat& window)
{
    for (int step=0;step<=1000;++step) {
        const auto p=recursive_bezier(points,step/1000.f);
        if (p.x>=0 && p.x<window.cols && p.y>=0 && p.y<window.rows)
            window.at<cv::Vec3b>(int(p.y),int(p.x))[1]=255;
    }
}

void bezier_aa(const std::vector<cv::Point2f>& points, cv::Mat& window)
{
    for (int step=0;step<=4000;++step) {
        const auto p=recursive_bezier(points,step/4000.f);
        if (p.x < -1 || p.x > window.cols+1 || p.y < -1 || p.y > window.rows+1) continue;
        for (int y=int(std::floor(p.y))-1;y<=int(std::floor(p.y))+1;++y)
        for (int x=int(std::floor(p.x))-1;x<=int(std::floor(p.x))+1;++x) {
            if (x<0 || y<0 || x>=window.cols || y>=window.rows) continue;
            const float distance=std::hypot(p.x-(x+.5f),p.y-(y+.5f));
            const auto intensity=cv::saturate_cast<unsigned char>(255*std::max(0.f,1-distance));
            auto& green=window.at<cv::Vec3b>(y,x)[1];
            green=std::max(green,intensity);
        }
    }
}

static void render(const std::vector<cv::Point2f>& points, cv::Mat& image, const std::string& mode)
{
    recursive_bezier(points,0);
    for (const auto& point:points) if (std::abs(point.x)>1000000 || std::abs(point.y)>1000000) throw std::invalid_argument("render coordinate out of range");
    for (const auto& point:points) cv::circle(image,point,3,{255,255,255},3);
    if (mode=="green") bezier(points,image);
    else if (mode=="aa") bezier_aa(points,image);
    else if ((mode=="naive" || mode=="compare") && points.size()==4) {
        naive_bezier(points,image);
        if (mode=="compare") bezier(points,image);
    } else throw std::invalid_argument("mode must be green, aa, naive or compare (last two require four points)");
}

int main(int argc,const char** argv)
{
    try {
    std::string mode="green";
    if (argc==2 && std::string(argv[1])=="--aa") mode="aa";
    else if (argc!=1) {
        if (argc<8 || std::string(argv[1])!="--render" || (argc-4)%2) throw std::invalid_argument("usage: BezierCurve [--aa] or --render output.png mode x0 y0 x1 y1 ...");
        std::vector<cv::Point2f> points;
        for (int i=4;i<argc;i+=2) points.emplace_back(std::stof(argv[i]),std::stof(argv[i+1]));
        cv::Mat image(700,700,CV_8UC3,cv::Scalar(0));
        render(points,image,argv[3]);
        return cv::imwrite(argv[2],image)?0:1;
    }
    cv::Mat window = cv::Mat(700, 700, CV_8UC3, cv::Scalar(0));
    cv::cvtColor(window, window, cv::COLOR_BGR2RGB);
    cv::namedWindow("Bezier Curve", cv::WINDOW_AUTOSIZE);

    cv::setMouseCallback("Bezier Curve", mouse_handler, nullptr);

    int key = -1;
    while (key != 27) 
    {
        for (auto &point : control_points) 
        {
            cv::circle(window, point, 3, {255, 255, 255}, 3);
        }

        if (control_points.size() == 4) 
        {
            render(control_points,window,mode);

            cv::imshow("Bezier Curve", window);
            cv::imwrite("my_bezier_curve.png", window);
            key = cv::waitKey(0);

            return 0;
        }

        cv::imshow("Bezier Curve", window);
        key = cv::waitKey(20);
    }

return 0;
    } catch (const std::exception& error) { std::cerr<<error.what()<<"\n"; return 1; }
}
