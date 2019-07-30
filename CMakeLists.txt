project(BuildRT)

find_package(DCMTK REQUIRED)

set (OpenCV_DIR "G:/Code/OpenCV/OpenCV-MinGW-Build")

find_package(OpenCV REQUIRED)

include_directories(${DCMTK_INCLUDE_DIRS})
include_directories( ${OpenCV_INCLUDE_DIRS} )

add_executable(buildRT main.cpp)

target_link_libraries(buildRT ${DCMTK_LIBRARIES} ${OpenCV_LIBS})
