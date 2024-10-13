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
#include <vector>
#include <string>
#include <map>

using std::string;
using std::vector;
using std::map;

namespace SimpleHTTP {
	//HTTP/1.1 Request Parsing
	class Request {
	private:
		enum ParsingStage : int {
			WaitingRequestLine,
			WaitingHeaders,
			WaitingBody,
			WaitingComplete
		} parsingStage;
		static const char SpaceChar = ' ';

		Result lastResult;
		static const int requestBufferSize = 512;
		static constexpr const char TransferEncodingHeaderName[] = "TRANSFER-ENCODING";
		static constexpr const char ContentLengthHeaderName[] = "CONTENT-LENGTH";

		vector<char> requestBuffer;
		int bufferReadPos;
		int lastBodyOutputBytesWritten;

		Result appendToBuffer(char* data, int size);

		inline void resetBuffer() {
			requestBuffer.clear();
			bufferReadPos = 0;
		}

		bool bodyEncodingChunked;
		int bodyLength;
		bool bodyReadInProgress;

	public:
		static const int requestMethodsCount = 7;

		enum Method : int {
			GET = 0,
			PUT,
			HEAD,
			POST,
			SEND,
			DELETE,
			OPTIONS,
			UnknownMethod
		} method;



		enum HTTPVersion : int {
			HTTP10 = 0,
			HTTP11,
			VersionUnknown,
			Error
		} version;

		static const int HTTPVersionsCount = 2;

		static const int MaxHeaderNameLength = 64;
		static const int MaxHeaderValueLength = 255;

		map<string, string> headers;
		string path;


		Request();

		Result parse(char* data, int length);

		void reset();

		inline bool receivedAllHeaders() { return parsingStage == WaitingBody || parsingStage == WaitingComplete; };
		/*
		* returns true if the request is ready for process or more body data has been received since
		* last time this method was called
		*/
		inline bool getAndClearForProcessing() {
			if (parsingStage == WaitingBody && hasMoreBodyDataSinceLastCheck) {
				return true;
				hasMoreBodyDataSinceLastCheck = true;
			}
			return parsingStage == WaitingComplete;
		}
		/**
		* read out the internal body buffer
		* dstBufferSize in/out pass the buffer size
		* on return it's set to the number of bytes written to the buffer
		*
		**/
		Result readBody(char* dstBuffer, int* dstBufferSize);
		/*
		* reset the buffer position back to the point before the last call to readBody()
		* OK or ERROR if net not be undone
		*/
		Result unReadBody();

		inline int getBodyLength() { return bodyLength; }
		inline bool isBodyReadInProgress() { return bodyReadInProgress; }

	private:
		static const constexpr struct SimpleString requestMethods[] = {
		   SIMPLE_STR("GET"),
		   SIMPLE_STR("PUT"),
		   SIMPLE_STR("HEAD"),
		   SIMPLE_STR("POST"),
		   SIMPLE_STR("SEND"),
		   SIMPLE_STR("DELETE"),
		   SIMPLE_STR("OPTIONS")
		};

		static const constexpr bool methodHasBody[] = {
			false,
			true,
			false,
			true,
			true,
			false,
			false
		};

		static const constexpr struct SimpleString HTTPVersions[] = {
			SIMPLE_STR("HTTP/1.0"),
			SIMPLE_STR("HTTP/1.1")
		};
	protected:
		/**
		 * accepts GET,POST, SEND or OPTIONS strings and returns the enum value
		**/
		Method parseMethod(SimpleString str);

		bool hasMoreBodyDataSinceLastCheck;

		HTTPVersion parseHTTPVersion(SimpleString str);
		/**
		* parses a http header line i.e name:value\r\n
		*/
		HTTPHeader parseHeaderLine(SimpleString data, char** eolEndPosPtr);
		/**
		 * returns up to the first space
		 */
		string parsePath(const char* data, int size);
		/**
		* returns a string up to the next instance of TOK CHAR
		*/
		SimpleString nextToken(SimpleString data, char tok);
		/**
		 * returns the string up to \n. skipping  leading \r if any
		**/
		SimpleString nextEOL(SimpleString line, char** eolEndPosPtr);
		/**
		* returns true if the next line is blank
		*/
		int isEOL(SimpleString line);
		/**
		* returns true if the next token is 0\r\n\r\n
		*/
		bool isEndOfChunkedBodyMarker(SimpleString line);
	};
};
