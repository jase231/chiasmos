file(GLOB SRC CONFIGURE_DEPENDS *.cpp)
add_library(chiasmos_normalize STATIC ${SRC})
target_link_libraries(chiasmos_normalize PUBLIC chiasmos_core)
