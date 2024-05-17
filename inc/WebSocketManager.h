/*
 *  Copyright (c) 2023 Rhys Bryant
 *  Author Rhys Bryant
 *
 *	This file is part of SimpleHTTP
 *
 *   SimpleHTTP is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   any later version.
 *
 *   SimpleHTTP is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with SimpleHTTP.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#include "Websocket.h"

namespace SimpleHTTP
{
    //Manages the WebSock instances
    //there is a predefined pool and they are reused
    class WebsocketManager
    {
    public:
        typedef void (*FrameReceivedHandler)(SimpleHTTP::Websocket *socket, SimpleHTTP::Websocket::Frame *frame);

    private:
        static const int poolSize = 5;
        static Websocket connections[poolSize];
        static Websocket connectionBufferLock[poolSize];
        
        static int nextFreeClientIndex();
        static int acceptKey(string clientKey, char *outputBuffer);
        static Result dataReceivedHandler(void *arg, uint8_t *data, uint16_t len);
        static FrameReceivedHandler frameReceivedHandler;

    public:
        static const int pingInterval = 15000;
        static void process();

        static void upgradeHandler(Request *req, Response *resp);

        static void writeFrameToAll(Websocket::FrameType frameType, char* headerExta, const uint16_t headerExtraSize, char* payload, int size);

        static inline void setFrameReceivedHandler(FrameReceivedHandler frh){
            frameReceivedHandler = frh;
        }
    };
}