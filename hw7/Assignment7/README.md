# GAMES101 HW7：路径追踪、多线程与 Microfacet

基于官方学生starter导入提交9d8f389的独立分支 `edidada/hw7`；迁移本仓库HW6已验证的三角形/AABB/BVH，保留HW7面积/采样接口。没有导入社区答案。

## 编译运行

C++17、CMake3.13+、标准线程库，无OpenCV：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
cd build
./hw7_tests
./RayTracing --size 512 --spp 8 --threads 4 --output diffuse.ppm
./RayTracing --size 512 --spp 32 --threads 4 --material metal --roughness 0.25 --output sharp.ppm
./RayTracing --size 512 --spp 32 --threads 4 --material metal --roughness 0.65 --output rough.ppm
```

必须在源码目录内的build运行，以读取`../models/cornellbox/`。默认512×512、16spp、4线程、seed7、官方Diffuse CornellBox。`--material metal`在前部加入同位置金属球，便于展示两种粗糙度的高光；其余官方模型/灯光保留。

`--seed`控制确定性；`--rr`默认0.8，0只用于直接光诊断，不能当完整路径追踪。CLI检查尺寸/spp/线程/粗糙度/概率范围。PPM用官方gamma0.6显示；同路径`.linear`保存逐像素RGB little-endian float32，便于在显示clamp前验证。

## 实现与得分点

- `castRay`使用面光源NEE：`Le*BRDF*cos_surface*cos_light/(distance²*pdf_area)`，阴影射线限于采样光源前；随机俄罗斯轮盘继续路径，吞吐乘`BRDF*cos/(pdf*P_RR)`，迭代形式不截断固定深度。间接路径撞光源不再重复加发射，摄像机直接看到发光面返回发射值，解决官方提及的黑光源。
- `sampleLight`按所有发光物体面积选择，乘物体选择概率。BVH面积CDF使用均匀随机数，修正starter中sqrt偏差；Triangle/Sphere采样填充材质/发射信息。Sphere从均匀z采样表面，修正uniform phi的极区偏差。
- 多线程按行领取任务，每个像素仅一个线程写入；随机状态为thread_local，并对seed/像素编号先做非线性哈希再重置，线程调度不会改变随机序列。图元与树在渲染阶段只读，控制台只在join后写。记录实际峰值worker及同工作量三轮时间。
- Microfacet在不改变`sample/eval/pdf`接口下采用各向同性GGX/Trowbridge–Reitz分布、Smith G1乘积、Schlick Fresnel；`alpha=roughness²`，微表面项`D*G*F/(4*NdotV*NdotL)`。保留作业允许的均匀半球sample/pdf，非重要性采样，所以低粗糙度会有较大方差。金属球Kd=0，F0=(0.85,0.6,0.2)；非delta粗糙度，不声称导体精确光谱或多重微表面散射。
- RAII清理节点、网格、材质，修复空树、旧网格重载及Vector索引问题。所有编译/运行只在Actions，本地只编辑/静态检查。

参考：[PBRT microfacet theory](https://pbr-book.org/4ed/Reflection_Models/Roughness_Using_Microfacet_Theory)、[path integrator](https://pbr-book.org/4ed/Light_Transport_I_Surface_Reflection/A_Better_Path_Tracer)。本作业遵循教学NEE+RR算法，没有实现PBRT的MIS或BSDF重要性采样。

## 验证与提交包

`ci/verify-hw7.sh`在普通Release与ASan/UBSan/LeakSanitizer构建运行：面积CDF/多光源选择100000次，半球/Sphere各50000次；2000对GGX互易性、4个解析法线高光、9个方向反照率积分；正方形面光源独立500×500数值积分与RR0/0.8各50000条路径的均值/标准误差对照，1000条遮挡、可见发光面、避免重复计光与无光源检查。

普通/sanitizer构建在128×128、8spp、1/4线程完整PPM及线性图相同；正式Diffuse512×512、8spp三轮1/4线程完整两类图相同，峰值worker实际大于1；Microfacet两张512×512、32spp、不同roughness；另128×128、64spp全局光/仅直接光检查阴影区域与总贡献差异。测试范围不等于私有评分。

工件保存源码ZIP、images、每张分辨率/spp/seed/时间、原始日志及线性图。包内README追加当次真实计算时间；干净解压独立配置/编译/测试，再完整复现Diffuse和Microfacet图。排除build/.vs；未提交SmartChair或取得课程评分。正式图像和原始计时在Actions完成后才可作为证据，不能由代码中参数推断成功。

## 随机序列条纹回归

首次6661f1d虽通过数值与并行一致性检查，但实际图像有横向条纹：若像素初始状态按生成器固定增量线性排列，相邻像素序列只是彼此移位，重复使用同一串随机数。修复为先哈希像素种子，再初始化各像素生成器，保留跨线程精确复现。新增4096像素的相邻序列移位重合与相关系数检查；Actions用旧6661f1d代码和同一新测试构建负例，必须因序列重合失败。所有正式图像与源码包都必须在修复后的完整运行重新生成，不能用旧绿灯或旧图交付。
