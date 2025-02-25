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
#include "Response.h"
#include "utility.h"

using namespace SimpleHTTP;

Response::Response(ServerConnection* conn, bool connectionKeepAlive,HTTPVersion requestVersion) {
	responseHeaderBufferPos = responseBuffer;
	responseBufferBodyStart = responseBuffer + responseBufferHeadersReservedSize;
	responseBufferPos = responseBufferBodyStart;
	responseSizeTotal = 0;
	headersSent = false;
	statusWritten = false;
	chunkedEncoding = true;
	connectionMode = connectionKeepAlive ? ConnectionKeepAlive : ConnectionClose;
	client = conn;
	responseVersion = requestVersion;
}

bool Response::ensureStatusWritten() {
	if (!statusWritten) {
		return writeHeader(Status::Ok);
	}
	return true;
}

bool Response::appendHeaders(char* headers, int size) {
	if (responseHeaderBufferPos + size >= responseBufferBodyStart) {
		//ran out of header space if no body data has been written yet
		//can just move the body starting pos
		if (responseBufferBodyStart == responseBufferPos && responseHeaderBufferPos + size < responseBufferEnd) {
			responseBufferBodyStart += size;
			responseBufferPos += size;
		}
		else {
			return false;
		}
	}

	memcpy(responseHeaderBufferPos, headers, size);
	responseHeaderBufferPos += size;
	return true;
}

bool Response::appendHeadersEOL() {
	return appendHeaders((char*)EOL, sizeof(EOL));
}

bool Response::appendBodyPrefix(char* data, int size) {

	if (responseBufferBodyStart - size <= responseHeaderBufferPos) {
		return false;
	}

	responseBufferBodyStart -= size;
	memcpy(responseBufferBodyStart, data, size);
	return true;
}

bool Response::appendBody(char* body, int size) {
	if (responseBufferPos + size > responseBufferEnd) {
		return false;
	}

	memcpy(responseBufferPos, body, size);
	responseBufferPos += size;
	return true;
}

int Response::write(const char* data, int length) {

	if (!statusWritten) {
		writeHeader(Status::Ok);
		statusWritten = true;
	}

	int remainingDataLength = length;
	while (remainingDataLength > 0) {
		int avBuffer = (responseBufferEnd - responseBufferPos);
		//account for EOL required for chunked transfer
		if (chunkedEncoding) {
			if (avBuffer > 2) {
				avBuffer -= sizeof(EOL);
			}
			else {
				avBuffer = 0;
			}
		}

		int lengthToCopy = remainingDataLength;

		if (remainingDataLength > avBuffer) {
			lengthToCopy = avBuffer;
		}

		if (!appendBody((char*)data, lengthToCopy)) {
			return length - remainingDataLength;
		}

		data += lengthToCopy;
		remainingDataLength -= lengthToCopy;

		if (remainingDataLength > 0) {
			if (flush() != OK) {
				return length - remainingDataLength;
			}
		}
	}

	return 0;
}

int Response::write(const char* data) {
	return write(data, strlen(data));
}

int Response::writeDirect(const char* data, int length) {
	int result = flush();
	if (result != 0) {
		return result;
	}

	if (client->writeData((uint8_t*)data, length, ServerConnection::WriteFlagZeroCopy)) {
		responseSizeTotal += length;
		return OK;
	}

	return ERROR;
}

bool Response::writeHeader(Response::Status status) {
	if (headersSent || statusWritten) {
		return false;
	}

	statusWritten = true;

	auto strStatus = statusStrings[status];
	auto strVersion = HTTPVersions[responseVersion];

	return appendHeaders((char*)strVersion.value, strVersion.size)
		&& appendHeaders(" ",1)
		&& writeHeaderLine(strStatus.value, strStatus.size);

}

bool Response::writeHeaderLine(const SimpleString str) {
	return writeHeaderLine(str.value, str.size);
}

