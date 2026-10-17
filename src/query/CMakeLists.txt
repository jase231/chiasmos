file(GLOB SRC CONFIGURE_DEPENDS *.cpp)
add_library(chiasmos_query STATIC ${SRC})
target_link_libraries(chiasmos_query PUBLIC chiasmos_index chiasmos_normalize chiasmos_core)
