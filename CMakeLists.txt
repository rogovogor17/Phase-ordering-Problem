cmake_minimum_required(VERSION 3.20)
project(MyProject VERSION 1.0 LANGUAGES CXX)
enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FLAGS -DDEBUG -O0 -fsanitize=address,alignment -Wall -Wextra -Wfloat-equal
	-Weffc++ -Wnon-virtual-dtor -Woverloaded-virtual -Wsuggest-override
	-Wcast-align -Wcast-qual -Wconversion -Wsign-conversion -Wsign-promo
	-Wpacked -Wunused -fsized-deallocation
	-Wmissing-declarations -Wchar-subscripts -Wempty-body -Winline -Wshadow
	-Wswitch-default -Wundef -Wunreachable-code -Wvariadic-macros
	-fPIE -fno-omit-frame-pointer -Werror=vla)

find_program(IWYU_PATH NAMES include-what-you-use iwyu)
if(IWYU_PATH)
	set(IWYU_COMMAND "${IWYU_PATH}"
		"-Xiwyu;--verbose=1;"
		"-Xiwyu;--comment_style=long;"
		"-Xiwyu;--mapping_file=$ENV{IWYU_MAPPING_FILE};"
	)
endif()

add_library(implementation STATIC)
add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(benchmarks)
