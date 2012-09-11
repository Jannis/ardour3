#ifndef ardour_wiimote_control_protocol_h
#define ardour_wiimote_control_protocol_h

#include <cwiid.h>

#include "pbd/abstract_ui.h"
#include "ardour/types.h"
#include "control_protocol/control_protocol.h"

struct WiimoteControlUIRequest : public BaseUI::BaseRequestObject {
public:
	WiimoteControlUIRequest () {}
	~WiimoteControlUIRequest () {}
};

class WiimoteControlProtocol
	: public ARDOUR::ControlProtocol
	, public AbstractUI<WiimoteControlUIRequest>
{
public:
	WiimoteControlProtocol (ARDOUR::Session &);
	virtual ~WiimoteControlProtocol ();

	static bool probe ();
	int set_active (bool yn);

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	void wiimote_callback (cwiid_wiimote_t* wiimote, int mesg_count, union cwiid_mesg msg[], struct timespec* t);

protected:
	void do_request (WiimoteControlUIRequest*);
	int start ();
	int stop ();

	void thread_init ();

	bool idle ();
	bool connect_wiimote ();

	void update_led_state ();

protected:
	PBD::ScopedConnectionList session_connections;
	cwiid_wiimote_t* wiimote;
	GSource *idle_source;
	uint16_t button_state;
};

#endif  /* ardour_wiimote_control_protocol_h */

