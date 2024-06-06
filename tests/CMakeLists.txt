add_executable(test test_pi.cpp)
target_link_libraries(test PRIVATE tpexecutor)
target_link_libraries(test PRIVATE compiler_flags)

