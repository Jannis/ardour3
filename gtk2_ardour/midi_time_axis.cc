/*
    Copyright (C) 2000 Paul Davis

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

#include <cstdlib>
#include <cmath>

#include <algorithm>
#include <string>
#include <vector>

#include <sigc++/bind.h>

#include "pbd/error.h"
#include "pbd/stl_delete.h"
#include "pbd/whitespace.h"
#include "pbd/basename.h"
#include "pbd/enumwriter.h"
#include "pbd/memento_command.h"

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/selector.h>
#include <gtkmm2ext/stop_signal.h>
#include <gtkmm2ext/bindable_button.h>
#include <gtkmm2ext/utils.h>

#include "ardour/midi_playlist.h"
#include "ardour/midi_diskstream.h"
#include "ardour/midi_patch_manager.h"
#include "ardour/midi_source.h"
#include "ardour/processor.h"
#include "ardour/ladspa_plugin.h"
#include "ardour/location.h"
#include "ardour/playlist.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/session_playlist.h"
#include "ardour/tempo.h"
#include "ardour/utils.h"

#include "add_midi_cc_track_dialog.h"
#include "ardour_ui.h"
#include "automation_line.h"
#include "automation_time_axis.h"
#include "canvas-note-event.h"
#include "canvas_impl.h"
#include "crossfade_view.h"
#include "editor.h"
#include "enums.h"
#include "ghostregion.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "midi_scroomer.h"
#include "midi_streamview.h"
#include "midi_region_view.h"
#include "midi_time_axis.h"
#include "piano_roll_header.h"
#include "playlist_selector.h"
#include "plugin_selector.h"
#include "plugin_ui.h"
#include "point_selection.h"
#include "prompter.h"
#include "region_view.h"
#include "rgb_macros.h"
#include "selection.h"
#include "simplerect.h"
#include "utils.h"

#include "ardour/midi_track.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Editing;

// Minimum height at which a control is displayed
static const uint32_t MIDI_CONTROLS_BOX_MIN_HEIGHT = 162;
static const uint32_t KEYBOARD_MIN_HEIGHT = 140;

MidiTimeAxisView::MidiTimeAxisView (PublicEditor& ed, Session* sess,
		boost::shared_ptr<Route> rt, Canvas& canvas)
	: AxisView(sess) // virtually inherited
	, RouteTimeAxisView(ed, sess, rt, canvas)
	, _ignore_signals(false)
	, _range_scroomer(0)
	, _piano_roll_header(0)
	, _note_mode(Sustained)
	, _note_mode_item(0)
	, _percussion_mode_item(0)
	, _color_mode(MeterColors)
	, _meter_color_mode_item(0)
	, _channel_color_mode_item(0)
	, _track_color_mode_item(0)
	, _step_edit_item (0)
	, _midi_thru_item (0)
	, default_channel_menu (0)
{
	subplugin_menu.set_name ("ArdourContextMenu");

	_view = new MidiStreamView (*this);

	ignore_toggle = false;

	mute_button->set_active (false);
	solo_button->set_active (false);

	step_edit_insert_position = 0;

	if (is_midi_track()) {
		controls_ebox.set_name ("MidiTimeAxisViewControlsBaseUnselected");
		_note_mode = midi_track()->note_mode();
	} else { // MIDI bus (which doesn't exist yet..)
		controls_ebox.set_name ("MidiBusControlsBaseUnselected");
	}

	/* map current state of the route */

	processors_changed (RouteProcessorChange ());

	ensure_xml_node ();

	set_state (*xml_node, Stateful::loading_state_version);

	_route->processors_changed.connect (*this, boost::bind (&MidiTimeAxisView::processors_changed, this, _1));

	if (is_track()) {
		_piano_roll_header = new PianoRollHeader(*midi_view());
		_range_scroomer = new MidiScroomer(midi_view()->note_range_adjustment);

		controls_hbox.pack_start(*_range_scroomer);
		controls_hbox.pack_start(*_piano_roll_header);

		controls_ebox.set_name ("MidiTrackControlsBaseUnselected");
		controls_base_selected_name = "MidiTrackControlsBaseSelected";
		controls_base_unselected_name = "MidiTrackControlsBaseUnselected";

		midi_view()->NoteRangeChanged.connect (sigc::mem_fun(*this, &MidiTimeAxisView::update_range));

		/* ask for notifications of any new RegionViews */
		_view->RegionViewAdded.connect (sigc::mem_fun(*this, &MidiTimeAxisView::region_view_added));
		_view->attach ();
	}

	HBox* midi_controls_hbox = manage(new HBox());

	MIDI::Name::MidiPatchManager& patch_manager = MIDI::Name::MidiPatchManager::instance();

	MIDI::Name::MasterDeviceNames::Models::const_iterator m = patch_manager.all_models().begin();
	for (; m != patch_manager.all_models().end(); ++m) {
		_model_selector.append_text(m->c_str());
	}

	_model_selector.signal_changed().connect(sigc::mem_fun(*this, &MidiTimeAxisView::model_changed));

	_custom_device_mode_selector.signal_changed().connect(
			sigc::mem_fun(*this, &MidiTimeAxisView::custom_device_mode_changed));

	// TODO: persist the choice
	// this initializes the comboboxes and sends out the signal
	_model_selector.set_active(0);

	midi_controls_hbox->pack_start(_channel_selector, true, false);
	if (!patch_manager.all_models().empty()) {
		_midi_controls_box.pack_start(_model_selector, true, false);
		_midi_controls_box.pack_start(_custom_device_mode_selector, true, false);
	}

	_midi_controls_box.pack_start(*midi_controls_hbox, true, true);

	controls_vbox.pack_start(_midi_controls_box, false, false);

	boost::shared_ptr<MidiDiskstream> diskstream = midi_track()->midi_diskstream();

	// restore channel selector settings
	_channel_selector.set_channel_mode(diskstream->get_channel_mode(),
			diskstream->get_channel_mask());
	_channel_selector.mode_changed.connect(
		sigc::mem_fun(*midi_track()->midi_diskstream(), &MidiDiskstream::set_channel_mode));

	XMLProperty *prop;
	if ((prop = xml_node->property ("color-mode")) != 0) {
		_color_mode = ColorMode (string_2_enum(prop->value(), _color_mode));
		if (_color_mode == ChannelColors) {
			_channel_selector.set_channel_colors(CanvasNoteEvent::midi_channel_colors);
		}
	}

	if ((prop = xml_node->property ("note-mode")) != 0) {
		_note_mode = NoteMode (string_2_enum(prop->value(), _note_mode));
		if (_percussion_mode_item) {
			_percussion_mode_item->set_active (_note_mode == Percussive);
		}
	}
}

