# GAMES101 Homework 1

已实现模型绕 Z 轴旋转、透视投影，以及提高题 `get_rotation(axis, angle)`：绕通过原点的任意非零轴旋转（Rodrigues 公式）。角度使用度数，轴会归一化，使用列向量与右手主动旋转。相机看向 -Z，zNear/zFar 为正距离，近/远平面映射至 NDC -1/+1。

依赖 C++17、Eigen3 和 OpenCV（Ubuntu: `sudo apt-get install libeigen3-dev libopencv-dev`）。

```sh
cmake -S . -B build
cmake --build build --parallel 2
./build/Rasterizer
./build/Rasterizer -r 20
./build/Rasterizer -r 20 image.png
```

窗口 A 增加 10 度、D 减少 10 度、Esc 退出；省略输出文件时保存 output.png。命令行拒绝非法选项与非有限角度。修正 starter 的 framebuffer 行索引为 height-1-y，避免底行越界。未实现作业范围外的填充、深度测试或通用裁剪。

仓库 ci 包含普通与 ASan/UBSan 几何/像素检查及 Xvfb 键盘交互验证。源码包可独立编译，默认不构建仓库专用检查。提高题已完成，未提交 SmartChair，未取得课程评分。打包文件以 GitHub 用户名命名，可在个人提交时改为真实姓名。
