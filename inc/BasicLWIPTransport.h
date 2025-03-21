#pragma once

#include "Transport.h"
namespace SimpleHTTP::Internal
{
	class BasicLWIPTransport : public Transport
	{
	private:
		tcp_pcb* pcb;
	public:

		void setPCB(tcp_pcb* pcb) {
			this->pcb = pcb;
		}

		inline int write(const void* dataptr, u16_t len, uint8_t apiflags)
		{
			auto err = tcp_write(pcb, (uint8_t*)dataptr, len, apiflags);
			if (err != ERR_OK) {
				return err;
			}
			if ((apiflags & WriteFlagNoFlush) == 0)
				err = tcp_output(pcb);
			if (err != ERR_OK) {
				return err;
			}
			return len;
		}

		inline err_t shutdown()
		{
			return tcp_close(pcb);
		}

		int getAvailableSendBuffer() {
			return tcp_sndbuf(pcb);
		}

		inline bool getRemoteIPAddress(char* buf, int buflen) {
#if LWIP_IPV6
			switch (pcb->remote_ip.type) {
			case IPADDR_TYPE_V4:
				return ip4addr_ntoa_r(&pcb->remote_ip.u_addr.ip4, buf, buflen) != 0;
			case IPADDR_TYPE_V6:
				return ip6addr_ntoa_r(&pcb->remote_ip.u_addr.ip6, buf, buflen) != 0;
			}
#else
			return ip4addr_ntoa_r(&pcb->remote_ip, buf, buflen) != 0;
#endif
			return false;
		}
	};
};