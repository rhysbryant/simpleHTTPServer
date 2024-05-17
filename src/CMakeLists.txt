include_directories (simpleHttp ../inc)
add_executable (simpleHttp Request.cpp utility.cpp Response.cpp Websocket.cpp RequestTest.cpp ResponseTest.cpp sha1.c cencode.c ServerConnection.cpp)
include(FetchContent)
FetchContent_Declare(
  googletest
  # Specify the commit you depend on and update it regularly.
  URL https://github.com/google/googletest/archive/5376968f6948923e2411081fd9372e71a59d8e77.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Now simply link against gtest or gtest_main as needed. Eg
target_link_libraries(simpleHttp gtest_main)
add_test(NAME example_test COMMAND example)