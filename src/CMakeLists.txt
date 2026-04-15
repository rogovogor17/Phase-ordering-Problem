#CMakeLists.txt for source files

target_sources(implementation PRIVATE
	utils.cpp
)

target_include_directories(implementation PUBLIC ${CMAKE_SOURCE_DIR}/include)

set(MAIN_TARGET ${PROJECT_NAME})
add_executable(${MAIN_TARGET} main.cpp)

target_link_libraries(${MAIN_TARGET} PRIVATE implementation)
target_include_directories(${MAIN_TARGET} PRIVATE ${CMAKE_SOURCE_DIR}/include)
set_target_properties(${MAIN_TARGET} PROPERTIES CXX_INCLUDE_WHAT_YOU_USE "${IWYU_COMMAND}")
target_compile_options(${MAIN_TARGET} PRIVATE ${FLAGS})
target_link_options(${MAIN_TARGET} PRIVATE ${FLAGS})
