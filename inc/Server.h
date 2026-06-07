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
extern "C"{
#include "lwip/tcp.h"
}
#include "common.h"

namespace SimpleHTTP{
    class Server{
        private:
			static struct tcp_pcb* tcpServer;
            #if defined(SIMPLE_HTTP_RTOS_MODE) && SIMPLE_HTTP_RTOS_MODE == 1
            static SemaphoreHandle_t dataReceivedSem;
            #endif

            static err_t tcp_sent_cb(void* arg, struct tcp_pcb* tpcb, u16_t len);
            static err_t tcp_recv_cb(void* arg, struct tcp_pcb* tpcb, struct pbuf* p, err_t err);
            static err_t tcp_accept_cb(void* arg, struct tcp_pcb* newpcb, err_t err);
            static err_t tcp_poll_cb(void* arg, struct tcp_pcb* tpcb);
            static void tcp_err_cb(void* arg, err_t err);

        public:
            // optional hook fired on every TCP ACK (from tcp_sent_cb) so an
            // external producer can wake without the library depending on it.
            static void (*sendCompleteNotify)();

            static void waitOnData();
            static void listen(int port);

    };
};
