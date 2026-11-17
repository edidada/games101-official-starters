#include <opencv2/opencv.hpp>
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
cv::Point2f recursive_bezier(const std::vector<cv::Point2f>&,float);
void bezier(const std::vector<cv::Point2f>&,cv::Mat&);
void bezier_aa(const std::vector<cv::Point2f>&,cv::Mat&);
int main() {
    unsigned checked=0;
    for(unsigned count=1;count<=9;++count) {
        std::vector<cv::Point2f> points;
        for(unsigned i=0;i<count;++i)points.emplace_back(20+(i*71)%550,40+(i*137)%550);
        for(unsigned step=0;step<=100;++step) {
            float t=step/100.f;cv::Point2d expected(0,0);double binomial=1;
            for(unsigned i=0;i<count;++i) {
                double weight=binomial*std::pow(double(t),i)*std::pow(1-double(t),count-1-i);
                expected.x+=weight*points[i].x;expected.y+=weight*points[i].y;
                if(i+1<count)binomial*=double(count-1-i)/(i+1);
            }
            auto got=recursive_bezier(points,t);
            assert(cv::norm(cv::Point2d(got)-expected)<.0005);
            auto reverse=points;std::reverse(reverse.begin(),reverse.end());
            assert(cv::norm(got-recursive_bezier(reverse,1-t))<.0005);
            ++checked;
        }
        assert(recursive_bezier(points,0)==points.front());assert(recursive_bezier(points,1)==points.back());
    }
    try {recursive_bezier({},.5);assert(false);}catch(const std::invalid_argument&){}
    try {recursive_bezier({{0,0}},-1);assert(false);}catch(const std::invalid_argument&){}
    cv::Mat image(40,40,CV_8UC3,cv::Scalar(0));
    bezier_aa({{20.2f,20.3f}},image);
    for(int y=0;y<40;++y)for(int x=0;x<40;++x) {
        int expected=cv::saturate_cast<unsigned char>(255*std::max(0.f,1-std::hypot(20.2f-(x+.5f),20.3f-(y+.5f))));
        assert(std::abs(int(image.at<cv::Vec3b>(y,x)[1])-expected)<=1);
    }
    bezier({{-10,-10},{0,0},{39,39},{50,50}},image);
    bezier_aa({{-10,20},{50,20}},image);
    assert(image.at<cv::Vec3b>(0,0)[1]>0 && image.at<cv::Vec3b>(39,39)[1]>0);
    std::cout<<"PASS: "<<checked<<" Bernstein/reversal comparisons (1-9 control points), exact endpoints, invalid input, distance-kernel and viewport edge checks\n";
}
