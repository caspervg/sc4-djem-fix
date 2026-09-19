add_executable(SC4DjemFixTests
    FocusedTests.cpp
    "${CMAKE_SOURCE_DIR}/src/dll/DjemLogic.cpp"
    "${CMAKE_SOURCE_DIR}/src/dll/utils/Logger.cpp"
    "${CMAKE_SOURCE_DIR}/src/dll/utils/Settings.cpp"
)

target_include_directories(SC4DjemFixTests PRIVATE
    "${CMAKE_SOURCE_DIR}/src/dll"
    "${MINI_INCLUDE_DIR}"
)

target_link_libraries(SC4DjemFixTests PRIVATE
    doctest::doctest
    spdlog::spdlog
)

if(MSVC)
    set_target_properties(SC4DjemFixTests PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )
endif()

add_test(NAME SC4DjemFix.FocusedTests COMMAND SC4DjemFixTests)
