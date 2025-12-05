# FileBackupSoftware 测试套件

本目录包含 FileBackupSoftware 项目的自动化测试。

## 测试架构

测试使用 [Google Test](https://github.com/google/googletest) 框架，通过 CMake 的 FetchContent 自动下载。

### 测试分类

| 测试文件 | 测试类型 | 描述 |
|---------|---------|------|
| `test_backup_types.cpp` | 单元测试 | 测试数据类型的序列化/反序列化 |
| `test_compression.cpp` | 单元测试 | 测试压缩算法 (Huffman, RLE, Zlib) |
| `test_encryption.cpp` | 单元测试 | 测试加密算法 (XOR, RC4, AES256) |
| `test_backup_restore.cpp` | 集成测试 | 测试完整的备份/恢复工作流程 |

## 构建测试

```bash
# 进入项目根目录
cd SoftwareLecture

# 创建构建目录
mkdir -p build && cd build

# 配置 CMake（默认启用测试）
cmake .. -DBUILD_TESTS=ON

# 构建所有测试
make -j4

# 或者只构建特定测试
make test_backup_types
make test_compression
make test_encryption
make test_backup_restore
```

## 运行测试

### 使用 CTest（推荐）

```bash
cd build

# 运行所有测试
ctest

# 显示详细输出
ctest --output-on-failure

# 显示每个测试的详细信息
ctest -V

# 运行特定测试
ctest -R BackupTypes
ctest -R Compression
ctest -R Encryption
ctest -R BackupRestore
```

### 直接运行测试可执行文件

```bash
cd build

# 运行单个测试套件
./tests/test_backup_types
./tests/test_compression
./tests/test_encryption
./tests/test_backup_restore

# 运行特定测试用例
./tests/test_backup_types --gtest_filter="BackupTypesTest.FileKindToString"

# 列出所有测试
./tests/test_backup_types --gtest_list_tests
```

## 测试覆盖范围

### BackupTypes 测试 (23 tests)
- FileKind 枚举的序列化/反序列化
- CompressionMethod 枚举的序列化/反序列化
- EncryptionMethod 枚举的序列化/反序列化
- FileRecord 的 JSON 序列化/反序列化
- BackupManifest 的 JSON 序列化/反序列化
- Unicode 路径支持
- 特殊字符处理

### Compression 测试 (48 tests)
- Huffman 压缩（空数据、文本、二进制、随机数据）
- RLE 压缩（重复字节、无重复、混合数据）
- Zlib 压缩（各种数据类型和大小）
- 边界条件（单字节、最大运行长度等）
- 性能测试（小/中/大数据）

### Encryption 测试 (45+ tests)
- XOR 加密（对称性、各种密码长度）
- RC4 加密（流加密属性）
- AES256 加密（块大小处理、填充）
- 密码处理（空密码、Unicode 密码）
- 压缩+加密组合

### Backup/Restore 测试 (41 tests)
- 基本备份/恢复工作流程
- 带压缩的备份/恢复
- 带加密的备份/恢复
- 打包模式 (.fbk 文件)
- 错误处理（无效路径、错误密码等）
- 元数据保留
- 大文件和多文件备份
- Unicode 文件名和内容
- 数据完整性验证

## 添加新测试

1. 在相应的测试文件中添加新的测试用例：

```cpp
TEST_F(YourTestFixture, YourTestName)
{
    // 准备测试数据
    // 执行被测代码
    // 验证结果
    EXPECT_TRUE(result.success);
}
```

2. 使用提供的测试工具类：

```cpp
// 使用 TempDirectory 创建临时文件
TempDirectory temp;
temp.createFile("test.txt", "content");

// 使用 BackupTestFixture 进行备份测试
class MyTest : public BackupTestFixture {
    // 自动创建 m_sourceDir, m_destDir, m_restoreDir
};

// 使用 TestData 命名空间生成测试数据
QByteArray data = TestData::randomData(1024);
QByteArray text = TestData::textData(100);
```

## 禁用测试

如果需要禁用测试构建：

```bash
cmake .. -DBUILD_TESTS=OFF
```

## 持续集成

测试可以在 CI/CD 管道中运行：

```bash
# 构建并运行测试，失败时返回非零退出码
cd build
cmake .. -DBUILD_TESTS=ON
make -j4
ctest --output-on-failure
```
