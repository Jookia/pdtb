#include <bios.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "utils.h"
#include "packet.h"
#include "arp.h"
#include "tcp.h"
#include "tcpsockm.h"

TcpSocket *sock;

/* user customziable stuff */
int setupSocket(void) {
	Utils::parseEnv();
	Utils::initStack(1, TCP_SOCKET_RING_SIZE);
	sock = TcpSocketMgr::getSocket();
	IpAddr_t serverAddr = { 10, 0, 2, 2 };
	uint16_t srcPort = rand() + 1024;
	uint16_t dstPort = 6666;
	int timeout = 500; /* milliseconds */
	return sock->connect(srcPort, serverAddr, dstPort, timeout);
}

void cleanupSocket(void) {
	sock->close();
	TcpSocketMgr::freeSocket(sock);
	Utils::endStack();
	fclose(TrcStream);
}

int processPackets(void) {
	uint8_t *packet;
	PACKET_PROCESS_SINGLE;
	Arp::driveArp();
	Tcp::drivePackets();
	if(sock->isRemoteClosed() != 0) {
		fprintf(stdout, "Remote has closed, quitting\n");
		return 0;
	}
	if(bioskey(1) && bioskey(0) == (45 << 8)) {
		fprintf(stdout, "Alt-X hit, quitting\n");
		return 0;
	}
	while((packet = (uint8_t*)sock->incoming.dequeue()) != 0) {
		IpHeader *ip = (IpHeader*)(packet + sizeof(EthHeader));
		TcpHeader *tcp = (TcpHeader*)(ip->payloadPtr());
		uint16_t len = ip->payloadLen() - tcp->getTcpHlen();
		TcpBuffer *buf = TcpBuffer::getXmitBuf();
		while(buf == NULL) {
	PACKET_PROCESS_SINGLE;
	Arp::driveArp();
	Tcp::drivePackets();
		printf("skipped packet\n");
		continue;
		}
		    
		uint8_t *data = (uint8_t*)buf + sizeof(TcpBuffer);
		snprintf((char*)data, 32, "got tcp payload length %i\n", len);
		buf->dataLen = 32;
		int rc = sock->enqueue(buf);
		printf("sent pkt %p %i\n", buf, rc);
		Buffer_free(packet);
		
	#if 0
		IpHeader *ip = (IpHeader*)(packet + sizeof(EthHeader));
		TcpHeader *tcp = (TcpHeader*)(ip->payloadPtr());
		uint8_t *userData = ((uint8_t*)tcp) + tcp->getTcpHlen();
		uint16_t len = ip->payloadLen() - tcp->getTcpHlen();
		printf("got pkt %p %i\n", userData, len);
		TcpBuffer *buf = TcpBuffer::getXmitBuf();
		uint8_t *data = (uint8_t*)buf + sizeof(TcpBuffer);
		snprintf((char*)data, 16, "size %i got\n", len);
		buf->dataLen = 16;
		memcpy(data, userData, len);
		buf->dataLen = len;
		int rc = sock->enqueue(buf);
		Buffer_free(packet);
		printf("sent pkt %p %i\n", buf, rc);
	#endif
	}
	return 1;
}

int main(void) {
	int rc = 1;
	fprintf(stdout, "sizeof(TcpBuffer) == %i\n", sizeof(TcpBuffer));
	if(setupSocket()) {
		fprintf(stdout, "Unable to connect to host\n");
		cleanupSocket();
		return 1;
	}
	while(processPackets());
	cleanupSocket();
	return 0;
}
