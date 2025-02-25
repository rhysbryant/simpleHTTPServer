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
#include "common.h"
#include <string.h>
#include "ServerConnection.h"

using std::string;

namespace SimpleHTTP {
	//HTTP/1.1 Response Generation
	class Response {
	public:
		enum ConnectionMode{
			ConnectionKeepAlive,
			ConnectionUpgrade,
			ConnectionClose
		};
	private:
		//End of line char sequence
		static const constexpr char EOL[] = { '\r','\n' };

		static const int responseBufferSize = 512;
		static const int responseBufferHeadersReservedSize = 256;
		//mainly for chunked transfer where EOL is appended to the buffer before sending
		static const int responseBufferBodyReservedSize = sizeof(EOL);

		char responseBuffer[responseBufferSize];
		char* responseHeaderBufferPos;
		char* responseBufferBodyStart;
		char* responseBufferPos;
		const char* responseBufferEnd = responseBuffer + responseBufferSize;
		int responseSizeTotal;

		Result networkWrite(char* data, int length);

		static const constexpr struct SimpleString VersionString = SIMPLE_STR("HTTP/1.1 ");
		static const constexpr struct SimpleString ChunckedTransferHeader = SIMPLE_STR("Transfer-Encoding: chunked");
		static const constexpr struct SimpleString ConnectionKeepAliveHeader = SIMPLE_STR("Keep-Alive: timeout=15, max=1000");
		static const constexpr struct SimpleString ConnectionCloseHeader = SIMPLE_STR("Connection: close");
		static const constexpr struct SimpleString ConnectionUpgradeHeader = SIMPLE_STR("Connection: Upgrade");
		static const constexpr struct SimpleString ContentLengthHeader = SIMPLE_STR("Content-Length: ");

		//max number of hex chars + EOL
		static const int ChunkedTransferSizeHeaderSize = 20 + sizeof(EOL);


		static const constexpr struct SimpleString statusStrings[] = {
		   SIMPLE_STR("200 OK"),
		   SIMPLE_STR("101 Switching Protocols"),
		   SIMPLE_STR("404 Not Found"),
		   SIMPLE_STR("400 Bad Request"),
		   SIMPLE_STR("500 Internal Server Error")
		};

		bool headersSent;
		bool statusWritten;
		bool chunkedEncoding;
		HTTPVersion responseVersion;
		ConnectionMode connectionMode;

		ServerConnection* client;

		Result flush(bool finalize);
		/**
		 * writes the default status if no status has been written yet
		 */
		bool ensureStatusWritten();
		
		bool appendHeaders(char* headers, int size);

		bool appendHeadersEOL();
		/**
		* adds to the revered space between the headers and body
		* used for example to add the chunk size
		* returns false and makes no change if insuffisant space
		* this method moves the body start pointer backwoods
		* there for each call appends data to the left
		**/
		bool appendBodyPrefix(char* body, int size);

		bool appendBody(char* body, int size);

	public:

		Response(ServerConnection* conn, bool connectionKeepAlive,HTTPVersion requestVersion);

		enum Status : int {
			Ok = 0,
			SwitchingProtocol,
			NotFound,
			BadRequest,
			InternalServerError
		};

		/**
		 * adds a content length header to the buffer
		 * don't call this more then once
		 */
		void addContentLengthHeader(int length);
		/*
		 * writes a response header line to the buffer
		 * calls to this method after the headers have already been sent are ignored
		 */
		bool writeHeaderLine(const char* name, int size);
		/*
		 * writes a response header line to the buffer
		 * calls to this method after the headers have already been sent are ignored
		 */
		bool writeHeaderLine(const SimpleString str);
		/*
		 * writes a response header line to the buffer
		 * calls to this method after the headers have already been sent are ignored
		 */
		bool writeHeaderLine(const char* headerName, const char* value);
		/*
		* writes the status line to the buffer
		* if the is not called before write() 200 OK is implicitly written
		* this can only be called once.
		*/
		bool writeHeader(Status status);
		/**
		* write request body data to the buffer
		* returns the count of bytes written
		**/
		int write(const char* data, int length);
		/**
		* write request body data to the buffer
		* this expects a null terminated string
		* returns the count of bytes written
		**/
		int write(const char* data);
		/**
		 * writes directly to the network without buffering
		 * this method flushes the buffer before it's starts writing
		 * 
		 * this method does not block, it's expected the pointer will be always be valid
		 */
		int writeDirect(const char* data, int length);
		/**
		* writes out the buffer to the network
		**/
		Result flush();
		/**
		* in the case of chunked transfer encoding sends the final chunk and the no more chunks marker
		**/
		inline int finalize() { return flush(true); }
		/**
		 * returns the underlying server connection object and flags the connection as hijacked this means it stops parsing the incoming data as a http request
		 */
		ServerConnection* hijackConnection();
		/**
		 * retrieve the argument to be persisted between reuses of the same tcp connection
		 * set with setSessionArg()
		 */
		void* getSessionArg() { return client->sessionArg; }
		/**
		 * set an argument to be persisted between reuses of the same tcp connection
		 */
		void setSessionArg(void* arg) { client->sessionArg = arg; }
		/**
		* called if SessionArg is non 0 and the connection is closed or disposed of
		*/
		void setSessionArgFreeHandler(ServerConnection::SessionArgFree h) { client->sessionArgFreeHandler = h; }

		/**
		 * returns the count of bytes sent
		 */
		inline int getResponseSizeSent() { return responseSizeTotal;}

		inline void setConnectionMode(ConnectionMode connMode) { connectionMode=connMode; }

		inline ConnectionMode getConnectionMode() { return connectionMode; }

		inline bool getRemoteIPAddress(char *buf, int buflen){
			return client->getRemoteIPAddress(buf,buflen);
		}
	};
};
