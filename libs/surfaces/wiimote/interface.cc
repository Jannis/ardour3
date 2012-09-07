#include "pbd/failed_constructor.h"
#include "pbd/error.h"

#include "ardour/session.h"
#include "control_protocol/control_protocol.h"

#include "wiimote.h"

using namespace ARDOUR;
using namespace PBD;

ControlProtocol*
new_wiimote_protocol (ControlProtocolDescriptor* descriptor, Session* s)
{
	WiimoteControlProtocol* wmcp = new WiimoteControlProtocol (*s);
	wmcp->set_active (true);
	return wmcp;
}

void
delete_wiimote_protocol (ControlProtocolDescriptor* descriptor, ControlProtocol* cp)
{
	delete cp;
}

bool
probe_wiimote_protocol (ControlProtocolDescriptor* descriptor)
{
	return WiimoteControlProtocol::probe ();
}

static ControlProtocolDescriptor wiimote_descriptor = {
	name : "Wiimote",
	id : "uri://ardour.org/surfaces/wiimote:0",
	ptr : 0,
	module : 0,
	mandatory : 0,
	supports_feedback : false,
	probe : probe_wiimote_protocol,
	initialize : new_wiimote_protocol,
	destroy : delete_wiimote_protocol
};


extern "C" {

ControlProtocolDescriptor*
protocol_descriptor () {
	return &wiimote_descriptor;
}

}

