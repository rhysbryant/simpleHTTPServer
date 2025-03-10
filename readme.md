# Simple and Fast HTTP Server For Embedded Devices
based on the the LWIP event driven API does need an an RTOS or any other framework.
but can work with freeRTOS if LWIP core locking is enabled

written a few years back and used in a number of my hobby projects.

a header file with embedded files to be served can be generated using
`genEmbeddedFiles.h.py`
# Examples #

## Simple ##

```cpp
#include "Server.h"
#include "Router.h"

//add a handler
SimpleHTTP::Router::addHandler("/", [](SimpleHTTP::Request *req, SimpleHTTP::Response *resp)
{
    resp->writeHeaderLine("Content-Type", "text/html");
    resp->write("<h1>Hello World</h1>");
});
//open port (may need to wait for the network to be up)
SimpleHTTP::Server::listen(80);

//in main loop (or within a task if using RTOS)
SimpleHTTP::Router::process();
```

## Websocket ##


```cpp
#include "Server.h"
#include "Router.h"
#include "WebSocketManager.h"
WebsocketManager::setFrameReceivedHandler([](Websocket *sock, SimpleHTTP::Websocket::Frame *frame)
{
    if( frame->frameType == Websocket::FrameTypeBin ){
        //process frame
    } 
});

//add a handler
SimpleHTTP::Router::addHandler("/ws", [](SimpleHTTP::Request *req, SimpleHTTP::Response *resp)
{
    auto origin = req->headers["ORIGIN"];
    if( origin.empty() || o != "http://expected-orign" ) {
        resp->writeHeader(SimpleHTTP::Response::NotFound);
        res->write("failed");
        return;
    }
    WebsocketManager::upgradeHandler(req,resp); 
});
//open port (may need to wait for the network to be up)
SimpleHTTP::Server::listen(80);

//in main loop (or within a task if using RTOS)
SimpleHTTP::Router::process();
SimpleHTTP::WebsocketManager::process();
```

## Embedded Files

first generate the header file

`python genEmbeddedFiles.h.py "*.js *.html" > files.h`

```cpp
#include "files.h"
#include "Server.h"
#include "Router.h"
#include "EmbeddedFiles.h"
//register the files array with the wrapper
SimpleHTTP::EmbeddedFilesHandler::addFiles(
(SimpleHTTP::EmbeddedFile *)files,
    sizeof(files) / sizeof(FileContent),(SimpleHTTP::EmbeddedFileType*)filesType);

//set the wrapper as the default handler
SimpleHTTP::Router::setDefaultHandler(SimpleHTTP::EmbeddedFilesHandler::embeddedFilesHandler);

//open port (may need to wait for the network to be up)
SimpleHTTP::Server::listen(80);

//in main loop (or within a task if using RTOS)
SimpleHTTP::Router::process();
```
## Config File Options

an config file `simpleHTTPServer.conf.h` needs to be created a level up from this directory.
example config follows

```c
//enable this running under freeRTOS enables use of locks
//LWIP_TCPIP_CORE_LOCKING also seperately be enabled in your lwip config
#define SIMPLE_HTTP_RTOS_MODE 0
//enable support for RTSP methods (DESCRIBE,SETUP,PLAY,PAUSE,TEARDOWN)
#define SIMPLE_HTTP_RTSP_SUPPORT 0
//enables use of ESP_LOG_LEVEL_LOCAL
#define SIMPLE_HTTP_ESP_LOG_SUPPORT 0
```
