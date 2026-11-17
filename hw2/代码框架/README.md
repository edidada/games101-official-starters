# GAMES101 Homework 2

已实现像素中心三角形覆盖、bounding box 扫描、插值 z-buffer，以及提高题 2×2 supersampling。四个样本分别位于 (.25,.25)、(.75,.25)、(.25,.75)、(.75,.75)，各有独立深度和颜色；每个像素对四个样本求平均，共享边使用一致的半开归属规则，支持两种顶点绕序。

```sh
sudo apt-get install libeigen3-dev libopencv-dev
cmake -S . -B build
cmake --build build --parallel 2
./build/Rasterizer single.png
./build/Rasterizer ssaa.png --ssaa
./build/Rasterizer
./build/Rasterizer --ssaa
```

无输出文件参数时打开窗口，Esc 退出。模型按题目保持单位矩阵；透视投影沿用 HW1 的 -Z 观察、正 near/far 和 NDC 近 -1/远 +1。深度变换后越小越近；每个样本独立比较并平均最终颜色。只做屏幕 bounding box 裁切，不实现跨近面通用齐次裁剪；顶点齐次 w 非正或非有限的三角形跳过。

交付包包含 single.png、ssaa.png、完整源码和此 README，提高题已完成。仓库 CI 使用独立解析 oracle 检查覆盖和深度，含普通及 ASan/UBSan。尚未提交 SmartChair 或获得课程评分。源码包用 GitHub 用户名命名，个人提交可改真实姓名。
