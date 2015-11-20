#include <math.h>
#include <string>
#include <sstream>
#include <intrin.h>
#include <float.h>

#include "PaletteHost.h"
#include "NosuchUtil.h"
#include "NosuchOscInput.h"
#include "PaletteOscInput.h"
#include "NosuchOscTcpInput.h"
#include "NosuchOscUdpInput.h"

PaletteOscInput::PaletteOscInput(PaletteHost* server, const char* host, int port) : NosuchOscMessageProcessor() {

	NosuchDebug(2,"PaletteOscInput constructor port=%d",port);
	// _seq = -1;
	// _tcp = new NosuchOscTcpInput(host,port);
	_tcp = NULL;
	_udp = new NosuchOscUdpInput(host,port,this);
	_server = server;
}

PaletteOscInput::~PaletteOscInput() {
	if ( _tcp )
		delete _tcp;
	if ( _udp )
		delete _udp;
}

void
PaletteOscInput::ProcessOscMessage(const char *source, const osc::ReceivedMessage& m) {
	_server->ProcessOscMessage(source,m);
}

void
PaletteOscInput::Check() {
	if ( _tcp )
		_tcp->Check();
	if ( _udp )
		_udp->Check();
}

void
PaletteOscInput::UnListen() {
	if ( _tcp )
		_tcp->UnListen();
	if ( _udp )
		_udp->UnListen();
}

int
PaletteOscInput::Listen() {
	int e;
	if ( _tcp ) {
		if ( (e=_tcp->Listen()) != 0 ) {
			if ( e == WSAEADDRINUSE ) {
				NosuchErrorOutput("TCP port/address (%d/%s) is already in use?",_tcp->Port(),_tcp->Host());
			} else {
				NosuchErrorOutput("Error in _tcp->Listen = %d\n",e);
			}
			return e;
		}
	}
	if ( _udp ) {
		if ( (e=_udp->Listen()) != 0 ) {
			if ( e == WSAEADDRINUSE ) {
				NosuchErrorOutput("UDP port/address (%d/%s) is already in use?",_udp->Port(),_udp->Host());
			} else {
				NosuchErrorOutput("Error in _udp->Listen = %d\n",e);
			}
			return e;
		}
	}
	return 0;
}
