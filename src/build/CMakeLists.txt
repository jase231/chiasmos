file(GLOB SRC CONFIGURE_DEPENDS *.cpp)
add_library(chiasmos_build STATIC ${SRC})
target_link_libraries(chiasmos_build PUBLIC chiasmos_index chiasmos_normalize chiasmos_core chiasmos_osmium)
