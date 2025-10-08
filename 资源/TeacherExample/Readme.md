# Software-Development-Comprehensive-Experiment Part 4

程序构建和软件测试。

详情请参见实验指导书。

**MAKE SURE you have installed gtest!!!**

## 命令概要

```shell
// make
make
./bin/zoo_boom.out

// use valgrind to check mem
valgrind --tool=memcheck --leak-check=full ./bin/zoo_boom.out &> val.txt

// perf stat
make perf

// gprof + gprof2dot, to visualize gmon.out
make clean
make
./bin/zoo_boom.out
make gprof  // the call graph png is in test/output.png

// gtest 
cd test
make
./gtest.out

// clean
make clean

// top
top
```

## 功能简介

1. make: 使用make来自动化编译项目
2. perf: tiger中长循环，过度消耗cpu，可以通过perf stat查看
3. valgrind: mouse中内存泄漏，可通过valgrind查看
4. top: wolf中大量分配内存不合理，可通过top查看mem
5. gtest: 创建了一个成功用例一个失败用例
6. gprof: 可视化函数调用链
