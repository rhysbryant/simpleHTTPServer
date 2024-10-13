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
#include "Request.h"
#include <string.h>
#include <ctype.h>
using namespace SimpleHTTP;

Request::Request() {
	reset();
}

Result Request::parse(char* data, int length) {
	if (lastResult == MoreData) {
		if (appendToBuffer(data, length) == ERROR) {
			return ERROR;
		}
		data = &requestBuffer[bufferReadPos];
		length = requestBuffer.size() - bufferReadPos;
	}

	char* dataStartPtr = data;
	char* dataEndPtr = data + length;
	switch (parsingStage) {
	case WaitingRequestLine:
	{
		auto strMethod = nextToken({ data,length }, SpaceChar);

		if (strMethod.size == 0) {
			goto moreData;
		}
		auto m = parseMethod(strMethod);
		if (m == UnknownMethod) {
			return ERROR;
		}

		char* resetToPos = data;
		data += strMethod.size + 1;


		auto strPath = nextToken({ data,(int)(dataEndPtr - data) }, SpaceChar);
		if (strPath.size == 0) {
			data = resetToPos;
			goto moreData;
		}

		data += strPath.size + 1;

		auto strHTTPVersion = nextEOL({ data,(int)(dataEndPtr - data) }, &data);
		if (strHTTPVersion.size == 0) {
			data = resetToPos;
			goto moreData;
		}
		auto httpVersion = parseHTTPVersion({ strHTTPVersion.value,strHTTPVersion.size });
		if (httpVersion == Error) {
			return ERROR;
		}
		else if (httpVersion == VersionUnknown) {
			goto moreData;
		}

		method = m;
		version = httpVersion;
		path.assign(strPath.value, strPath.size);
		parsingStage = WaitingHeaders;

	}
	case WaitingHeaders:
	{
		int contentLength = 0;
		char* start = data;
		while (true) {
			auto header = parseHeaderLine({ data,(int)(dataEndPtr - data) }, &data);
			if (header.name.size == 0) {
				break;
			}
			string headerName(header.name.value, header.name.size);
			int length = headerName.size();

			for (int i = 0; i < length; i++) {
				headerName[i] = toupper(headerName[i]);
			}

			headers[headerName] = header.value;
			//if there is a body gather some info on how it's encoded
			if (methodHasBody[method]) {
				if (headerName == ContentLengthHeaderName) {
					bodyLength = std::stoi(header.value);

				}
				else if (headerName == TransferEncodingHeaderName) {
					if (header.value == "Chunked" || header.value == "chunked") {
						bodyEncodingChunked = true;
					}
				}
			}

		}
		auto endOfHeaders = isEOL({ data,(int)(dataEndPtr - data) });
		if (endOfHeaders) {
			data += endOfHeaders;
			parsingStage = WaitingBody;
		}
		else {
			data = start;
			goto moreData;
		}
	}
	case WaitingBody:
	{
		if (!methodHasBody[method]) {
			parsingStage = WaitingComplete;
			return OK;
		}

		if (bodyEncodingChunked || bodyLength != 0) {
			hasMoreBodyDataSinceLastCheck = true;
			goto moreData;
		}
	}
	case WaitingComplete:
		;
	}


	return ERROR;
moreData:
	if (lastResult == MoreData) {
		bufferReadPos = data - requestBuffer.data();

	}
	else {
		appendToBuffer(data, dataEndPtr - data);
		lastResult = MoreData;
	}

	return MoreData;

}

Result  Request::appendToBuffer(char* data, int size) {
	requestBuffer.insert(requestBuffer.end(),data,data+size);
	return MoreData;
}

Request::Method Request::parseMethod(SimpleString strMethod) {
	for (int i = 0; i < requestMethodsCount; i++) {
		if (strMethod.size == requestMethods[i].size && memcmp(strMethod.value, requestMethods[i].value, strMethod.size) == 0) {
			return (Method)i;
		}
	}
	return UnknownMethod;
}

Request::HTTPVersion Request::parseHTTPVersion(SimpleString str) {
	for (int i = 0; i < HTTPVersionsCount; i++) {
		if (str.size == HTTPVersions[i].size && memcmp(str.value, HTTPVersions[i].value, str.size) == 0) {
			return (HTTPVersion)i;
		}
	}
	return VersionUnknown;
}

HTTPHeader Request::parseHeaderLine(SimpleString data, char** eolEndPosPtr) {

	SimpleString headerName = nextToken(data, ':');
	if (headerName.size == 0) {
		return { 0 };
	}

	int offset = headerName.size + 2;
	if (offset >= data.size) {
		return { 0 };
	}

	auto value = nextEOL({ data.value + offset,data.size - offset }, eolEndPosPtr);

	if (value.value == 0) {
		return { 0 };
	}

	return { headerName,string(value.value,value.size) };

}

