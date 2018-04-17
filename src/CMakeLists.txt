# Executables
add_executable(r_server rserver.cpp)
target_include_directories(r_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(r_server PRIVATE ${MAPPING_CORE_PATH}/src)

# Dependencies
add_subdirectory(${MAPPING_CORE_PATH} ${CMAKE_BINARY_DIR}/mapping-core)
target_link_libraries(r_server mapping_core_base_lib)

find_package(RInside REQUIRED)
include_directories(r_server ${R_INCLUDE_DIR} ${RINSIDE_INCLUDE_DIRS})
include_directories(r_server
        /usr/local/lib/R/site-library/RInside/include
        /usr/lib/R/site-library/Rcpp/include
        ) # TODO: add to find file
target_link_libraries(r_server ${RINSIDE_LIBRARIES})
target_link_libraries(r_server /usr/local/lib/R/site-library/RInside/lib/libRInside.so) # TODO: add to find file

find_package(R REQUIRED)
include_directories(r_server ${R_INCLUDE_DIR} ${Rcpp_INCLUDE_DIR})
target_link_libraries(r_server ${R_LIBRARIES})
