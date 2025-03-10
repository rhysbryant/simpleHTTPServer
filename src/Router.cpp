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
#include "Router.h"
#include "log.h"
using namespace SimpleHTTP;

void Router::addHandler(string path, RequestHandler handler)
{
	handlers[path] = handler;
}

void Router::internalDefaultHandler(Request *req, Response *resp)
{
	string html = "<html><body> path was not found</body></html>";
	resp->writeHeader(Response::NotFound);
	resp->writeHeaderLine(SIMPLE_STR("Content-Type: text/html"));
	resp->write(html.c_str(), html.size());
}

void Router::setDefaultHandler(RequestHandler handler)
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

void Router::process()
{
	auto connCountInUse = getConnectionsInUseCount();
	if( lastConnectionsInUse != connCountInUse){
		SHTTP_LOGI(__FUNCTION__,"%d connections in use",connCountInUse);
		lastConnectionsInUse = connCountInUse;
	}
	for (int i = 0; i < maxClientConnections; i++)
	{
		if (clients[i].isConnected())
		{
			auto client = &clients[i];
			if (client->currentRequest.getAndClearForProcessing())
			{

				bool connectionKeepAlive = false;
				auto connHeader = client->currentRequest.headers["CONNECTION"];
				if (connHeader == "keep-alive" 
				#if defined(SIMPLE_HTTP_RTSP_SUPPORT) && SIMPLE_HTTP_RTSP_SUPPORT == 1
					//RTSP is keepalive by default
					|| client->currentRequest.version == HTTPVersion::RTSP10
				#endif
				){
					connectionKeepAlive = true;
				}

				Response resp(client, connectionKeepAlive,client->currentRequest.version);
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

				if( ! client->currentRequest.isBodyReadInProgress() ){
					resp.finalize();
                
					client->currentRequest.reset();
					client->lastRequestTime = os_getUnixTime();
					if (resp.getConnectionMode() == Response::ConnectionClose)
					{
						client->closeOnceSent = resp.getResponseSizeSent();
					}
				}else{
					//TODO allow data to be written while a body receive is in progress
					//resp.flush();
				}
			}
			else
			{
				if (!client->hijacted && client->lastRequestTime != 0 && os_getUnixTime() - client->lastRequestTime > KeepaliveTimeout)
				{
					SHTTP_LOGI(__FUNCTION__,"closing http connection");
					client->close();
				}
			}
		}
	}
}

ServerConnection* Router::getFreeConnection() {
	for (int i = 0; i < maxClientConnections; i++)
	{
		if (!clients[i].isConnected())
		{
            return &clients[i];
        }
    }
    return nullptr;
}

int Router::getConnectionsInUseCount() {
	int count =0;
	for (int i = 0; i < maxClientConnections; i++)
	{
		if (clients[i].isConnected())
		{
            count++;
        }
    }
	return count;
}

ServerConnection Router::clients[];
std::map<string, RequestHandler> Router::handlers;
RequestHandler Router::defaultHandler = Router::internalDefaultHandler;
int Router::lastConnectionsInUse = 0;

