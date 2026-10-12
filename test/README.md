# 单元测试

沿用 doctest 和 CTest。使用项目现有的 CMake 配置（`BUILD_TESTING=ON`）：

```sh
cmake --build cmake-build-debug --target unit_tests -j 4
ctest --test-dir cmake-build-debug --output-on-failure
```

构建目录不是 `cmake-build-debug` 时替换为自己的目录。单独运行一组用例：

```sh
./cmake-build-debug/test/unit_tests --test-case="*gnomonic*"
```

覆盖字符串处理、地理计算、三种地图投影、飞机轨迹、地图数据库查询与缓存、GLOBE 地形读取。
测试使用 QCoreApplication，无需显示器、网络、用户配置或本机 GLOBE 数据目录。
数据库用例复制仓库中的空数据库模板到临时目录，再插入少量已知记录；不会修改模板。
地形用例创建一个符合格式大小的临时稀疏瓦片，仅写入少量高度值，结束时自动清理。

新增用例优先覆盖正常使用路径和有实际意义的边界；数值算法同时检查已知结果或几何性质，
避免只做往返转换。无需为简单逻辑引入 mock 框架、随机数据或大批重复组合。
这些测试不覆盖窗口交互、macOS Spaces 或真实 AIRAC 地图绘制效果。
