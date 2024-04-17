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

void __interrupt __far handler_ctrlbrk(void) {
}

void __interrupt __far handler_ctrlc(void) {
}

/* user customziable stuff */
int setupSocket(void) {
	Utils::parseEnv();
	Utils::initStack(1, TCP_SOCKET_RING_SIZE, handler_ctrlbrk, handler_ctrlc);
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
}

#define TEST_INPUT  0
#define TEST_OUTPUT 1
int curr_test = TEST_INPUT;

int clamp(int a, int b) {
	return (a < b) ? a : b;
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
	if(_bios_keybrd(1) && _bios_keybrd(0) == (45 << 8)) {
		fprintf(stdout, "Alt-X hit, quitting\n");
		return 0;
	}
	while((packet = (uint8_t*)sock->incoming.dequeue()) != 0) {
		IpHeader *ip = (IpHeader*)(packet + sizeof(EthHeader));
		TcpHeader *tcp = (TcpHeader*)(ip->payloadPtr());
		uint16_t len_in = ip->payloadLen() - tcp->getTcpHlen();
		uint8_t *data_in = (uint8_t*)tcp + sizeof(TcpHeader);
		TcpBuffer *buf_out;
		if(curr_test == TEST_INPUT) {
			Buffer_free(packet);
			while(!(buf_out = TcpBuffer::getXmitBuf())) {
				PACKET_PROCESS_SINGLE;
				Arp::driveArp();
				Tcp::drivePackets();
				continue;
			}
			uint8_t *data_out = (uint8_t*)buf_out + sizeof(TcpBuffer);
			snprintf((char*)data_out, 32, "got tcp payload length %i\n", len_in);
			buf_out->dataLen = 32;
			int rc = sock->enqueue(buf_out);
			printf("sent pkt %p %i\n", buf_out, rc);
		} else if(curr_test == TEST_OUTPUT) {
			int size;
			sscanf((char const*)data_in, "send me %i please", &size);
			printf("they want us to send %i\n", size);
			Buffer_free(packet);
			int step = 500;
			for(int i = 0; i < size; i += step) {
				while(!(buf_out = TcpBuffer::getXmitBuf())) {
					PACKET_PROCESS_SINGLE;
					Arp::driveArp();
					Tcp::drivePackets();
					continue;
				}
				int amount = clamp(size - i, step);
				uint8_t *data_out = (uint8_t*)buf_out + sizeof(TcpBuffer);
				memset(buf_out, 'A', amount);
				buf_out->dataLen = amount;
				int rc = sock->enqueue(buf_out);
				printf("sent pkt %p %i\n", buf_out, rc);
			}
		}
	}
	return 1;
}

int main(int argc, char* argv[]) {
	int rc = 1;
	if(argc != 2) {
		fprintf(stderr, "Usage: test.exe TEST\n");
		return 1;
	}
	if(!strcmp(argv[1], "input")) {
		curr_test = TEST_INPUT;
	} else if(!strcmp(argv[1], "output")) {
		curr_test = TEST_OUTPUT;
	} else {
		fprintf(stderr, "Invalid test\n");
		return 1;
	}
	fprintf(stdout, "Test %i selected\n", curr_test);
	if(setupSocket()) {
		fprintf(stdout, "Unable to connect to host\n");
		cleanupSocket();
		return 1;
	}
	while(processPackets());
	cleanupSocket();
	return 0;
}
