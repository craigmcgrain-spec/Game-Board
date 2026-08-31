cmake_minimum_required(VERSION 3.22)

project(HexboardIntegrated VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

include(GNUInstallDirs)
include(CTest)
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Test)

qt_add_executable(hexboard
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/boardwidget.cpp
    src/boardwidget.h
    src/dicerollerwidget.cpp
    src/dicerollerwidget.h
    src/chancewheelwidget.cpp
    src/chancewheelwidget.h
    src/geargeneratorwidget.cpp
    src/geargeneratorwidget.h
    src/namegeneratorwidget.cpp
    src/namegeneratorwidget.h
    src/platformmessagefilter.cpp
    src/platformmessagefilter.h
    src/playerspanel.cpp
    src/playerspanel.h
    src/tileassetpicker.cpp
    src/tileassetpicker.h
)

target_link_libraries(hexboard PRIVATE Qt6::Widgets)

if(BUILD_TESTING)
    qt_add_executable(session_tests
        tests/sessiontest.cpp
        src/boardwidget.cpp
        src/boardwidget.h
    )
    target_include_directories(session_tests PRIVATE src)
    target_link_libraries(session_tests PRIVATE Qt6::Widgets Qt6::Test)
    add_test(NAME session-round-trip COMMAND session_tests)
    set_tests_properties(session-round-trip PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_executable(tile_asset_picker_tests
        tests/tileassetpickertest.cpp
        src/tileassetpicker.cpp
        src/tileassetpicker.h
    )
    target_include_directories(tile_asset_picker_tests PRIVATE src)
    target_link_libraries(tile_asset_picker_tests PRIVATE Qt6::Widgets Qt6::Test)
    add_test(NAME tile-asset-picker COMMAND tile_asset_picker_tests)
    set_tests_properties(tile-asset-picker PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_executable(tabletop_tool_tests
        tests/tabletoptooltest.cpp
        src/dicerollerwidget.cpp
        src/dicerollerwidget.h
        src/chancewheelwidget.cpp
        src/chancewheelwidget.h
        src/geargeneratorwidget.cpp
        src/geargeneratorwidget.h
        src/namegeneratorwidget.cpp
        src/namegeneratorwidget.h
    )
    target_include_directories(tabletop_tool_tests PRIVATE src)
    target_link_libraries(tabletop_tool_tests PRIVATE Qt6::Widgets Qt6::Test)
    add_test(NAME tabletop-tools COMMAND tabletop_tool_tests)
    set_tests_properties(tabletop-tools PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    qt_add_executable(main_window_tests
        tests/mainwindowtest.cpp
        src/mainwindow.cpp
        src/mainwindow.h
        src/boardwidget.cpp
        src/boardwidget.h
        src/dicerollerwidget.cpp
        src/dicerollerwidget.h
        src/chancewheelwidget.cpp
        src/chancewheelwidget.h
        src/geargeneratorwidget.cpp
        src/geargeneratorwidget.h
        src/namegeneratorwidget.cpp
        src/namegeneratorwidget.h
        src/platformmessagefilter.cpp
        src/platformmessagefilter.h
        src/playerspanel.cpp
        src/playerspanel.h
        src/tileassetpicker.cpp
        src/tileassetpicker.h
    )
    target_include_directories(main_window_tests PRIVATE src)
    target_link_libraries(main_window_tests PRIVATE Qt6::Widgets Qt6::Test)
    add_test(NAME main-window-tools COMMAND main_window_tests)
    set_tests_properties(main-window-tools PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()

install(TARGETS hexboard
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES data/io.github.craigmcgrain.Hexboard.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)
install(FILES data/io.github.craigmcgrain.Hexboard.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)
install(FILES data/io.github.craigmcgrain.Hexboard.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
)

set(CPACK_PACKAGE_NAME "hexboard-integrated")
set(CPACK_PACKAGE_VENDOR "Craig McGrain")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "An integrated tabletop game board and utility suite")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase >= 6.5")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
include(CPack)