MidiTimeAxisView::~MidiTimeAxisView ()
{
	delete _piano_roll_header;
	_piano_roll_header = 0;

	delete _range_scroomer;
	_range_scroomer = 0;
}

void MidiTimeAxisView::model_changed()
{
	std::list<std::string> device_modes = MIDI::Name::MidiPatchManager::instance()
		.custom_device_mode_names_by_model(_model_selector.get_active_text());

	_custom_device_mode_selector.clear_items();

	for (std::list<std::string>::const_iterator i = device_modes.begin();
			i != device_modes.end(); ++i) {
		cerr << "found custom device mode " << *i << " thread_id: " << pthread_self() << endl;
		_custom_device_mode_selector.append_text(*i);
	}

	_custom_device_mode_selector.set_active(0);
}

void MidiTimeAxisView::custom_device_mode_changed()
{
	_midi_patch_settings_changed.emit(_model_selector.get_active_text(),
			_custom_device_mode_selector.get_active_text());
}

MidiStreamView*
MidiTimeAxisView::midi_view()
{
	return dynamic_cast<MidiStreamView*>(_view);
}

guint32
MidiTimeAxisView::show_at (double y, int& nth, Gtk::VBox *parent)
{
	ensure_xml_node ();
	xml_node->add_property ("shown-editor", "yes");

	guint32 ret = TimeAxisView::show_at (y, nth, parent);
	return ret;
}

