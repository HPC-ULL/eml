
add_executable(eml-consumed consumed.c)
target_link_libraries(eml-consumed eml)

find_package(Threads REQUIRED)
find_package(Threads REQUIRED)

install(TARGETS eml-consumed DESTINATION bin)

if (ENABLE_SB_PDU)
    target_link_libraries(eml-consumed ${CRYPTO_LIBRARIES})
endif()

if (ENABLE_LABEE)
    target_link_libraries(eml-consumed ${LIBXML_LIBRARIES})
endif()

target_link_libraries(eml-consumed ${CONFUSE_LIBRARIES})
target_link_libraries(eml-consumed Threads::Threads)
target_link_libraries(eml-consumed m)


