set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5OpenGL REQUIRED)
find_package(Qt5Widgets REQUIRED)

add_executable(rwviewer WIN32
    main.cpp
    ViewerWindow.cpp

    models/ObjectListModel.cpp
    models/DFFFramesTreeModel.cpp

    views/ViewerInterface.hpp
    views/ObjectViewer.cpp
    views/ModelViewer.cpp
    views/WorldViewer.cpp
    views/ViewerInterface.cpp

    ViewerWidget.cpp
    ItemListModel.cpp
    ItemListWidget.cpp
    IMGArchiveModel.cpp
    widgets/ModelFramesWidget.cpp
    AnimationListModel.cpp
    AnimationListWidget.cpp
    )

target_link_libraries(rwviewer
    rwengine
    Qt5::OpenGL
    Qt5::Widgets
    )

openrw_target_apply_options(TARGET rwviewer)

install(TARGETS rwviewer
    RUNTIME DESTINATION "${BIN_DIR}"
    )