void
MidiTimeAxisView::hide ()
{
	ensure_xml_node ();
	xml_node->add_property ("shown-editor", "no");

	TimeAxisView::hide ();
}

void
MidiTimeAxisView::set_height (uint32_t h)
{
	RouteTimeAxisView::set_height (h);

	if (height >= MIDI_CONTROLS_BOX_MIN_HEIGHT) {
		_midi_controls_box.show();
	} else {
		_midi_controls_box.hide();
	}

	if (height >= KEYBOARD_MIN_HEIGHT) {
		if (is_track() && _range_scroomer)
			_range_scroomer->show();
		if (is_track() && _piano_roll_header)
			_piano_roll_header->show();
	} else {
		if (is_track() && _range_scroomer)
			_range_scroomer->hide();
		if (is_track() && _piano_roll_header)
			_piano_roll_header->hide();
	}
}

void
MidiTimeAxisView::append_extra_display_menu_items ()
{
	using namespace Menu_Helpers;

	MenuList& items = display_menu->items();

	// Note range
	Menu *range_menu = manage(new Menu);
	MenuList& range_items = range_menu->items();
	range_menu->set_name ("ArdourContextMenu");

	range_items.push_back (MenuElem (_("Show Full Range"), sigc::bind (
			sigc::mem_fun(*this, &MidiTimeAxisView::set_note_range),
			MidiStreamView::FullRange)));

	range_items.push_back (MenuElem (_("Fit Contents"), sigc::bind (
			sigc::mem_fun(*this, &MidiTimeAxisView::set_note_range),
			MidiStreamView::ContentsRange)));

	items.push_back (MenuElem (_("Note range"), *range_menu));
	items.push_back (MenuElem (_("Note mode"), *build_note_mode_menu()));
	items.push_back (MenuElem (_("Default Channel"), *build_def_channel_menu()));

	items.push_back (CheckMenuElem (_("MIDI Thru"), sigc::mem_fun(*this, &MidiTimeAxisView::toggle_midi_thru)));
	_midi_thru_item = dynamic_cast<CheckMenuItem*>(&items.back());
}

Gtk::Menu*
MidiTimeAxisView::build_def_channel_menu ()
{
	using namespace Menu_Helpers;

	if (default_channel_menu == 0) {
		default_channel_menu = manage (new Menu ());
	}

	uint8_t defchn = midi_track()->default_channel();
	MenuList& def_channel_items = default_channel_menu->items();
	RadioMenuItem* item;
	RadioMenuItem::Group dc_group;

	for (int i = 0; i < 16; ++i) {
		char buf[4];
		snprintf (buf, sizeof (buf), "%d", i+1);

		def_channel_items.push_back (RadioMenuElem (dc_group, buf,
							    sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::set_default_channel), i)));
		item = dynamic_cast<RadioMenuItem*>(&def_channel_items.back());
		item->set_active ((i == defchn));
	}

	return default_channel_menu;
}

void
MidiTimeAxisView::set_default_channel (int chn)
{
	midi_track()->set_default_channel (chn);
}

void
MidiTimeAxisView::toggle_midi_thru ()
{
	if (!_midi_thru_item) {
		return;
	}

	bool view_yn = _midi_thru_item->get_active();
	if (view_yn != midi_track()->midi_thru()) {
		midi_track()->set_midi_thru (view_yn);
	}
}

