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
#include "esp_log.h"
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

				if( ! client->currentRequest.isBodyReadInProgress() ){
					resp.finalize();
                
					client->currentRequest.reset();
					client->lastRequestTime = os_getUnixTime();
					if (resp.getConnectionMode() == Response::ConnectionClose)
					{
						client->closeOnceSent = resp.getResponseSizeSent();
					}
				}else{
					resp.flush();
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

ServerConnection* Router::getFreeConnection() {
	for (int i = 0; i < maxClientConnections; i++)
	{
		if (!clients[i].isConnected())
		{
            return &clients[i];
        }
    }
    for (int i = 0; i < maxClientConnections; i++)
	{
		if (clients[i].isConnected())
		{
            ESP_LOGE(__FUNCTION__,"client %d",(int)clients[i].hijacted);
        }
    }
    return nullptr;
}

ServerConnection Router::clients[];
std::map<string, RequestHandler> Router::handlers;
RequestHandler Router::defaultHandler = Router::internalDefaultHandler;