target_sources(${PROJECT_NAME}
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/device_twin.c
    ${CMAKE_CURRENT_LIST_DIR}/device_twin.h
    ${CMAKE_CURRENT_LIST_DIR}/direct_methods.c
    ${CMAKE_CURRENT_LIST_DIR}/direct_methods.h
    ${CMAKE_CURRENT_LIST_DIR}/font.h
    ${CMAKE_CURRENT_LIST_DIR}/iotConnect.c
    ${CMAKE_CURRENT_LIST_DIR}/iotConnect.h
    ${CMAKE_CURRENT_LIST_DIR}/m4_support.c
    ${CMAKE_CURRENT_LIST_DIR}/m4_support.h
    ${CMAKE_CURRENT_LIST_DIR}/uart_support.c
    ${CMAKE_CURRENT_LIST_DIR}/uart_support.h
    ${CMAKE_CURRENT_LIST_DIR}/deferred_updates.c
    ${CMAKE_CURRENT_LIST_DIR}/deferred_updates.h
)