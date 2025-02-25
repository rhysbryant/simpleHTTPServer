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
#include "../../simpleHTTPServer.conf.h"
#include <string>
namespace SimpleHTTP{
    enum Result{
        OK,
        ERROR,
        MoreData
    };

    #define SIMPLE_STR(X) {X,sizeof(X) - 1}
    //stucture to hold a const char* and it's size
    struct SimpleString{
        const char* value;
        const int size;
    };

	enum HTTPVersion : int {
		HTTP10 = 0,
		HTTP11,
#ifdef SIMPLE_HTTP_RTSP_SUPPORT
		RTSP10,
#endif
		VersionUnknown,
		Error
	};

	static const constexpr struct SimpleString HTTPVersions[] = {
		SIMPLE_STR("HTTP/1.0"),
		SIMPLE_STR("HTTP/1.1"),
#ifdef SIMPLE_HTTP_RTSP_SUPPORT
		SIMPLE_STR("RTSP/1.0")
#endif
	};

	static const int HTTPVersionsCount = sizeof(HTTPVersions)/sizeof(HTTPVersions[0]);

	struct HTTPHeader {
		SimpleString name;
		std::string value;
	};

};

extern "C" {
	uint32_t os_getUnixTime();
}
