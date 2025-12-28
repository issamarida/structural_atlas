add_executable(test_math test_math.c)
target_link_libraries(test_math m)
add_test(NAME pk_rk4_math COMMAND test_math)

add_executable(test_geometry test_geometry.c)
target_link_libraries(test_geometry m)
add_test(NAME pk_rk4_geometry COMMAND test_geometry)

add_executable(test_projection test_projection.c)
target_link_libraries(test_projection m)
add_test(NAME pk_rk4_projection COMMAND test_projection)
