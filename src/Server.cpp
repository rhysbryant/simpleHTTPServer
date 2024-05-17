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
#include <queue>

using namespace SimpleHTTP;

void Server::addHandler(string path, RequestHandler handler)
{
	handlers[path] = handler;
}

err_t Server::tcp_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err)
{

	for (int i = 0; i < maxClientConnections; i++)
	{
		if (!clients[i].isConnected())
		{
			tcp_arg(newpcb, &clients[i]);
			tcp_err(newpcb, tcp_err_cb);
			tcp_sent(newpcb, tcp_sent_cb);
			tcp_recv(newpcb, tcp_recv_cb);
			clients[i].init(newpcb);
			return ERR_OK;
		}
	}

	tcp_abort(newpcb);
	return ERR_ABRT;
}

err_t Server::tcp_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p,
						  err_t err)
{
	if (arg != 0)
	{
		ServerConnection *conn = (ServerConnection *)arg;
		if (p == 0)
		{
			conn->closeWithOutLocking();
			conn->init(0);
			return ERR_OK;
		}

		if (conn->dataReceived)
		{
			conn->dataReceived(conn->dataReceivedArg, (uint8_t *)p->payload, p->len);
		}
		else
		{
			// default to HTTP/1.1 request
			auto result = conn->currentRequest.parse((char *)p->payload, p->len);
			if (result == ERROR)
			{
				conn->closeWithOutLocking();
			}
		}

		tcp_recved(tpcb, p->len);
		pbuf_free(p);
		err_t result = ERR_OK; // conn->recv_cb(p, err);
		return result;
	}

	return ERR_OK;
}

err_t Server::tcp_sent_cb(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
	if (arg != 0)
	{
		ServerConnection *conn = (ServerConnection *)arg;

		if (conn->closeOnceSent)
		{
			conn->closeOnceSent -= len;
			if (!conn->closeOnceSent)
			{
				conn->closeWithOutLocking();
			}
		}

		conn->sendCompleteCallback(len);
	}

	return ERR_OK;
}

void Server::tcp_err_cb(void *arg, err_t err)
{
	if (arg != 0)
	{
		ServerConnection *conn = (ServerConnection *)arg;
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

	struct tcp_pcb *tmp = tcp_new();
	tcp_bind(tmp, IP4_ADDR_ANY, port);
	tcpServer = tcp_listen(tmp);
	tcp_accept(tcpServer, tcp_accept_cb);
	return;
}

void Server::internalDefaultHandler(Request *req, Response *resp)
{
	string html = "<html><body> path was not found</body></html>";
	resp->writeHeader(Response::NotFound);
	resp->writeHeaderLine(SIMPLE_STR("Content-Type: text/html"));
	resp->write(html.c_str(), html.size());
}

void Server::setDefaultHandler(RequestHandler handler)
{
	if (handler)
	{
		defaultHandler = handler;
	}
	else
	{
		defaultHandler = internalDefaultHandler;
	}
}

void Server::process()
{
	for (int i = 0; i < maxClientConnections; i++)
	{
		if (clients[i].isConnected())
		{
			auto client = &clients[i];
			if (client->currentRequest.receivedAllHeaders())
			{

				bool connectionKeepAlive = false;
				auto connHeader = client->currentRequest.headers["CONNECTION"];
				if (connHeader == "keep-alive")
				{
					connectionKeepAlive = true;
				}

				Response resp(client, connectionKeepAlive);
				auto path = client->currentRequest.path;

				auto h = handlers[path];
				if (h == 0)
				{
					defaultHandler(&client->currentRequest, &resp);
				}
				else
				{
					h(&client->currentRequest, &resp);
				}

				resp.finalize();
				client->flushData();
				client->currentRequest.reset();
				client->lastRequestTime = os_getUnixTime();
				if (resp.getConnectionMode() == Response::ConnectionClose)
				{
					client->closeOnceSent = resp.getResponseSizeSent();
				}
			}
			else
			{
				if (!client->hijacted && client->lastRequestTime != 0 && os_getUnixTime() - client->lastRequestTime > KeepaliveTimeout)
				{
					client->close();
				}
			}
		}
	}
}

struct tcp_pcb *Server::tcpServer = 0;
ServerConnection Server::clients[];
std::map<string, RequestHandler> Server::handlers;
RequestHandler Server::defaultHandler = Server::internalDefaultHandler;