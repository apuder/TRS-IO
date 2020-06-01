idf_component_register(SRC_DIRS "lib"
                                "."
                       INCLUDE_DIRS "include"
                                    "include/esp"
                                    "include/smb2"
                                    "lib")

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(ENABLE_EXAMPLES "Build example programs" OFF)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules)

include(cmake/ConfigureChecks.cmake)

include_directories(${CMAKE_CURRENT_BINARY_DIR}
                    include
                    include/smb2)

