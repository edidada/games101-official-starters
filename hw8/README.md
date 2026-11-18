# GAMES101 HW8：绳子仿真

独立分支`edidada/hw8`，基于官方starter导入9d8f389。完整实现绳子构造、显式/半隐式Euler、力式Verlet与阻尼；另外实现文档1.3节的无限刚度长度约束模式。未提交SmartChair或取得课程评分。

## 构建与运行

```sh
sudo apt-get install libgl1-mesa-dev libfreetype6-dev libglew-dev libglfw3-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
./build/rope_tests
./build/rope_trace trace.csv
./build/ropesim -s 32
```

默认16节点、蓝色半隐式Euler、绿色力式Verlet；`1/2`切换蓝色显式/半隐式，`C`切换绿色约束/力式，`+/-`调整子步，Space暂停，N单步。`-g x y`设置重力、`-m`质量；`-f 40`在40个仿真帧后暂停，便于实际窗口复现截图。保留框架每显示帧推进1单位仿真时间的语义，不当作实时秒数。

只验证数值可用`-DHW8_GUI=OFF`，无需图形依赖。`-DHW8_SANITIZE=ON`启用ASan/UBSan；数值测试开启泄漏检查，GUI使用普通构建/Mesa软件渲染。全部编译和执行只在Actions，本地不编译运行。

## 公式与范围

构造均匀节点及相邻弹簧，保存静长，按索引固定节点；检查数量/质量/刚度/坐标/索引。新建对象由Rope拥有；原指针vector构造接口借用对象，析构不释放借用指针。

Hooke力`k*(length-rest)*direction`对两端等大反向。Euler先以重力和`-damping*velocity`阻力求加速度，再更新速度；显式位置用旧速度，半隐式用新速度，默认阻力系数0.01。固定点不移动，每步清空全部forces。

Verlet按PDF公式`x_new=x+(1-0.00005)*(x-x_prev)+a*dt²`，保存旧位置，更新速度便于方法切换。默认沿用starter初始last_position=position；精度实验给出一致历史位置，避免将起步历史误差混为积分阶数。位置Verlet的一致历史全局位置误差为二阶，不能把PDF所述精度直接说成全局四阶。默认Verlet阻尼是每步系数，因此改变步长也会改变单位时间耗散；跨步长收敛实验明确关闭阻尼。

约束模式先做无弹簧力的位置预测，再迭代投影至原长，按逆质量分配修正；相同质量各移动一半，固定点承担零移动。支持两点重合时用初始方向恢复。有限迭代存在残差，不声称所有长链都严格满足每条约束。

修复框架keyboard_event未接到Viewer的key_event问题；用系统OpenGL/GLEW/GLFW/Freetype构建原CGL源代码，避免旧CMake全局链接选项；保留许可证和原依赖源码。清理字体数组delete[]、FreeType、VBO及OpenGL上下文退出顺序。

## 验证与提交

Actions普通及ASan/UBSan数值构建验证拓扑、无效输入、借用生命周期、一步解析公式、动量、固定点/力清零、Euler阻力与Verlet默认阻尼；谐振子四级步长与解析cos对照，区分两种Euler一阶与Verlet二阶；能量增长/有界及50000步默认Verlet耗散；10000步约束链长度残差。四方法384行固定时刻坐标跨构建完整一致。

真实Xvfb窗口在32子步/40帧暂停，实际按1/2/C/+/-/N/Escape，检查两色绳子、模式/步数日志与退出0。工件保存Matplotlib固定坐标轴对照图、真实窗口两图、CSV和日志。源码ZIP排除build/.vscode/PDF，包含全部源文件/CMake/README/images，解压独立重建GUI与数值目标、测试和完整CSV复现，并再次启动窗口操作退出。数值、窗口、源码包三种证据分开；绿色编译不能代替运行验收。
