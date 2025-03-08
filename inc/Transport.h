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

#if defined(_WIN32) || defined(__linux__)
#include "mock-tcp.h"
#else
#include "lwip/tcp.h"
#endif

namespace SimpleHTTP::Internal {
	/**
	 * abstract interface for basic IO functions such as wite and close(shutdown)
	 *
	 */
	class Transport {
	public:
		static const int WriteFlagNoLock = 1;
		//don't copy the data
		static const int WriteFlagZeroCopy = 2;
		static const int WriteFlagNoFlush = 4;
		virtual err_t shutdown() = 0;
		virtual int write(const void* dataptr, u16_t len, uint8_t apiflags) = 0;

        virtual int getAvailableSendBuffer() = 0;
	};
}
