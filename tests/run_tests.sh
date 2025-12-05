#!/bin/bash

# 测试运行脚本
# 用法: ./run_tests.sh [options]
#   -v, --verbose   显示详细输出
#   -f, --filter    过滤特定测试 (e.g., --filter BackupTypes)
#   -l, --list      列出所有测试
#   -h, --help      显示帮助

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

# 默认选项
VERBOSE=false
FILTER=""
LIST_ONLY=false

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -f|--filter)
            FILTER="$2"
            shift 2
            ;;
        -l|--list)
            LIST_ONLY=true
            shift
            ;;
        -h|--help)
            echo "用法: $0 [options]"
            echo "选项:"
            echo "  -v, --verbose   显示详细输出"
            echo "  -f, --filter    过滤特定测试 (e.g., --filter BackupTypes)"
            echo "  -l, --list      列出所有测试"
            echo "  -h, --help      显示帮助"
            exit 0
            ;;
        *)
            echo "未知选项: $1"
            exit 1
            ;;
    esac
done

# 检查构建目录是否存在
if [ ! -d "$BUILD_DIR" ]; then
    echo "构建目录不存在，正在创建并配置..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake .. -DBUILD_TESTS=ON
fi

cd "$BUILD_DIR"

# 检查测试是否已构建
if [ ! -f "tests/test_backup_types" ]; then
    echo "测试尚未构建，正在构建..."
    make -j$(nproc)
fi

echo "========================================"
echo "FileBackupSoftware 测试套件"
echo "========================================"
echo ""

# 列出测试
if [ "$LIST_ONLY" = true ]; then
    echo "可用的测试:"
    echo ""
    for test in test_backup_types test_compression test_encryption test_backup_restore; do
        echo "[$test]"
        ./tests/$test --gtest_list_tests 2>/dev/null | head -50
        echo ""
    done
    exit 0
fi

# 运行测试
if [ -n "$FILTER" ]; then
    # 过滤特定测试
    echo "运行过滤的测试: $FILTER"
    echo ""
    if [ "$VERBOSE" = true ]; then
        ctest -V -R "$FILTER"
    else
        ctest --output-on-failure -R "$FILTER"
    fi
else
    # 运行所有测试
    echo "运行所有测试..."
    echo ""
    if [ "$VERBOSE" = true ]; then
        ctest -V
    else
        ctest --output-on-failure
    fi
fi

echo ""
echo "========================================"
echo "测试完成"
echo "========================================"
