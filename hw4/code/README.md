# GAMES101 Homework 4

完成 de Casteljau 递归算法、绿色曲线及提高题距离反走样。支持任意非空控制点序列；基本曲线按 0.001 步长含两个端点采样，反走样按 0.00025 步长，以到邻近像素中心距离的线性衰减设置绿色强度，取最大值避免采样密度导致重复累加过亮。

```sh
sudo apt-get install libopencv-dev
cmake -S . -B build
cmake --build build --parallel 2
./build/BezierCurve
./build/BezierCurve --aa
./build/BezierCurve --render curve.png green 80 550 180 80 500 120 620 550
./build/BezierCurve --render aa.png aa 80 550 180 80 500 120 620 550
./build/BezierCurve --render compare.png compare 80 550 180 80 500 120 620 550
```

窗口中鼠标左键选四点，随后自动绘制并保存 my_bezier_curve.png，按键退出；--aa 启用反走样。green 是递归绿色曲线，compare 同时绘制红色 Bernstein 曲线供黄色叠加比较，aa 是提高题结果。images/ 内包含三种示例结果。源码包首层有所有代码、CMake、README，不含 build、.vscode 或 PDF。未提交 SmartChair 或获得评分。
