# CMakeList.txt : CMake project for CMakeProject1, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)
#if(!WIN32)
 
idf_component_register(SRCS src/Router.cpp src/Server.cpp src/SecureServer.cpp src/SecureServerConnection.cpp src/Request.cpp src/utility.cpp src/Response.cpp src/Websocket.cpp src/sha1.c src/cencode.c src/ServerConnection.cpp src/WebSocketManager.cpp src/EmbeddedFiles.cpp src/CBuffer.cpp
                       INCLUDE_DIRS "inc/" REQUIRES mbedtls)
                    
#else()
#    add_subdirectory(src)

#endif()