void
MidiTimeAxisView::build_automation_action_menu ()
{
	using namespace Menu_Helpers;

	RouteTimeAxisView::build_automation_action_menu ();

	MenuList& automation_items = automation_action_menu->items();

	automation_items.push_back (SeparatorElem());
	automation_items.push_back (MenuElem (_("Controller..."),
			sigc::mem_fun(*this, &MidiTimeAxisView::add_cc_track)));
	automation_items.push_back (MenuElem (_("Program Change"),
			sigc::bind(sigc::mem_fun(*this, &MidiTimeAxisView::add_parameter_track),
				Evoral::Parameter(MidiPgmChangeAutomation))));
	automation_items.push_back (MenuElem (_("Bender"),
			sigc::bind(sigc::mem_fun(*this, &MidiTimeAxisView::add_parameter_track),
				Evoral::Parameter(MidiPitchBenderAutomation))));
	automation_items.push_back (MenuElem (_("Pressure"),
			sigc::bind(sigc::mem_fun(*this, &MidiTimeAxisView::add_parameter_track),
				Evoral::Parameter(MidiChannelPressureAutomation))));
}

Gtk::Menu*
MidiTimeAxisView::build_note_mode_menu()
{
	using namespace Menu_Helpers;

	Menu* mode_menu = manage (new Menu);
	MenuList& items = mode_menu->items();
	mode_menu->set_name ("ArdourContextMenu");

	RadioMenuItem::Group mode_group;
	items.push_back (RadioMenuElem (mode_group, _("Sustained"),
				sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::set_note_mode), Sustained)));
	_note_mode_item = dynamic_cast<RadioMenuItem*>(&items.back());
	_note_mode_item->set_active(_note_mode == Sustained);

	items.push_back (RadioMenuElem (mode_group, _("Percussive"),
				sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::set_note_mode), Percussive)));
	_percussion_mode_item = dynamic_cast<RadioMenuItem*>(&items.back());
	_percussion_mode_item->set_active(_note_mode == Percussive);

	return mode_menu;
}

Gtk::Menu*
MidiTimeAxisView::build_color_mode_menu()
{
	using namespace Menu_Helpers;

	Menu* mode_menu = manage (new Menu);
	MenuList& items = mode_menu->items();
	mode_menu->set_name ("ArdourContextMenu");

	RadioMenuItem::Group mode_group;
	items.push_back (RadioMenuElem (mode_group, _("Meter Colors"),
				sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::set_color_mode), MeterColors)));
	_meter_color_mode_item = dynamic_cast<RadioMenuItem*>(&items.back());
	_meter_color_mode_item->set_active(_color_mode == MeterColors);

	items.push_back (RadioMenuElem (mode_group, _("Channel Colors"),
				sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::set_color_mode), ChannelColors)));
	_channel_color_mode_item = dynamic_cast<RadioMenuItem*>(&items.back());
	_channel_color_mode_item->set_active(_color_mode == ChannelColors);

	items.push_back (RadioMenuElem (mode_group, _("Track Color"),
				sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::set_color_mode), TrackColor)));
	_channel_color_mode_item = dynamic_cast<RadioMenuItem*>(&items.back());
	_channel_color_mode_item->set_active(_color_mode == TrackColor);

	return mode_menu;
}

void
MidiTimeAxisView::set_note_mode(NoteMode mode)
{
	if (_note_mode != mode || midi_track()->note_mode() != mode) {
		_note_mode = mode;
		midi_track()->set_note_mode(mode);
		xml_node->add_property ("note-mode", enum_2_string(_note_mode));
		_view->redisplay_diskstream();
	}
}

void
MidiTimeAxisView::set_color_mode(ColorMode mode)
{
	if (_color_mode != mode) {
		if (mode == ChannelColors) {
			_channel_selector.set_channel_colors(CanvasNoteEvent::midi_channel_colors);
		} else {
			_channel_selector.set_default_channel_color();
		}

		_color_mode = mode;
		xml_node->add_property ("color-mode", enum_2_string(_color_mode));
		_view->redisplay_diskstream();
	}
}

void
MidiTimeAxisView::set_note_range(MidiStreamView::VisibleNoteRange range)
{
	if (!_ignore_signals)
		midi_view()->set_note_range(range);
}


void
MidiTimeAxisView::update_range()
{
	MidiGhostRegion* mgr;

	for(list<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
		if ((mgr = dynamic_cast<MidiGhostRegion*>(*i)) != 0) {
			mgr->update_range();
		}
	}
}

