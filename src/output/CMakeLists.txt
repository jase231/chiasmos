file(GLOB SRC CONFIGURE_DEPENDS *.cpp)
add_library(chiasmos_output STATIC ${SRC})
target_link_libraries(chiasmos_output PUBLIC chiasmos_query nlohmann_json::nlohmann_json)
