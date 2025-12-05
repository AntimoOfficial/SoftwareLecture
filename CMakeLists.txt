cmake_minimum_required(VERSION 3.16)
project(FileBackupSoftware LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable testing
option(BUILD_TESTS "Build unit tests" ON)

# Find Qt
find_package(Qt6 COMPONENTS Core Gui Widgets Concurrent REQUIRED)

# Add executable
add_executable(file-backup-software
    src/main.cpp
    src/BackupEngine.cpp
    src/BackupTypes.cpp
)

# Link to Qt
target_link_libraries(file-backup-software PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent)

# Include directories
target_include_directories(file-backup-software PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Add tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