void Response::addContentLengthHeader(int length) {
	ensureStatusWritten();

	memcpy(responseHeaderBufferPos, ContentLengthHeader.value, ContentLengthHeader.size);
	responseHeaderBufferPos += ContentLengthHeader.size;

	auto lengthSize = Utility::toASCII(length, responseHeaderBufferPos, Utility::DecBase, 10);
	responseHeaderBufferPos += lengthSize;


	memcpy(responseHeaderBufferPos, EOL, sizeof(EOL));
	responseHeaderBufferPos += sizeof(EOL);

	chunkedEncoding = false;
}

bool Response::writeHeaderLine(const char* name, int size) {
	return ensureStatusWritten()
		&& appendHeaders((char*)name, size)
		&& appendHeadersEOL();

}

bool Response::writeHeaderLine(const char* headerName, const char* value) {

	int headerNameSize = strlen(headerName);
	int headerValueSize = strlen(value);

	return ensureStatusWritten()
		&& appendHeaders((char*)headerName, headerNameSize)
		&& appendHeaders(": ", 2)
		&& appendHeaders((char*)value, headerValueSize)
		&& appendHeadersEOL();
}

Result Response::flush() {
	return flush(false);
}

Result Response::flush(bool finalise) {

	int chunkSize = responseBufferPos - responseBufferBodyStart;

	if (!headersSent) {
		//if this is the one and only "chunk" of body data just use a content length header
		//if flush was called without adding a content length we are now stuck
		//unless a header was added directly
		if (!headersSent && finalise) {
			addContentLengthHeader(chunkSize);
		}
		else if (chunkedEncoding) {
			writeHeaderLine(ChunckedTransferHeader);
		}

		switch (connectionMode) {
		case ConnectionKeepAlive:
			writeHeaderLine(ConnectionKeepAliveHeader);
			break;
		case ConnectionClose:
			writeHeaderLine(ConnectionCloseHeader);
			break;
		case ConnectionUpgrade:
			writeHeaderLine(ConnectionUpgradeHeader);
			break;
		}

		//append the headers end
		writeHeaderLine("", 0);

		Result result = networkWrite(responseBuffer, responseHeaderBufferPos - responseBuffer);
		if (result != OK) {
			return ERROR;
		}
		headersSent = true;
	}

	//preend the chunk size to the payload and trailing new line
	auto beforeChunkAdd = responseBufferBodyStart;
	if (chunkedEncoding) {

		char tmp[ChunkedTransferSizeHeaderSize] = "";
		auto lengthSize = Utility::toASCII(chunkSize, tmp, Utility::HexBase, sizeof(tmp));
		memcpy(tmp + lengthSize, EOL, sizeof(EOL));
		lengthSize += sizeof(EOL);

		if (!(appendBodyPrefix(tmp, lengthSize)
			&& appendBody((char*)EOL, sizeof(EOL)))) {
			responseBufferBodyStart = beforeChunkAdd;
			return ERROR;
		}


		//the last chunk must always have a 0 length
		if (chunkSize > 0 && finalise) {
			write("0\r\n\r\n", 5);
		}
	}



	auto result = networkWrite(responseBufferBodyStart, responseBufferPos - responseBufferBodyStart);
	if (result == OK) {
		//no longer need the reserved space for the headers once the headers have been sent
		responseBufferBodyStart = responseBuffer + ChunkedTransferSizeHeaderSize;
		responseBufferPos = responseBufferBodyStart;
		responseHeaderBufferPos = responseBuffer;
	}
	else if (chunkedEncoding) {
		responseBufferBodyStart = beforeChunkAdd;
	}

	return result;
}

ServerConnection* Response::hijackConnection() {
	client->hijacted = true;
	return client;
}

Result Response::networkWrite(char* data, int length) {
	if (client->writeData((uint8_t*)data, length, 0)) {
		responseSizeTotal += length;
		return OK;
	}

	return ERROR;
}
const constexpr char Response::EOL[];
const constexpr struct SimpleString Response::statusStrings[];
const constexpr struct SimpleString Response::ConnectionKeepAliveHeader;
const constexpr struct SimpleString Response::ConnectionCloseHeader;
const constexpr struct SimpleString Response::ConnectionUpgradeHeader;
const constexpr struct SimpleString Response::ChunckedTransferHeader;
const constexpr struct SimpleString Response::ContentLengthHeader;
