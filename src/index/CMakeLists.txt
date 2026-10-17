file(GLOB SRC CONFIGURE_DEPENDS *.cpp)
add_library(chiasmos_index STATIC ${SRC})
target_link_libraries(chiasmos_index PUBLIC chiasmos_normalize chiasmos_core)
