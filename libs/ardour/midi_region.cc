/*
    Copyright (C) 2000-2006 Paul Davis

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

    $Id: midiregion.cc 746 2006-08-02 02:44:23Z drobilla $
*/

#include <cmath>
#include <climits>
#include <cfloat>

#include <set>

#include <sigc++/bind.h>
#include <sigc++/class_slot.h>

#include <glibmm/thread.h>

#include "pbd/basename.h"
#include "pbd/xml++.h"
#include "pbd/enumwriter.h"

#include "ardour/midi_region.h"
#include "ardour/session.h"
#include "ardour/gain.h"
#include "ardour/dB.h"
#include "ardour/playlist.h"
#include "ardour/midi_source.h"
#include "ardour/types.h"
#include "ardour/midi_ring_buffer.h"

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;

/** Basic MidiRegion constructor (one channel) */
MidiRegion::MidiRegion (boost::shared_ptr<MidiSource> src, nframes_t start, nframes_t length)
	: Region (src, start, length, PBD::basename_nosuffix(src->name()), DataType::MIDI, 0,  Region::Flag(Region::DefaultFlags|Region::External))
{
	assert(_name.find("/") == string::npos);
	midi_source(0)->Switched.connect(sigc::mem_fun(this, &MidiRegion::switch_source));
}

/* Basic MidiRegion constructor (one channel) */
MidiRegion::MidiRegion (boost::shared_ptr<MidiSource> src, nframes_t start, nframes_t length, const string& name, layer_t layer, Flag flags)
	: Region (src, start, length, name, DataType::MIDI, layer, flags)
{
	assert(_name.find("/") == string::npos);
	midi_source(0)->Switched.connect(sigc::mem_fun(this, &MidiRegion::switch_source));
}

/* Basic MidiRegion constructor (many channels) */
MidiRegion::MidiRegion (const SourceList& srcs, nframes_t start, nframes_t length, const string& name, layer_t layer, Flag flags)
	: Region (srcs, start, length, name, DataType::MIDI, layer, flags)
{
	assert(_name.find("/") == string::npos);
	midi_source(0)->Switched.connect(sigc::mem_fun(this, &MidiRegion::switch_source));
}


/** Create a new MidiRegion, that is part of an existing one */
MidiRegion::MidiRegion (boost::shared_ptr<const MidiRegion> other, nframes_t offset, nframes_t length, const string& name, layer_t layer, Flag flags)
	: Region (other, offset, length, name, layer, flags)
{
	assert(_name.find("/") == string::npos);
	midi_source(0)->Switched.connect(sigc::mem_fun(this, &MidiRegion::switch_source));
}

MidiRegion::MidiRegion (boost::shared_ptr<const MidiRegion> other)
	: Region (other)
{
	assert(_name.find("/") == string::npos);
	midi_source(0)->Switched.connect(sigc::mem_fun(this, &MidiRegion::switch_source));
}

MidiRegion::MidiRegion (boost::shared_ptr<MidiSource> src, const XMLNode& node)
	: Region (src, node)
{
	if (set_state (node, Stateful::loading_state_version)) {
		throw failed_constructor();
	}

	midi_source(0)->Switched.connect(sigc::mem_fun(this, &MidiRegion::switch_source));
	assert(_name.find("/") == string::npos);
	assert(_type == DataType::MIDI);
}

MidiRegion::MidiRegion (const SourceList& srcs, const XMLNode& node)
	: Region (srcs, node)
{
	if (set_state (node, Stateful::loading_state_version)) {
		throw failed_constructor();
	}

	midi_source(0)->Switched.connect(sigc::mem_fun(this, &MidiRegion::switch_source));
	assert(_name.find("/") == string::npos);
	assert(_type == DataType::MIDI);
}

MidiRegion::~MidiRegion ()
{
}

void
MidiRegion::set_position_internal (nframes_t pos, bool allow_bbt_recompute)
{
	BeatsFramesConverter old_converter(_session.tempo_map(), _position - _start);
	double length_beats = old_converter.from(_length);

	Region::set_position_internal(pos, allow_bbt_recompute);

	BeatsFramesConverter new_converter(_session.tempo_map(), pos - _start);

	set_length(new_converter.to(length_beats), 0);
}

nframes_t
MidiRegion::read_at (Evoral::EventSink<nframes_t>& out, sframes_t position, nframes_t dur, uint32_t chan_n, NoteMode mode, MidiStateTracker* tracker) const
{
	return _read_at (_sources, out, position, dur, chan_n, mode, tracker);
}

nframes_t
MidiRegion::master_read_at (MidiRingBuffer<nframes_t>& out, sframes_t position, nframes_t dur, uint32_t chan_n, NoteMode mode) const
{
	return _read_at (_master_sources, out, position, dur, chan_n, mode); /* no tracker */
}

