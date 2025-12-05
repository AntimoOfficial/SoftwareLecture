# Tests for FileBackupSoftware
cmake_minimum_required(VERSION 3.16)

# Enable testing
enable_testing()

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Find Qt
find_package(Qt6 COMPONENTS Core Gui Widgets Concurrent REQUIRED)

# Create a library from BackupEngine and BackupTypes for testing
add_library(backup_core STATIC
    ${CMAKE_SOURCE_DIR}/src/BackupEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/BackupTypes.cpp
)
target_include_directories(backup_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(backup_core PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent)

# Test utilities library
add_library(test_utils STATIC
    test_utils.cpp
)
target_include_directories(test_utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_utils PUBLIC backup_core GTest::gtest)

# Unit Tests for BackupTypes
add_executable(test_backup_types
    test_backup_types.cpp
)
target_link_libraries(test_backup_types PRIVATE
    backup_core
    test_utils
    GTest::gtest_main
)
add_test(NAME BackupTypesTests COMMAND test_backup_types)

# Unit Tests for Compression Algorithms
add_executable(test_compression
    test_compression.cpp
)
target_link_libraries(test_compression PRIVATE
    backup_core
    test_utils
    GTest::gtest_main
)
add_test(NAME CompressionTests COMMAND test_compression)

# Unit Tests for Encryption Algorithms
add_executable(test_encryption
    test_encryption.cpp
)
target_link_libraries(test_encryption PRIVATE
    backup_core
    test_utils
    GTest::gtest_main
)
add_test(NAME EncryptionTests COMMAND test_encryption)

# Integration Tests for Backup/Restore Workflow
add_executable(test_backup_restore
    test_backup_restore.cpp
)
target_link_libraries(test_backup_restore PRIVATE
    backup_core
    test_utils
    GTest::gtest_main
)
add_test(NAME BackupRestoreTests COMMAND test_backup_restore)

# All tests target
add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_backup_types test_compression test_encryption test_backup_restore
    COMMENT "Running all tests..."
)
