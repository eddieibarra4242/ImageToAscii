add_executable(ascii "./main.cpp")

include_directories(../stb/)

target_link_libraries(
  ascii
  PRIVATE project_options
          project_warnings)
