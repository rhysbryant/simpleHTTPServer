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
#include <string>
#include <map>
#include "Request.h"
#include "Response.h"
#include "ServerConnection.h"
#include <stdint.h>
extern "C"{
#include "lwip/tcp.h"
}
using std::string;

namespace SimpleHTTP{
    typedef void (*RequestHandler) (Request* request,Response* response);
    class Server{
        private:
            static std::map<string,RequestHandler> handlers;
            static RequestHandler defaultHandler;

            static void internalDefaultHandler(Request* request,Response* response);

            static const int maxClientConnections = 10;
			static ServerConnection clients[maxClientConnections];
			static struct tcp_pcb* tcpServer;
            static const uint32_t KeepaliveTimeout = 60 * 1000;

            static err_t tcp_sent_cb(void* arg, struct tcp_pcb* tpcb, u16_t len);
            static err_t tcp_recv_cb(void* arg, struct tcp_pcb* tpcb, struct pbuf* p, err_t err);
            static err_t tcp_accept_cb(void* arg, struct tcp_pcb* newpcb, err_t err);
            static void tcp_err_cb(void* arg, err_t err);

        public:
            /**
             * add URL path to handler (callback function) mapping
            */
            static void addHandler(string path,RequestHandler handler);
            /**
             * the handler to use when the path is not found in the handles map
             * pass null to restore the default
             */
            static void setDefaultHandler(RequestHandler handler);

            static void listen(int port);

            static void process();

    };
};
