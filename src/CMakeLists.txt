include_directories (simpleHttp ../inc)
add_executable (simpleHttp Request.cpp utility.cpp Response.cpp CBuffer.cpp Websocket.cpp WebSocketManager.cpp RequestTest.cpp ResponseTest.cpp sha1.c cencode.c ServerConnection.cpp MockServerConnection.cpp)
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
set_property(TARGET simpleHttp PROPERTY CXX_STANDARD 20)
target_compile_definitions(simpleHttp PRIVATE SIMPLE_HTTP_ESP_LOG_SUPPORT=0 SIMPLE_HTTP_RTOS_MODE=0)
add_test(NAME example_test COMMAND example)