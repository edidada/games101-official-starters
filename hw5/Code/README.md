# GAMES101 Homework 5

完成像素中心主光线生成及双面 Möller–Trumbore 求交，返回非负射线参数和重心坐标；平行、退化、三角形外部与负向交点拒绝。中间计算用 double，最终接口保持 float，失败时不写输出参数。主光线通过透视屏幕像素中心并归一化。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
./build/RayTracing
```

生成 binary.ppm，默认场景1280×960、两个球体与棋盘地面。images/result.png 为运行结果。沿用官方材质/反射/折射及照明代码；未扩展到加速结构或通用场景导入。源码包首层包含所有源码/CMake/README，排除 build/.vscode/PDF。未提交 SmartChair 或取得课程评分。
