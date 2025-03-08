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
#include "WebSocketManager.h"
#include "Request.h"
#include "Response.h"
#include "log.h"
#include "libsha1.h"
extern "C" {
#include "cencode.h"
}

using namespace SimpleHTTP;

void WebsocketManager::upgradeHandler(Request* req, Response* resp)
{
	auto connHeader = req->headers["CONNECTION"];
	if ( connHeader.find("Upgrade") == -1) {
		resp->writeHeader(Response::BadRequest);
		return;
	}

	auto keyHeader = req->headers["SEC-WEBSOCKET-KEY"];
	if (keyHeader.empty()) {
		resp->writeHeader(Response::BadRequest);
		return;
	}

	int wsIndex = nextFreeClientIndex();
	if( wsIndex == -1){
		resp->writeHeader(Response::InternalServerError);
		return;
	}

	resp->writeHeader(Response::SwitchingProtocol);

	const int headerNameSize = sizeof("Sec-WebSocket-Accept: ") - 1;
	char buffer[100] = "";

	int len = acceptKey(keyHeader, buffer);
	buffer[len] = 0;

	resp->writeHeaderLine("Sec-WebSocket-Accept",buffer);
	resp->writeHeaderLine(SIMPLE_STR("Upgrade: websocket"));
	resp->setConnectionMode(Response::ConnectionUpgrade);

	auto client = resp->hijackConnection();
	//setup the mapping from ServerConnection to the WebSocket and back
	
	connections[wsIndex].assign(client);
    client->dataReceivedArg = &connections[wsIndex];
	client->dataReceived = dataReceivedHandler;
	connections[wsIndex].lastPingSent =  os_getUnixTime();
	
}

int WebsocketManager::acceptKey(string clientKey, char* outputBuffer)
{
	uint8_t sha1HashBin[20] = { 0 };
	clientKey += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	sha1_ctx ctx;
	sha1_begin(&ctx);
	sha1_hash((const unsigned char*)clientKey.c_str(), clientKey.length(), &ctx);
	sha1_end(&sha1HashBin[0], &ctx);

	base64_encodestate b64State;

	base64_init_encodestate(&b64State);
	int length = base64_encode_block((const char*)sha1HashBin, 20, outputBuffer, &b64State);
	length += base64_encode_blockend(outputBuffer + length, &b64State);
	//string key = base64_encode(sha1HashBin, 20);


	return length - 1;//trailing new line
}

Result WebsocketManager::dataReceivedHandler(void* arg, uint8_t* data, uint16_t len){
	SHTTP_LOGI(__FUNCTION__,"got dataReceivedHandler: %d bytes",len);
    auto ws = static_cast<Websocket*>(arg);

	if( data == 0 && len == 0){
		ws->unAssign();
		return OK;
	}

    if (!ws->bufferLock())
    {
		SHTTP_LOGE(__FUNCTION__,"buffer lock failed");
        return ERROR;
    }
    
	ws->dataReceivedHandler(data,len);
    ws->bufferUnLock();

	return OK;
}

void WebsocketManager::writeFrameToAll(Websocket::FrameType frameType, const Websocket::Payload* payload) {
	for (int i = 0; i < poolSize; i++) {
		if (connections[i].isInUse()) {
			connections[i].writeFrame(frameType, payload);
		}
	}
}

int WebsocketManager::nextFreeClientIndex(){
	for(int i=0;i<poolSize;i++){
		if( ! connections[i].isInUse()){
			return i;
		}
	}
	return -1;
}

void WebsocketManager::process(){
	for(int i=0;i< poolSize;i++){
		if( connections[i].isInUse() ){
			auto ws = &connections[i];
			if (!ws->bufferLock()){
				return;
			}
			uint8_t buffer[1024];
			Websocket::Frame f;
			f.payload = buffer;
			f.payloadLength = sizeof(buffer);
			auto gotMessage = ws->nextFrame(&f) == OK;

			ws->bufferUnLock();
            
			if ( gotMessage ){


				frameReceivedHandler(&connections[i],&f);
				//frames that require echoing back the payload
				if( f.frameType == Websocket::FrameTypeConnectionClose ){
					if( !ws->isCloseRequestedByServer() ){
						Websocket::Payload closePayload{ (uint8_t*)f.payload,f.payloadLength,false,nullptr };
						ws->writeFrame(f.frameType, &closePayload);
					}
				}else if( f.frameType == Websocket::FrameTypePing ){
					Websocket::Payload pongPayload{ (uint8_t*)f.payload,f.payloadLength,false,nullptr };
					ws->writeFrame(Websocket::FrameTypePong, &pongPayload);
				}else if(f.frameType == Websocket::FrameTypePong){
					ws->lastPongReceived = os_getUnixTime();
				}

			}else if( false || os_getUnixTime() - ws->lastPingSent > 15000 ){

				if( ws->lastPongReceived != 0 && os_getUnixTime() -  ws->lastPongReceived > 60000 ){
					ws->getConnection()->closeWithOutLocking();
					return;
				}

				if( ws->writeFrame(Websocket::FrameTypePing,nullptr) == ERROR ) {
					SHTTP_LOGE(__FUNCTION__,"closing due to ping error");
					ws->getConnection()->close();
				}
				ws->lastPingSent = os_getUnixTime();
			}else if( ws->lastPongReceived != 0 &&  os_getUnixTime() - ws->lastPingSent > 30000 && !ws->isCloseRequestedByServer() ){
				SHTTP_LOGE(__FUNCTION__,"pong timeout %d %d",(int)ws->lastPingSent ,(int) ws->lastPongReceived );
				ws->sendCloseFrame(66);
			}

			
		}
	}
}

Websocket WebsocketManager::connections[poolSize];
WebsocketManager::FrameReceivedHandler WebsocketManager::frameReceivedHandler=0;