void
MidiTimeAxisView::show_all_automation ()
{
	if (midi_track()) {
		const set<Evoral::Parameter> params = midi_track()->midi_diskstream()->
				midi_playlist()->contained_automation();

		for (set<Evoral::Parameter>::const_iterator i = params.begin(); i != params.end(); ++i) {
			create_automation_child(*i, true);
		}
	}

	RouteTimeAxisView::show_all_automation ();
}

void
MidiTimeAxisView::show_existing_automation ()
{
	if (midi_track()) {
		const set<Evoral::Parameter> params = midi_track()->midi_diskstream()->
				midi_playlist()->contained_automation();

		for (set<Evoral::Parameter>::const_iterator i = params.begin(); i != params.end(); ++i) {
			create_automation_child(*i, true);
		}
	}

	RouteTimeAxisView::show_existing_automation ();
}

/** Prompt for a controller with a dialog and add an automation track for it
 */
void
MidiTimeAxisView::add_cc_track()
{
	int response;
	Evoral::Parameter param(0, 0, 0);

	{
		AddMidiCCTrackDialog dialog;
		dialog.set_transient_for (_editor);
		response = dialog.run();

		if (response == Gtk::RESPONSE_ACCEPT)
			param = dialog.parameter();
	}

	if (param.type() != 0 && response == Gtk::RESPONSE_ACCEPT)
		create_automation_child(param, true);
}


/** Add an automation track for the given parameter (pitch bend, channel pressure).
 */
void
MidiTimeAxisView::add_parameter_track(const Evoral::Parameter& param)
{
	if ( !EventTypeMap::instance().is_midi_parameter(param) ) {
		error << "MidiTimeAxisView: unknown automation child "
			<< ARDOUR::EventTypeMap::instance().to_symbol(param) << endmsg;
		return;
	}

	// create the parameter lane for each selected channel
	uint16_t selected_channels = _channel_selector.get_selected_channels();

	for (uint8_t i = 0; i < 16; i++) {
		if (selected_channels & (0x0001 << i)) {
			Evoral::Parameter param_with_channel(param.type(), i, param.id());
			create_automation_child(param_with_channel, true);
		}
	}
}

void
MidiTimeAxisView::create_automation_child (const Evoral::Parameter& param, bool show)
{
	/* These controllers are region "automation", so we do not create
	 * an AutomationList/Line for the track */

	if (param.type() == NullAutomation) {
		cerr << "WARNING: Attempt to create NullAutomation child, ignoring" << endl;
		return;
	}

	AutomationTracks::iterator existing = _automation_tracks.find(param);
	if (existing != _automation_tracks.end())
		return;

	boost::shared_ptr<AutomationControl> c = _route->get_control (param);

	assert(c);

	boost::shared_ptr<AutomationTimeAxisView> track(new AutomationTimeAxisView (_session,
			_route, boost::shared_ptr<ARDOUR::Automatable>(), c,
			_editor,
			*this,
			true,
			parent_canvas,
			_route->describe_parameter(param)));

	add_automation_child(param, track, show);
}


void
MidiTimeAxisView::route_active_changed ()
{
	RouteUI::route_active_changed ();

	if (is_track()) {
		if (_route->active()) {
			controls_ebox.set_name ("MidiTrackControlsBaseUnselected");
			controls_base_selected_name = "MidiTrackControlsBaseSelected";
			controls_base_unselected_name = "MidiTrackControlsBaseUnselected";
		} else {
			controls_ebox.set_name ("MidiTrackControlsBaseInactiveUnselected");
			controls_base_selected_name = "MidiTrackControlsBaseInactiveSelected";
			controls_base_unselected_name = "MidiTrackControlsBaseInactiveUnselected";
		}
	} else {

		throw; // wha?

		if (_route->active()) {
			controls_ebox.set_name ("BusControlsBaseUnselected");
			controls_base_selected_name = "BusControlsBaseSelected";
			controls_base_unselected_name = "BusControlsBaseUnselected";
		} else {
			controls_ebox.set_name ("BusControlsBaseInactiveUnselected");
			controls_base_selected_name = "BusControlsBaseInactiveSelected";
			controls_base_unselected_name = "BusControlsBaseInactiveUnselected";
		}
	}
}

