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

/*
	BUG: mTCP will hang when dropping packets larger than its MTU
	This is because it can't ACK these packets and thus the stream hangs.
	Instead it should send RST or something.
	BUG: SLIRP proxies the TCP connection and will re-pack the TCP
	stream according to the TCP window size (four times the MSS) and using
	its own MTU as the cap rather than the MSS.
	COMBINED ISSUE: Using a smaller MTU in mTCP will cause the window
	to be set to something larger than its MTU and cause SLIRP to send
	packets the size of its MTU, larger than ours.
	WORKAROUND: Set mTCP's MTU to SLIRP's MTU to avoid SLIRP making
	packets too large for our uses.
*/

TcpSocket *sock;
uint8_t lineBuffer[8] = {0};
uint8_t *linePos = lineBuffer;
#define LINE_END (lineBuffer + sizeof(lineBuffer))
#define LINE_LEN (linePos - lineBuffer)

/* user customziable stuff */
int setupSocket(void) {
	Utils::parseEnv();
	Utils::initStack(1, TCP_SOCKET_RING_SIZE);
	sock = TcpSocketMgr::getSocket();
	IpAddr_t serverAddr = { 10, 0, 2, 2 };
	uint16_t srcPort = rand() + 1024;
	uint16_t dstPort = 6667;
	int timeout = 500; /* milliseconds */
	return sock->connect(srcPort, serverAddr, dstPort, timeout);
}

void cleanupSocket(void) {
	sock->close();
	TcpSocketMgr::freeSocket(sock);
	Utils::endStack();
	fclose(TrcStream);
}

void sendLine(void) {
	TcpBuffer *buf = TcpBuffer::getXmitBuf();
	if(buf != NULL) {
		/* check if maxData or whatever is going to fit this */
		uint8_t *data = (uint8_t*)buf + sizeof(TcpBuffer);
		memcpy(data, lineBuffer, LINE_LEN);
		buf->dataLen = LINE_LEN + 1;
		data[LINE_LEN] = '\0';
		fprintf(stdout, "OUTGOING %s\n", data);
		sock->enqueue(buf);
	}
}

void handleLine(void) {
	/* bot code goes here using lineBuffer */
	if(lineBuffer[0] == '!') {
		sendLine();
	}
}

int appendLineBuffer(uint8_t in) {
	if(LINE_END <= linePos) {
		if(LINE_END == linePos) {
			fprintf(stderr, "Dropping long line\n");
		}
		if(in == '\n') {
			linePos = lineBuffer;
		}
		return 1;
	}
	if(in == '\n') {
		*linePos = '\0';
		handleLine();
		linePos = lineBuffer;
	} else {
		*linePos++ = in;
	}
	return 0;
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
		uint8_t *userData = ((uint8_t*)tcp) + tcp->getTcpHlen();
		uint16_t len = ip->payloadLen() - tcp->getTcpHlen();
		uint8_t *pos = userData;
		uint8_t *after = userData + len;
		for(; pos != after; appendLineBuffer(*pos++));
		Buffer_free(packet);
	}
	return 1;
}

int main(void) {
	int rc = 1;
	if(setupSocket()) {
		fprintf(stdout, "Unable to connect to host\n");
		cleanupSocket();
		return 1;
	}
	while(processPackets());
	cleanupSocket();
	return 0;
}
