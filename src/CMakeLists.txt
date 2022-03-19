add_executable(ascii "./main.cpp")
target_link_libraries(
  ascii
  PRIVATE project_options
          project_warnings
          CONAN_PKG::stb
          CONAN_PKG::fmt
          CONAN_PKG::spdlog)
