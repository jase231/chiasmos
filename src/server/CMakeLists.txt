file(GLOB SRC CONFIGURE_DEPENDS *.cpp)
add_library(chiasmos_server STATIC ${SRC})
target_link_libraries(chiasmos_server PUBLIC chiasmos_output Boost::headers Threads::Threads)
