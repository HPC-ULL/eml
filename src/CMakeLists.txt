
set(sources
        timer.c
        error.c
        configuration.c
        monitor.c
        data.c
        device.c
)

set(public_headers
        ../include/eml/error.h
        ../include/eml/data.h
        ../include/eml/device.h
)

add_library(eml SHARED ${sources})

option(ENABLE_DUMMY "Enable a dummy driver for testing")
if (ENABLE_DUMMY)
    target_compile_definitions(eml PUBLIC ENABLE_DUMMY)
endif()

cmake_dependent_option(ENABLE_MIC
        "Enable MIC support" ON
        "HAVE_MIC" OFF)
if (ENABLE_DUMMY)
    target_compile_definitions(eml PUBLIC ENABLE_DUMMY)
    set(sources ${sources} drivers/driver-dummy.c)
endif()

option(ENABLE_RAPL "Enable Intel RAPL support" ON)
if (ENABLE_RAPL)
    target_compile_definitions(eml PUBLIC ENABLE_RAPL)
    set(sources ${sources} drivers/driver-rapl.c)
endif()

cmake_dependent_option(ENABLE_NVML
        "Enable Nvidia NVML support" ON
        "HAVE_NVML" OFF)
if (ENABLE_NVML)
    target_compile_definitions(eml PUBLIC ENABLE_NVML)
    set(sources ${sources} drivers/driver-nvml.c)
endif()

option(ENABLE_SB_PDU "Enable Schleifenbauer PDU support" OFF)
if (ENABLE_SB_PDU)
    pkg_search_module(CRYPTO REQUIRED libcrypto)
    target_include_directories(eml SYSTEM PUBLIC ${CRYPTO_INCLUDE_DIRS})
    target_link_libraries(eml ${CRYPTO_LIBRARIES})
    target_compile_definitions(eml PUBLIC ENABLE_SB_PDU)
    set(sources ${sources} drivers/driver-sb-pdu.c)
endif()

option(ENABLE_ODROID "Enable Odroid INA231 Sensor support" OFF)
if (ENABLE_ODROID)
    target_compile_definitions(eml PUBLIC ENABLE_ODROID)
    set(sources ${sources} drivers/driver-odroid.c)
endif()

option(ENABLE_LABEE "Enable Labee support" OFF)
if (ENABLE_LABEE)
    pkg_search_module(LIBXML REQUIRED libxml-2.0)
    target_include_directories(eml SYSTEM PUBLIC ${LIBXML_INCLUDE_DIRS})
    target_link_libraries(eml ${LIBXML_LIBRARIES})
    target_compile_definitions(eml PUBLIC ENABLE_LABEE)
    set(sources ${sources} drivers/driver-labee.c)
endif()

option(ENABLE_PMLIB "Enable PMLib support" OFF)
if (ENABLE_PMLIB)
    target_compile_definitions(eml PUBLIC ENABLE_PMLIB)
    set(sources ${sources} drivers/driver-pmlib.c)
endif()

target_sources(eml PRIVATE ${sources})

set_target_properties(eml PROPERTIES
        VERSION ${PROJECT_VERSION}
        )

install(FILES ${public_headers}
        DESTINATION include/eml)

install(FILES ../include/eml.h
        DESTINATION include)


target_include_directories(eml PRIVATE ../include)
install(TARGETS eml
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(FILES ${CMAKE_BINARY_DIR}/eml.pc
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)