nframes_t
MidiRegion::_read_at (const SourceList& /*srcs*/, Evoral::EventSink<nframes_t>& dst, sframes_t position, nframes_t dur, uint32_t chan_n, 
		      NoteMode mode, MidiStateTracker* tracker) const
{
	nframes_t internal_offset = 0;
	nframes_t src_offset      = 0;
	nframes_t to_read         = 0;

	/* precondition: caller has verified that we cover the desired section */

	assert(chan_n == 0);

	if (muted()) {
		return 0; /* read nothing */
	}

	if (position < _position) {
		internal_offset = 0;
		src_offset = _position - position;
		dur -= src_offset;
	} else {
		internal_offset = position - _position;
		src_offset = 0;
	}

	if (internal_offset >= _length) {
		return 0; /* read nothing */
	}

	if ((to_read = min (dur, _length - internal_offset)) == 0) {
		return 0; /* read nothing */
	}

	_read_data_count = 0;

	boost::shared_ptr<MidiSource> src = midi_source(chan_n);
	src->set_note_mode(mode);

	nframes_t output_buffer_position = 0;
	nframes_t negative_output_buffer_position = 0;
	if (_position >= _start) {
		// handle resizing of beginnings of regions correctly
		output_buffer_position = _position - _start;
	} else {
		// when _start is greater than _position, we have to subtract
		// _start from the note times in the midi source
		negative_output_buffer_position = _start;
	}

	/*cerr << "MR read @ " << position << " * " << to_read
		<< " _position = " << _position
	    << " _start = " << _start
	    << " offset = " << output_buffer_position
	    << " negoffset = " << negative_output_buffer_position
	    << " intoffset = " << internal_offset
	    << endl;*/

	if (src->midi_read (
			dst, // destination buffer
			_position - _start, // start position of the source in this read context
			_start + internal_offset, // where to start reading in the source
			to_read, // read duration in frames
			output_buffer_position, // the offset in the output buffer
			negative_output_buffer_position, // amount to substract from note times
			tracker
		    ) != to_read) {
		return 0; /* "read nothing" */
	}

	_read_data_count += src->read_data_count();

	return to_read;
}

XMLNode&
MidiRegion::state (bool full)
{
	XMLNode& node (Region::state (full));
	char buf[64];
	char buf2[64];
	LocaleGuard lg (X_("POSIX"));

	node.add_property ("flags", enum_2_string (_flags));

	// XXX these should move into Region

	for (uint32_t n=0; n < _sources.size(); ++n) {
		snprintf (buf2, sizeof(buf2), "source-%d", n);
		_sources[n]->id().print (buf, sizeof(buf));
		node.add_property (buf2, buf);
	}

	for (uint32_t n=0; n < _master_sources.size(); ++n) {
		snprintf (buf2, sizeof(buf2), "master-source-%d", n);
		_master_sources[n]->id().print (buf, sizeof (buf));
		node.add_property (buf2, buf);
	}

	if (full && _extra_xml) {
		node.add_child_copy (*_extra_xml);
	}

	return node;
}

int
MidiRegion::set_live_state (const XMLNode& node, int version, Change& what_changed, bool send)
{
	const XMLProperty *prop;
	LocaleGuard lg (X_("POSIX"));

	Region::set_live_state (node, version, what_changed, false);

	uint32_t old_flags = _flags;

	if ((prop = node.property ("flags")) != 0) {
		_flags = Flag (string_2_enum (prop->value(), _flags));

		//_flags = Flag (strtol (prop->value().c_str(), (char **) 0, 16));

		_flags = Flag (_flags & ~Region::LeftOfSplit);
		_flags = Flag (_flags & ~Region::RightOfSplit);
	}

	if ((old_flags ^ _flags) & Muted) {
		what_changed = Change (what_changed|MuteChanged);
	}
	if ((old_flags ^ _flags) & Opaque) {
		what_changed = Change (what_changed|OpacityChanged);
	}
	if ((old_flags ^ _flags) & Locked) {
		what_changed = Change (what_changed|LockChanged);
	}

	if (send) {
		send_change (what_changed);
	}

	return 0;
}

int
MidiRegion::set_state (const XMLNode& node, int version)
{
	/* Region::set_state() calls the virtual set_live_state(),
	   which will get us back to AudioRegion::set_live_state()
	   to handle the relevant stuff.
	*/

	return Region::set_state (node, version);
}

void
MidiRegion::recompute_at_end ()
{
	/* our length has changed
	 * (non destructively) "chop" notes that pass the end boundary, to
	 * prevent stuck notes.
	 */
}

void
MidiRegion::recompute_at_start ()
{
	/* as above, but the shift was from the front
	 * maybe bump currently active note's note-ons up so they sound here?
	 * that could be undesireable in certain situations though.. maybe
	 * remove the note entirely, including it's note off?  something needs to
	 * be done to keep the played MIDI sane to avoid messing up voices of
	 * polyhonic things etc........
	 */
}

int
MidiRegion::separate_by_channel (ARDOUR::Session&, vector< boost::shared_ptr<Region> >&) const
{
	// TODO
	return -1;
}

int
MidiRegion::exportme (ARDOUR::Session&, ARDOUR::ExportSpecification&)
{
	return -1;
}

boost::shared_ptr<MidiSource>
MidiRegion::midi_source (uint32_t n) const
{
	// Guaranteed to succeed (use a static cast?)
	return boost::dynamic_pointer_cast<MidiSource>(source(n));
}


void
MidiRegion::switch_source(boost::shared_ptr<Source> src)
{
	boost::shared_ptr<MidiSource> msrc = boost::dynamic_pointer_cast<MidiSource>(src);
	if (!msrc)
		return;

	// MIDI regions have only one source
	_sources.clear();
	_sources.push_back(msrc);

	set_name(msrc->name());
}

