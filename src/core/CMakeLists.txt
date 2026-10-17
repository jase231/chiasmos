file(GLOB SRC CONFIGURE_DEPENDS *.cpp)
add_library(chiasmos_core STATIC ${SRC})
target_link_libraries(chiasmos_core PUBLIC chiasmos_flags)
