# one static lib per module, each globbing its *.cpp
# dependency order: core < normalize < index < build, query < output < cli/server.
add_subdirectory(core)
add_subdirectory(normalize)
add_subdirectory(index)
add_subdirectory(build)
add_subdirectory(query)
add_subdirectory(output)
if(CHIASMOS_SERVER)
  add_subdirectory(server)
endif()
add_subdirectory(cli)
