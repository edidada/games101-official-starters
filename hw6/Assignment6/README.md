# GAMES101 HW6：包围盒、BVH 与 SAH

基于仓库 main 的官方学生起始包（9d8f389）。独立分支 `edidada/hw6`。
本作业包含基础 AABB/BVH 与加分项 SAH；公开自动验证不等于原课程评分或私有评测。

## 编译与运行

只需 C++17 编译器与 CMake 3.13+，不依赖 OpenCV。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
cd build
./hw6_tests
./hw6_tests --benchmark
./RayTracing naive
mv binary.ppm naive.ppm
./RayTracing sah
mv binary.ppm sah.ppm
```

必须在源码内的 `build` 目录运行，程序从 `../models/bunny/bunny.obj` 读取随包模型。
默认无参数等同 `naive`；两种方式均生成官方相机、1280×960、两盏灯的 bunny 图。
`hw6_tests --benchmark` 在当前目录生成 `benchmark.csv`。
额外开启 `-DHW6_SANITIZE=ON` 可用 ASan、UBSan 和 LeakSanitizer 检查。

## 实现

- Renderer：像素中心采样，按 FOV 与宽高比生成归一化的 -Z 方向主射线。
- Triangle：双精度 Möller–Trumbore，保持框架正面命中/背面剔除约定；检查重心与 Ray 的闭区间，填充距离、坐标、法向、物体及材质指针。小面积三角形不使用固定绝对阈值误删。
- Bounds3：逐轴 slab 与 `[t_min,t_max]` 取交集；平行射线显式检查原点，覆盖正负零和边界。框架逆方向为 float，可能溢出或舍入，因此从方向分量重算 double 倒数；保留原有接口。
- BVH 查找：先测试节点包围盒，叶节点求交；先查左子树，再以已有距离收紧右子树区间，返回最近命中。空树返回未命中。树只拥有节点，不拥有场景物体。
- SAH：每个内部节点分别按 X/Y/Z 的包围盒中心排序，用前缀/后缀 bounds 扫描所有非空划分，最小化 `N_L*A_L + N_R*A_R`。父面积及遍历成本对同节点候选是常数。每叶保留一个图元，维持作业节点结构；相同成本偏向较均衡划分。选定轴/位置后重排一次，不在每个改进候选复制完整数组。这是精确 sweep SAH，不是分桶近似或中位数别名。
- 材质与内外层 BVH 通过 RAII 释放；修复框架空树 root 未初始化和旧网格求交接口访问未初始化数组的问题。

算法参考：[PBRT 4e, Bounding Volume Hierarchies](https://pbr-book.org/4ed/Primitives_and_Intersection_Acceleration/Bounding_Volume_Hierarchies)。实现保留教学树结构，不采用 PBRT 的线性节点布局或多图元叶子。

## 验证与时间对照

远端 `ci/verify-hw6.sh` 完成全部编译/运行；本地只做静态检查。

1. normal 和 ASan/UBSan 两套构建分别检查 12000 组随机 AABB、平行/正负零/边界/有限区间/空盒/退化盒/极小方向，参考算法枚举六个面的穿越。
2. 三角形使用独立的平面相交及 Gram 矩阵重心算法作对照；覆盖背面、边界、退化、极小三角形及交点信息。
3. 256 个图元、3000 条射线逐个暴力计算最近距离，与中位数 BVH、SAH 分别比较；检查空树、单图元、相同中心、树包围关系及唯一叶子。独立穷举检查 SAH 根节点代价最优。
4. bunny 使用同一 192×144 主射线集，先预热两种构建，再交替顺序测量三轮；记录各轮构建/遍历毫秒、命中数、距离和。另抽样 bunny 射线与独立暴力算法核对。性能只代表该 runner、该模型与射线，不宣称所有场景都加速。
5. 两种构建各生成中位数与 SAH 的完整 1280×960 PPM；四张图必须逐字节相同，且含有效前景与明暗变化。保存 PNG、原始日志与 timing。
6. 自动生成 `edidada_Homework6.zip`，含全部源文件、模型、CMake、README、images 和时间对照；排除 build/.vs。解压到新目录独立编译，重跑测试与两种渲染，要求完整 PPM 与主构建一致。

Actions 的 `hw6-evidence` artifact 提供 `benchmark.csv`、`benchmark-summary.json`、`timing.md` 与结果图片。打包 README 会追加该次运行的实际时间表；不得把 sanitizer 时间当成发布构建性能。

## 范围

教学模型仍是浮点顶点、硬阴影和单线程 Whitted 渲染；保留官方背面剔除与光照外观。没有实现通用 OBJ 多网格场景、路径追踪或多图元叶子。
