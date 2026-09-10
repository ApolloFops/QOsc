# add the library sources
set(
    headers
        QOsc
        qosc.h

        qosc_global.h

        QOscInterface
        qoscinterface.h

        QOscTcpInterface
        qosctcpinterface.h

        QOscUdpInterface
        qoscudpinterface.h

        QOscValue
        qoscvalue.h

        QOscMessage
        qoscmessage.h

        QOscBundle
        qoscbundle.h

        QOscMethod
        qoscmethod.h
)

add_library(QOsc
    SHARED
        ${headers}
        qosc_global.cpp

        qoscinterface.cpp

        qosctcpinterface.cpp

        qoscudpinterface.cpp

        qoscvalue.cpp
        qoscvalue_p.h

        qoscmessage.cpp
        qoscmessage_p.h

        qoscbundle.cpp
        qoscbundle_p.h

        qoscmethod.cpp
)

# Don't link the platform plugin, this fixes builds on iOS for some reason
qt_import_plugins(QOsc EXCLUDE_BY_TYPE platforms)

# define library target
target_link_libraries(
    QOsc
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Network
)
target_include_directories(
    QOsc
    PUBLIC
        $<BUILD_INTERFACE:${QOSC_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:${include_dest}>

    PRIVATE
        ${${Qt}Core_PRIVATE_INCLUDE_DIRS}
)
target_compile_definitions(QOsc PRIVATE QOSC_LIBRARY)
target_compile_definitions(QOsc PRIVATE SOFT_VERSION=${PROJECT_VERSION})
target_compile_definitions(QOsc PRIVATE GIT_VERSION=${GIT_VERSION})

# install
install(TARGETS QOsc EXPORT QOsc DESTINATION ${lib_dest})
install(FILES ${headers} DESTINATION ${include_dest})