SimpleString Request::nextToken(SimpleString data, char tok) {
	char* ptr = (char*)memchr(data.value, tok, data.size);
	if (ptr == 0) {
		return {};
	}
	return { data.value,(int)(ptr - data.value) };
}

SimpleString Request::nextEOL(SimpleString line, char** eolEndPosPtr) {
	char* ptr = (char*)memchr(line.value, '\n', line.size);
	if (ptr == 0) {
		return {};
	}

	*eolEndPosPtr = ptr + 1;

	if (ptr - line.value > 0 && *(ptr - 1) == '\r') {
		ptr--;
	}

	return { line.value,(int)(ptr - line.value) };
}

int Request::isEOL(SimpleString line) {
	if (line.size >= 2) {
		if (memcmp(line.value, "\r\n", 2) == 0) {
			return 2;
		}
	}
	else if (line.size >= 1) {
		if (memcmp(line.value, "\n", 1) == 0) {
			return 1;
		}
	}
	return 0;
}

bool Request::isEndOfChunkedBodyMarker(SimpleString line) {
	if (line.size >= 5) {
		if (memcmp(line.value, "0\r\n\r\n", 2) == 0) {
			return true;
		}
	}
	return false;
}

Result Request::readBody(char* dstBuffer, int* dstBufferSize) {
	int outputBufferSize = *dstBufferSize;
	int outputBytesWritten = 0;
	bool atEndOfBuffer = false;
	bodyReadInProgress = true;

	char* requestBufferReadPos = &requestBuffer[bufferReadPos];
	const char* requestBufferEnd = requestBuffer.data() + requestBuffer.size();

	//if the body is chunked it should start with the chunk size in hex followed by a new line

	if (bodyLength == 0) {
		if (!bodyEncodingChunked) {
			return ERROR;
		}
	tryReadNextChunk:
		auto strChunkSize = nextEOL({ requestBufferReadPos,(int)(requestBuffer.size()) }, &requestBufferReadPos);
		if (strChunkSize.value == nullptr) {		
			return MoreData;
		}

		bodyLength = std::stoi(string(strChunkSize.value, strChunkSize.size), 0, 16);
		if (bodyLength == 0 && isEOL({ requestBufferReadPos,(int)(requestBuffer.size()) })) {
			*dstBufferSize = outputBytesWritten;
			lastBodyOutputBytesWritten = outputBytesWritten;
			bodyReadInProgress = false;
			bufferReadPos = requestBufferEnd - requestBufferReadPos;
			return OK;
		}
	}

	if (bodyLength != 0) {
		int sizeToCopy = bodyLength;
		int dataInRequestBuffer = requestBufferEnd - requestBufferReadPos;
		if (sizeToCopy >= dataInRequestBuffer) {
			sizeToCopy = dataInRequestBuffer;
			atEndOfBuffer = true;
		}

		int spaceInDstBuffer = outputBufferSize;
		if (sizeToCopy >= spaceInDstBuffer) {
			sizeToCopy = spaceInDstBuffer;
		}

		memcpy(dstBuffer, requestBufferReadPos, sizeToCopy);
		requestBufferReadPos += sizeToCopy;
		bodyLength -= sizeToCopy;
		outputBytesWritten += sizeToCopy;
		*dstBufferSize = outputBytesWritten;
		lastBodyOutputBytesWritten = outputBytesWritten;

		if (bodyLength == 0 && bodyEncodingChunked) {
			nextEOL({ requestBufferReadPos,(int)(requestBuffer.size()) }, &requestBufferReadPos);
			goto tryReadNextChunk;
		}
	}

	if (atEndOfBuffer) {
		resetBuffer();
	}

	bufferReadPos = requestBufferEnd - requestBufferReadPos;

	if (bodyLength == 0) {
		bodyReadInProgress = false;
		return OK;
	}
	else {
		return MoreData;
	}
}

Result Request::unReadBody() {
	if (bufferReadPos - lastBodyOutputBytesWritten <= 0) {
		return ERROR;
	}
	bufferReadPos -= lastBodyOutputBytesWritten;

	return OK;
}

void Request::reset() {
	version = VersionUnknown;
	method = UnknownMethod;
	parsingStage = WaitingRequestLine;
	lastResult = Result::OK;
	requestBuffer.clear();
	bodyEncodingChunked = false;
	bodyReadInProgress = false;
	hasMoreBodyDataSinceLastCheck = false;
	lastBodyOutputBytesWritten = 0;
	headers.clear();
	path.clear();
	bodyLength = 0;
}

const constexpr struct SimpleString Request::requestMethods[];
const constexpr struct SimpleString Request::HTTPVersions[];
const constexpr char Request::TransferEncodingHeaderName[];
const constexpr char Request::ContentLengthHeaderName[];
const constexpr bool Request::methodHasBody[];
