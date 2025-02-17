set(LIBRARY_NAME libpathfind)
set(PYTHON_NAME pathfind)

set(SRC
    BVH.cpp
    Map.cpp
    TemporaryObstacle.cpp
    Tile.cpp
    pathfind_c_bindings.cpp
)

add_library(${LIBRARY_NAME} STATIC ${SRC})
target_include_directories(${LIBRARY_NAME} PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(${LIBRARY_NAME} PRIVATE ${FILESYSTEM_LIBRARY} utility RecastNavigation::Recast RecastNavigation::Detour)

install(TARGETS ${LIBRARY_NAME} ARCHIVE DESTINATION lib)

if (PYTHON_COMPONENT)
    if (Python3_FOUND)
        Python3_add_library(${PYTHON_NAME} ${SRC} python.cpp)
    elseif (Python2_FOUND)
        Python2_add_library(${PYTHON_NAME} ${SRC} python.cpp)
    else()
        message(FATAL_ERROR "Python component defined but neither python2 nor python3 were found")
    endif()

    target_compile_definitions(${PYTHON_NAME} PUBLIC PYTHON BOOST_PYTHON_STATIC_LIB)
    target_include_directories(${PYTHON_NAME} PRIVATE ${Boost_INCLUDE_DIRS} ${PYTHON_INCLUDE_DIRS})
    target_link_libraries(${PYTHON_NAME} PRIVATE ${FILESYSTEM_LIBRARY} utility RecastNavigation::Recast RecastNavigation::Detour storm ${Boost_LIBRARIES})

    install(TARGETS ${PYTHON_NAME} DESTINATION lib)
endif()
