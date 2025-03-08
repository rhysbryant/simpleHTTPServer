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
#include <stdint.h>
#include <string.h>
//this is for testing
//type definitions for lwip

typedef enum {
	/** No error, everything OK. */
	ERR_OK = 0,
	/** Out of memory error.     */
	ERR_MEM = -1,
	/** Buffer error.            */
	ERR_BUF = -2,
	/** Timeout.                 */
	ERR_TIMEOUT = -3,
	/** Routing problem.         */
	ERR_RTE = -4,
	/** Operation in progress    */
	ERR_INPROGRESS = -5,
	/** Illegal value.           */
	ERR_VAL = -6,
	/** Operation would block.   */
	ERR_WOULDBLOCK = -7,
	/** Address in use.          */
	ERR_USE = -8,
	/** Already connecting.      */
	ERR_ALREADY = -9,
	/** Conn already established.*/
	ERR_ISCONN = -10,
	/** Not connected.           */
	ERR_CONN = -11,
	/** Low-level netif error    */
	ERR_IF = -12,

	/** Connection aborted.      */
	ERR_ABRT = -13,
	/** Connection reset.        */
	ERR_RST = -14,
	/** Connection closed.       */
	ERR_CLSD = -15,
	/** Illegal argument.        */
	ERR_ARG = -16
} err_enum_t;

/** Define LWIP_ERR_T in cc.h if you want to use
 *  a different type for your platform (must be signed). */

typedef err_enum_t err_t;

typedef short u16_t;
struct tcp_pcb {
	void* arg;
	int remote_ip;
};

#define TCP_WRITE_FLAG_COPY 1

inline err_t tcp_write(struct tcp_pcb* client,uint8_t* data,int size, uint8_t apiFlags) {
	char buffer[1024 + 1] = "";
	memcpy(buffer, data, size);
	buffer[size] = 0;
	printf("%s",buffer);
	return ERR_OK;
}

inline err_t tcp_output(struct tcp_pcb* client) { return ERR_OK;  }

inline err_t tcp_close(struct tcp_pcb* client) { return ERR_OK; }

inline err_t tcp_abort(struct tcp_pcb* client) { return ERR_OK; }

inline int ip4addr_ntoa_r(int *v, char* b, int len);