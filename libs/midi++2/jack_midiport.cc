/*
  Copyright (C) 2006 Paul Davis 
  Written by Dave Robillard
 
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <fcntl.h>
#include <cerrno>
#include <cassert>
#include <cstring>
#include <cstdlib>

#include "pbd/error.h"
#include "pbd/compose.h"
#include "pbd/strsplit.h"

#include "midi++/types.h"
#include "midi++/jack.h"

using namespace std;
using namespace MIDI;
using namespace PBD;

pthread_t JACK_MidiPort::_process_thread;

Signal0<void> JACK_MidiPort::JackHalted;
Signal0<void> JACK_MidiPort::MakeConnections;

JACK_MidiPort::JACK_MidiPort(const XMLNode& node, jack_client_t* jack_client)
	: Port(node)
	, _jack_client(jack_client)
	, _jack_input_port(NULL)
	, _jack_output_port(NULL)
	, _last_read_index(0)
	, output_fifo (512)
	, input_fifo (1024)
{
	if (!create_ports (node)) {
		_ok = true;
	}

	MakeConnections.connect_same_thread (connect_connection, boost::bind (&JACK_MidiPort::make_connections, this));
	JackHalted.connect_same_thread (halt_connection, boost::bind (&JACK_MidiPort::jack_halted, this));

	set_state (node);
}

JACK_MidiPort::~JACK_MidiPort()
{
	if (_jack_input_port) {
		if (_jack_client) {
			jack_port_unregister (_jack_client, _jack_input_port);
		}
		_jack_input_port = 0;
	}

	if (_jack_output_port) {
		if (_jack_client) {
			jack_port_unregister (_jack_client, _jack_input_port);
		}
		_jack_input_port = 0;
	}
}

void
JACK_MidiPort::jack_halted ()
{
	_jack_client = 0;
	_jack_input_port = 0;
	_jack_output_port = 0;
}

void
JACK_MidiPort::cycle_start (nframes_t nframes)
{
	Port::cycle_start(nframes);
	assert(_nframes_this_cycle == nframes);
	_last_read_index = 0;
	_last_write_timestamp = 0;

	if (_jack_output_port != 0) {
		// output
		void *buffer = jack_port_get_buffer (_jack_output_port, nframes);
		jack_midi_clear_buffer (buffer);
		flush (buffer);	
	}
	
	if (_jack_input_port != 0) {
		// input
		void* jack_buffer = jack_port_get_buffer(_jack_input_port, nframes);
		const nframes_t event_count = jack_midi_get_event_count(jack_buffer);

		jack_midi_event_t ev;
		timestamp_t cycle_start_frame = jack_last_frame_time (_jack_client);

		for (nframes_t i = 0; i < event_count; ++i) {
			jack_midi_event_get (&ev, jack_buffer, i);
			input_fifo.write (cycle_start_frame + ev.time, (Evoral::EventType) 0, ev.size, ev.buffer);
		}	
		
		if (event_count) {
			xthread.wakeup ();
		}
	}
}

void
JACK_MidiPort::cycle_end ()
{
	if (_jack_output_port != 0) {
		flush (jack_port_get_buffer (_jack_output_port, _nframes_this_cycle));
	}

	Port::cycle_end();
}

int
JACK_MidiPort::write(byte * msg, size_t msglen, timestamp_t timestamp)
{
	int ret = 0;

	if (!is_process_thread()) {

		Glib::Mutex::Lock lm (output_fifo_lock);
		RingBuffer< Evoral::Event<double> >::rw_vector vec = { { 0, 0 }, { 0, 0} };
		
		output_fifo.get_write_vector (&vec);

		if (vec.len[0] + vec.len[1] < 1) {
			error << "no space in FIFO for non-process thread MIDI write" << endmsg;
			return 0;
		}

		if (vec.len[0]) {
                        if (!vec.buf[0]->owns_buffer()) {
                                vec.buf[0]->set_buffer (0, 0, true);
                        }
			vec.buf[0]->set (msg, msglen, timestamp);
		} else {
                        if (!vec.buf[1]->owns_buffer()) {
                                vec.buf[1]->set_buffer (0, 0, true);
                        }
			vec.buf[1]->set (msg, msglen, timestamp);
		}

		output_fifo.increment_write_idx (1);
		
		ret = msglen;

	} else {

		assert(_jack_output_port);
		
		// XXX This had to be temporarily commented out to make export work again
		if (!(timestamp < _nframes_this_cycle)) {
			std::cerr << "assertion timestamp < _nframes_this_cycle failed!" << std::endl;
		}

		if (_currently_in_cycle) {
			if (timestamp == 0) {
				timestamp = _last_write_timestamp;
			} 

			if (jack_midi_event_write (jack_port_get_buffer (_jack_output_port, _nframes_this_cycle), 
						timestamp, msg, msglen) == 0) {
				ret = msglen;
				_last_write_timestamp = timestamp;

			} else {
				ret = 0;
				cerr << "write of " << msglen << " failed, port holds "
					<< jack_midi_get_event_count (jack_port_get_buffer (_jack_output_port, _nframes_this_cycle))
					<< endl;
			}
		} else {
			cerr << "write to JACK midi port failed: not currently in a process cycle." << endl;
		}
	}

	if (ret > 0 && output_parser) {
		// ardour doesn't care about this and neither should your app, probably
		// output_parser->raw_preparse (*output_parser, msg, ret);
		for (int i = 0; i < ret; i++) {
			output_parser->scanner (msg[i]);
		}
		// ardour doesn't care about this and neither should your app, probably
		// output_parser->raw_postparse (*output_parser, msg, ret);
	}	

	return ret;
}

void
JACK_MidiPort::flush (void* jack_port_buffer)
{
	RingBuffer< Evoral::Event<double> >::rw_vector vec = { { 0, 0 }, { 0, 0 } };
	size_t written;

	output_fifo.get_read_vector (&vec);

	if (vec.len[0] + vec.len[1]) {
		// cerr << "Flush " << vec.len[0] + vec.len[1] << " events from non-process FIFO\n";
	}

	if (vec.len[0]) {
		Evoral::Event<double>* evp = vec.buf[0];
		
		for (size_t n = 0; n < vec.len[0]; ++n, ++evp) {
			jack_midi_event_write (jack_port_buffer,
					       (timestamp_t) evp->time(), evp->buffer(), evp->size());
		}
	}
	
	if (vec.len[1]) {
		Evoral::Event<double>* evp = vec.buf[1];

		for (size_t n = 0; n < vec.len[1]; ++n, ++evp) {
			jack_midi_event_write (jack_port_buffer,
					       (timestamp_t) evp->time(), evp->buffer(), evp->size());
		}
	}
	
	if ((written = vec.len[0] + vec.len[1]) != 0) {
		output_fifo.increment_read_idx (written);
	}
}

int
JACK_MidiPort::read (byte *, size_t)
{
	timestamp_t time;
	Evoral::EventType type;
	uint32_t size;
	byte buffer[input_fifo.capacity()];

	while (input_fifo.read (&time, &type, &size, buffer)) {
		if (input_parser) {
			input_parser->set_timestamp (time);
			for (uint32_t i = 0; i < size; ++i) {
				input_parser->scanner (buffer[i]);
			}
		}
	}

	return 0;
}

int
JACK_MidiPort::create_ports(const XMLNode& node)
{
	Descriptor desc (node);

	assert(!_jack_input_port);
	assert(!_jack_output_port);
	
	jack_nframes_t nframes = jack_get_buffer_size(_jack_client);

	bool ret = true;

	if (desc.mode == O_RDWR || desc.mode == O_WRONLY) {
		_jack_output_port = jack_port_register(_jack_client,
						       string(desc.tag).append("_out").c_str(),
						       JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
		if (_jack_output_port) {
			jack_midi_clear_buffer(jack_port_get_buffer(_jack_output_port, nframes));
		}
		ret = ret && (_jack_output_port != NULL);
	}
	
	if (desc.mode == O_RDWR || desc.mode == O_RDONLY) {
		_jack_input_port = jack_port_register(_jack_client,
						      string(desc.tag).append("_in").c_str(),
						      JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
		if (_jack_input_port) {
			jack_midi_clear_buffer(jack_port_get_buffer(_jack_input_port, nframes));
		}
		ret = ret && (_jack_input_port != NULL);
	}

	return ret ? 0 : -1;
}

XMLNode& 
JACK_MidiPort::get_state () const
{
	XMLNode& root (Port::get_state ());

	if (_jack_output_port) {
		
		const char** jc = jack_port_get_connections (_jack_output_port);
		string connection_string;
		if (jc) {
			for (int i = 0; jc[i]; ++i) {
				if (i > 0) {
					connection_string += ',';
				}
				connection_string += jc[i];
			}
			free (jc);
		}
		
		if (!connection_string.empty()) {
			root.add_property ("outbound", connection_string);
		}
	} else {
		if (!_outbound_connections.empty()) {
			root.add_property ("outbound", _outbound_connections);
		}
	}

	if (_jack_input_port) {

		const char** jc = jack_port_get_connections (_jack_input_port);
		string connection_string;
		if (jc) {
			for (int i = 0; jc[i]; ++i) {
				if (i > 0) {
					connection_string += ',';
				}
				connection_string += jc[i];
			}
			free (jc);
		}

		if (!connection_string.empty()) {
			root.add_property ("inbound", connection_string);
		}
	} else {
		if (!_inbound_connections.empty()) {
			root.add_property ("inbound", _inbound_connections);
		}
	}

	return root;
}

void
JACK_MidiPort::set_state (const XMLNode& node)
{
	Port::set_state (node);
	const XMLProperty* prop;

	if ((prop = node.property ("inbound")) != 0 && _jack_input_port) {
		_inbound_connections = prop->value ();
	}

	if ((prop = node.property ("outbound")) != 0 && _jack_output_port) {
		_outbound_connections = prop->value();
	}
}

void
JACK_MidiPort::make_connections ()
{
	if (!_inbound_connections.empty()) {
		vector<string> ports;
		split (_inbound_connections, ports, ',');
		for (vector<string>::iterator x = ports.begin(); x != ports.end(); ++x) {
			if (_jack_client) {
				jack_connect (_jack_client, (*x).c_str(), jack_port_name (_jack_input_port));
				/* ignore failures */
			}
		}
	}

	if (!_outbound_connections.empty()) {
		vector<string> ports;
		split (_outbound_connections, ports, ',');
		for (vector<string>::iterator x = ports.begin(); x != ports.end(); ++x) {
			if (_jack_client) {
				jack_connect (_jack_client, jack_port_name (_jack_output_port), (*x).c_str());
				/* ignore failures */
			}
		}
	}
	connect_connection.disconnect ();
}

void
JACK_MidiPort::set_process_thread (pthread_t thr)
{
	_process_thread = thr;
}

bool
JACK_MidiPort::is_process_thread()
{
	return (pthread_self() == _process_thread);
}