void
MidiTimeAxisView::start_step_editing ()
{
	step_edit_insert_position = _editor.get_preferred_edit_position ();
	step_edit_beat_pos = 0;
	step_edit_region = playlist()->top_region_at (step_edit_insert_position);

	if (step_edit_region) {
		RegionView* rv = view()->find_view (step_edit_region);
		step_edit_region_view = dynamic_cast<MidiRegionView*> (rv);
	} else {
		step_edit_region_view = 0;
	}

	midi_track()->set_step_editing (true);
}

void
MidiTimeAxisView::stop_step_editing ()
{
	midi_track()->set_step_editing (false);
}

void
MidiTimeAxisView::check_step_edit ()
{
	MidiRingBuffer<nframes_t>& incoming (midi_track()->step_edit_ring_buffer());
	Evoral::Note<Evoral::MusicalTime> note;
	uint8_t* buf;
	uint32_t bufsize = 32;

	buf = new uint8_t[bufsize];

	while (incoming.read_space()) {
		nframes_t time;
		Evoral::EventType type;
		uint32_t size;

		incoming.read_prefix (&time, &type, &size);

		if (size > bufsize) {
			delete [] buf;
			bufsize = size;
			buf = new uint8_t[bufsize];
		}

		incoming.read_contents (size, buf);

		if ((buf[0] & 0xf0) == MIDI_CMD_NOTE_ON) {

			if (step_edit_region == 0) {

				step_edit_region = add_region (step_edit_insert_position);
				RegionView* rv = view()->find_view (step_edit_region);

				if (rv) {
					step_edit_region_view = dynamic_cast<MidiRegionView*>(rv);
				} else {
					fatal << X_("programming error: no view found for new MIDI region") << endmsg;
					/*NOTREACHED*/
				}
			}

			if (step_edit_region_view) {

				bool success;
				Evoral::MusicalTime beats = _editor.get_grid_type_as_beats (success, step_edit_insert_position);

				if (!success) {
					continue;
				}

				step_edit_region_view->add_note (buf[0] & 0xf, buf[1], buf[2], step_edit_beat_pos, beats);
				step_edit_beat_pos += beats;
			}
		}

	}
}

void
MidiTimeAxisView::step_edit_rest ()
{
	bool success;
	Evoral::MusicalTime beats = _editor.get_grid_type_as_beats (success, step_edit_insert_position);
	step_edit_beat_pos += beats;
}

boost::shared_ptr<Region>
MidiTimeAxisView::add_region (nframes64_t pos)
{
	Editor* real_editor = dynamic_cast<Editor*> (&_editor);

	real_editor->begin_reversible_command (_("create region"));
	XMLNode &before = playlist()->get_state();

	nframes64_t start = pos;
	real_editor->snap_to (start, -1);
	const Meter& m = _session->tempo_map().meter_at(start);
	const Tempo& t = _session->tempo_map().tempo_at(start);
	double length = floor (m.frames_per_bar(t, _session->frame_rate()));

	const boost::shared_ptr<MidiDiskstream> diskstream =
		boost::dynamic_pointer_cast<MidiDiskstream>(view()->trackview().track()->diskstream());

	boost::shared_ptr<Source> src = _session->create_midi_source_for_session (*diskstream.get());

	boost::shared_ptr<Region> region = (RegionFactory::create (src, 0, (nframes_t) length,
								   PBD::basename_nosuffix(src->name())));

	playlist()->add_region (region, start);
	XMLNode &after = playlist()->get_state();
	_session->add_command (new MementoCommand<Playlist> (*playlist().get(), &before, &after));

	real_editor->commit_reversible_command();

	return region;
}
