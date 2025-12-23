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
#include "Server.h"
#include "Router.h"
#include <queue>
#if defined(SIMPLE_HTTP_RTOS_MODE) && SIMPLE_HTTP_RTOS_MODE == 1
#include <freertos/semphr.h>
#endif


using namespace SimpleHTTP;


err_t Server::tcp_accept_cb(void* arg, struct tcp_pcb* newpcb, err_t err)
{

	auto conn = Router::getFreeConnection();
	if (conn == 0) {
		tcp_abort(newpcb);
		return ERR_ABRT;
	}

	tcp_arg(newpcb, conn);
	tcp_err(newpcb, tcp_err_cb);
	tcp_sent(newpcb, tcp_sent_cb);
	tcp_recv(newpcb, tcp_recv_cb);
	conn->init(newpcb);
	return ERR_OK;

}

err_t Server::tcp_recv_cb(void* arg, struct tcp_pcb* tpcb, struct pbuf* p,
	err_t err)
{
	if (arg != 0)
	{
		ServerConnection* conn = (ServerConnection*)arg;
		if (p == 0)
		{
			conn->closeWithOutLocking();
			conn->init(0);
			return ERR_OK;
		}

		if (conn->dataReceived)
		{
			auto pack = p;
			while (pack) {
				conn->dataReceived(conn->dataReceivedArg, (uint8_t*)pack->payload, pack->len);
				pack = p->next;
			}
		}
		else
		{
			// default to HTTP/1.1 request
			auto result = conn->currentRequest.parse((char*)p->payload, p->len);
			if (result == ERROR)
			{
				conn->closeWithOutLocking();
				SHTTP_LOGE(__FUNCTION__, "Failed to parse request, closing connection");
			}
		}

		tcp_recved(tpcb, p->len);
		pbuf_free(p);
#if	defined(SIMPLE_HTTP_RTOS_MODE) && SIMPLE_HTTP_RTOS_MODE == 1
		// Signal the semaphore to unblock waitOnData()
		if (dataReceivedSem != nullptr) {
			xSemaphoreGive(dataReceivedSem);
		}
#endif
		err_t result = ERR_OK; // conn->recv_cb(p, err);
		return result;
	}

	return ERR_OK;
}

err_t Server::tcp_sent_cb(void* arg, struct tcp_pcb* tpcb, u16_t len)
{
	if (arg != 0)
	{
		ServerConnection* conn = (ServerConnection*)arg;

		conn->sendCompleteCallback(len);

		if (conn->closeOnceSent)
		{
			if (!conn->closeOnceSent && !conn->waitingForSendComplete())
			{
				conn->closeWithOutLocking();
			}
		}

	}

	return ERR_OK;
}

void Server::tcp_err_cb(void* arg, err_t err)
{
	if (arg != 0)
	{
		ServerConnection* conn = (ServerConnection*)arg;
		if (conn->sessionArg != 0 && conn->sessionArgFreeHandler != 0)
		{
			conn->sessionArgFreeHandler(conn->sessionArg);
		}

		conn->dataReceived(conn->dataReceivedArg, 0, 0);
		conn->init(0);
	}
}

void Server::listen(int port)
{

	struct tcp_pcb* tmp = tcp_new();
	tcp_bind(tmp, IP4_ADDR_ANY, port);
	tcpServer = tcp_listen(tmp);
	tcp_accept(tcpServer, tcp_accept_cb);
	return;
}


void Server::waitOnData() {
	SHTTP_LOGD("Server", "Waiting for data...");
#if defined(SIMPLE_HTTP_RTOS_MODE) && SIMPLE_HTTP_RTOS_MODE == 1
	if (dataReceivedSem == nullptr) {
		return;
	}

	// Block until tcp_recv_cb is next called
	xSemaphoreTake(dataReceivedSem, pdMS_TO_TICKS(1000));

#endif
}

struct tcp_pcb* Server::tcpServer = 0;
SemaphoreHandle_t Server::dataReceivedSem = xSemaphoreCreateBinary();