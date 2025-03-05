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
extern "C" {
#include "lwip/tcp.h"

#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509.h"
#include "mbedtls/ssl.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"

#if defined(MBEDTLS_SSL_CACHE_C)
#include "mbedtls/ssl_cache.h"
#endif
}
#include <queue>
#include "ServerConnection.h"
#include "SecureServerConnection.h"

#include "common.h"

namespace SimpleHTTP {
	class SecureServer {
	private:
	
		static struct tcp_pcb* tcpServer;

		static err_t tcp_sent_cb(void* arg, struct tcp_pcb* tpcb, u16_t len);
		static err_t tcp_recv_cb(void* arg, struct tcp_pcb* tpcb, struct pbuf* p, err_t err);
		static err_t tcp_accept_cb(void* arg, struct tcp_pcb* newpcb, err_t err);
		static void tcp_err_cb(void* arg, err_t err);

		static mbedtls_entropy_context entropy;
		static mbedtls_ctr_drbg_context ctr_drbg;
		static mbedtls_ssl_config conf;
		static mbedtls_x509_crt srvcert;
		static mbedtls_pk_context pkey;
		static mbedtls_ssl_cache_context cache;
		static bool crtInitDone;

		//entry point for sending of unedncripted data
		static int tcp_write_tls(tcp_pcb* pcb, const void* dataptr, u16_t len, u8_t apiflags);

		static const int maxSendSize = ServerConnection::maxSendSize + 29;

	public:
		static int loadPrivateKey(SimpleString* cert);
		static int loadCert(SimpleString* cert);
		static mbedtls_x509_crt* getCertChain();
		static int TLSInit();
		static void listen(int port);

	};
};
