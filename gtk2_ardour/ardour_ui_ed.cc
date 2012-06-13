/*
    Copyright (C) 20002-2004 Paul Davis

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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

/* This file contains any ARDOUR_UI methods that require knowledge of
   the editor, and exists so that no compilation dependency exists
   between the main ARDOUR_UI modules and the PublicEditor class. This
   is to cut down on the nasty compile times for both these classes.
*/

#include <cmath>

#include <glibmm/miscutils.h>
#include <gtk/gtk.h>

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/window_title.h"
#include "gtkmm2ext/tearoff.h"
#include "gtkmm2ext/cairo_packer.h"

#include "pbd/file_utils.h"
#include "pbd/fpu.h"
#include "pbd/convert.h"

#include "ardour_ui.h"
#include "public_editor.h"
#include "audio_clock.h"
#include "keyboard.h"
#include "monitor_section.h"
#include "engine_dialog.h"
#include "editor.h"
#include "actions.h"
#include "mixer_ui.h"
#include "startup.h"
#include "utils.h"
#include "window_proxy.h"
#include "global_port_matrix.h"
#include "location_ui.h"
#include "main_clock.h"

#include <gtkmm2ext/application.h>

#include "ardour/session.h"
#include "ardour/profile.h"
#include "ardour/audioengine.h"

#include "control_protocol/control_protocol.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace Glib;

int
ARDOUR_UI::create_editor ()
{
	try {
		editor = new Editor ();
	}

	catch (failed_constructor& err) {
		return -1;
	}

	editor->Realized.connect (sigc::mem_fun (*this, &ARDOUR_UI::editor_realized));
	editor->signal_window_state_event().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::main_window_state_event_handler), true));

	return 0;
}

void
ARDOUR_UI::install_actions ()
{
	Glib::RefPtr<ActionGroup> main_actions = ActionGroup::create (X_("Main"));
	Glib::RefPtr<ActionGroup> main_menu_actions = ActionGroup::create (X_("Main_menu"));
	Glib::RefPtr<Action> act;

	/* menus + submenus that need action items */

	ActionManager::register_action (main_menu_actions, X_("Session"), _("Session"));
	act = ActionManager::register_action (main_menu_actions, X_("Cleanup"), _("Clean-up"));
	ActionManager::write_sensitive_actions.push_back (act);
	ActionManager::register_action (main_menu_actions, X_("Sync"), _("Sync"));
	ActionManager::register_action (main_menu_actions, X_("TransportOptions"), _("Options"));
	ActionManager::register_action (main_menu_actions, X_("WindowMenu"), _("Window"));
	ActionManager::register_action (main_menu_actions, X_("Help"), _("Help"));
 	ActionManager::register_action (main_menu_actions, X_("KeyMouseActions"), _("Misc. Shortcuts"));
	ActionManager::register_action (main_menu_actions, X_("AudioFileFormat"), _("Audio File Format"));
	ActionManager::register_action (main_menu_actions, X_("AudioFileFormatHeader"), _("File Type"));
	ActionManager::register_action (main_menu_actions, X_("AudioFileFormatData"), _("Sample Format"));
	ActionManager::register_action (main_menu_actions, X_("ControlSurfaces"), _("Control Surfaces"));
	ActionManager::register_action (main_menu_actions, X_("Plugins"), _("Plugins"));
	ActionManager::register_action (main_menu_actions, X_("Metering"), _("Metering"));
	ActionManager::register_action (main_menu_actions, X_("MeteringFallOffRate"), _("Fall Off Rate"));
	ActionManager::register_action (main_menu_actions, X_("MeteringHoldTime"), _("Hold Time"));
	ActionManager::register_action (main_menu_actions, X_("Denormals"), _("Denormal Handling"));

	/* the real actions */

	act = ActionManager::register_action (main_actions, X_("New"), _("New..."),  hide_return (sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::get_session_parameters), false, true, "")));

	ActionManager::register_action (main_actions, X_("Open"), _("Open..."),  sigc::mem_fun(*this, &ARDOUR_UI::open_session));
	ActionManager::register_action (main_actions, X_("Recent"), _("Recent..."),  sigc::mem_fun(*this, &ARDOUR_UI::open_recent_session));
	act = ActionManager::register_action (main_actions, X_("Close"), _("Close"),  sigc::mem_fun(*this, &ARDOUR_UI::close_session));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("AddTrackBus"), _("Add Track or Bus..."),
					      sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::add_route), (Gtk::Window*) 0));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);

#ifdef WITH_CMT

	sys::path anicomp_file_path;

	if (PBD::find_file_in_search_path (Glib::getenv("PATH"), "AniComp", anicomp_file_path)) {
		act = ActionManager::register_action (main_actions, X_("aniConnect"), _("Connect"),  (sigc::mem_fun (*editor, &PublicEditor::connect_to_image_compositor)));
		ActionManager::session_sensitive_actions.push_back (act);
	}

#endif

	act = ActionManager::register_action (main_actions, X_("Snapshot"), _("Snapshot..."), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::snapshot_session), false));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("SaveAs"), _("Save As..."), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::snapshot_session), true));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("Rename"), _("Rename..."), sigc::mem_fun(*this, &ARDOUR_UI::rename_session));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("SaveTemplate"), _("Save Template..."),  sigc::mem_fun(*this, &ARDOUR_UI::save_template));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("Metadata"), _("Metadata"));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("EditMetadata"), _("Edit Metadata..."),  sigc::mem_fun(*this, &ARDOUR_UI::edit_metadata));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("ImportMetadata"), _("Import Metadata..."),  sigc::mem_fun(*this, &ARDOUR_UI::import_metadata));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("ExportAudio"), _("Export To Audio File(s)..."),  sigc::mem_fun (*editor, &PublicEditor::export_audio));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("StemExport"), _("Stem export..."),  sigc::mem_fun (*editor, &PublicEditor::stem_export));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("Export"), _("Export"));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("CleanupUnused"), _("Clean-up Unused Sources..."),  sigc::mem_fun (*(ARDOUR_UI::instance()), &ARDOUR_UI::cleanup));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("FlushWastebasket"), _("Flush Wastebasket"),  sigc::mem_fun (*(ARDOUR_UI::instance()), &ARDOUR_UI::flush_trash));
	ActionManager::write_sensitive_actions.push_back (act);
	ActionManager::session_sensitive_actions.push_back (act);

	/* JACK actions for controlling ... JACK */

	Glib::RefPtr<ActionGroup> jack_actions = ActionGroup::create (X_("JACK"));
	ActionManager::register_action (jack_actions, X_("JACK"), _("JACK"));
	ActionManager::register_action (jack_actions, X_("Latency"), _("Latency"));

	act = ActionManager::register_action (jack_actions, X_("JACKReconnect"), _("Reconnect"), sigc::mem_fun (*(ARDOUR_UI::instance()), &ARDOUR_UI::reconnect_to_jack));
	ActionManager::jack_opposite_sensitive_actions.push_back (act);

	act = ActionManager::register_action (jack_actions, X_("JACKDisconnect"), _("Disconnect"), sigc::mem_fun (*(ARDOUR_UI::instance()), &ARDOUR_UI::disconnect_from_jack));
	ActionManager::jack_sensitive_actions.push_back (act);

	RadioAction::Group jack_latency_group;

	act = ActionManager::register_radio_action (jack_actions, jack_latency_group, X_("JACKLatency32"), X_("32"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::set_jack_buffer_size), (pframes_t) 32));
	ActionManager::jack_sensitive_actions.push_back (act);
	act = ActionManager::register_radio_action (jack_actions, jack_latency_group, X_("JACKLatency64"), X_("64"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::set_jack_buffer_size), (pframes_t) 64));
	ActionManager::jack_sensitive_actions.push_back (act);
	act = ActionManager::register_radio_action (jack_actions, jack_latency_group, X_("JACKLatency128"), X_("128"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::set_jack_buffer_size), (pframes_t) 128));
	ActionManager::jack_sensitive_actions.push_back (act);
	act = ActionManager::register_radio_action (jack_actions, jack_latency_group, X_("JACKLatency256"), X_("256"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::set_jack_buffer_size), (pframes_t) 256));
	ActionManager::jack_sensitive_actions.push_back (act);
	act = ActionManager::register_radio_action (jack_actions, jack_latency_group, X_("JACKLatency512"), X_("512"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::set_jack_buffer_size), (pframes_t) 512));
	ActionManager::jack_sensitive_actions.push_back (act);
	act = ActionManager::register_radio_action (jack_actions, jack_latency_group, X_("JACKLatency1024"), X_("1024"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::set_jack_buffer_size), (pframes_t) 1024));
	ActionManager::jack_sensitive_actions.push_back (act);
	act = ActionManager::register_radio_action (jack_actions, jack_latency_group, X_("JACKLatency2048"), X_("2048"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::set_jack_buffer_size), (pframes_t) 2048));
	ActionManager::jack_sensitive_actions.push_back (act);
	act = ActionManager::register_radio_action (jack_actions, jack_latency_group, X_("JACKLatency4096"), X_("4096"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::set_jack_buffer_size), (pframes_t) 4096));
	ActionManager::jack_sensitive_actions.push_back (act);
	act = ActionManager::register_radio_action (jack_actions, jack_latency_group, X_("JACKLatency8192"), X_("8192"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::set_jack_buffer_size), (pframes_t) 8192));
	ActionManager::jack_sensitive_actions.push_back (act);

	/* these actions are intended to be shared across all windows */

	common_actions = ActionGroup::create (X_("Common"));
	ActionManager::register_action (common_actions, X_("Quit"), _("Quit"), (hide_return (sigc::mem_fun(*this, &ARDOUR_UI::finish))));

	/* windows visibility actions */

	ActionManager::register_toggle_action (common_actions, X_("ToggleMaximalEditor"), _("Maximise Editor Space"), sigc::mem_fun (*this, &ARDOUR_UI::toggle_editing_space));
	act = ActionManager::register_toggle_action (common_actions, X_("KeepTearoffs"), _("Show Toolbars"), mem_fun (*this, &ARDOUR_UI::toggle_keep_tearoffs));
	ActionManager::session_sensitive_actions.push_back (act);

	ActionManager::register_toggle_action (common_actions, X_("toggle-mixer"), S_("Window|Mixer"),  sigc::mem_fun(*this, &ARDOUR_UI::toggle_mixer_window));
	ActionManager::register_action (common_actions, X_("toggle-mixer-on-top"), _("Mixer on Top"),  sigc::mem_fun(*this, &ARDOUR_UI::toggle_mixer_on_top));
	ActionManager::register_toggle_action (common_actions, X_("ToggleRCOptionsEditor"), _("Preferences"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_rc_options_window));
	ActionManager::register_toggle_action (common_actions, X_("ToggleSessionOptionsEditor"), _("Properties"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_session_options_window));
	act = ActionManager::register_toggle_action (common_actions, X_("ToggleInspector"), _("Tracks and Busses"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_route_params_window));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (common_actions, X_("ToggleLocations"), _("Locations"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_location_window));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (common_actions, X_("ToggleBigClock"), _("Big Clock"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_big_clock_window));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (common_actions, X_("toggle-speaker-config"), _("Speaker Configuration"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_speaker_config_window));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (common_actions, X_("toggle-audio-connection-manager"), _("Audio Connection Manager"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::toggle_global_port_matrix), ARDOUR::DataType::AUDIO));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (common_actions, X_("toggle-midi-connection-manager"), _("MIDI Connection Manager"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::toggle_global_port_matrix), ARDOUR::DataType::MIDI));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("NewMIDITracer"), _("MIDI Tracer"), sigc::mem_fun(*this, &ARDOUR_UI::new_midi_tracer_window));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::register_action (common_actions, X_("About"), _("About"),  sigc::mem_fun(*this, &ARDOUR_UI::show_about));
	ActionManager::register_action (common_actions, X_("Chat"), _("Chat"),  sigc::mem_fun(*this, &ARDOUR_UI::launch_chat));
	/** TRANSLATORS: This is `Manual' in the sense of an instruction book that tells a user how to use Ardour */
	ActionManager::register_action (common_actions, X_("Manual"), S_("Help|Manual"),  mem_fun(*this, &ARDOUR_UI::launch_manual));
	ActionManager::register_action (common_actions, X_("Reference"), _("Reference"),  mem_fun(*this, &ARDOUR_UI::launch_reference));
	ActionManager::register_toggle_action (common_actions, X_("ToggleThemeManager"), _("Theme Manager"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_theme_manager));
	ActionManager::register_toggle_action (common_actions, X_("ToggleKeyEditor"), _("Key Bindings"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_key_editor));
	ActionManager::register_toggle_action (common_actions, X_("ToggleBundleManager"), _("Bundle Manager"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_bundle_manager));

#if 0
	act = ActionManager::register_action (common_actions, X_("AddAudioTrack"), _("Add Audio Track"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::session_add_audio_track), 1, 1, ARDOUR::Normal, (ARDOUR::RouteGroup *) 0, 1));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("AddAudioBus"), _("Add Audio Bus"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::session_add_audio_bus), 1, 1, (ARDOUR::RouteGroup *) 0, 1));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (common_actions, X_("AddMIDITrack"), _("Add MIDI Track"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::session_add_midi_track), (ARDOUR::RouteGroup *) 0, 1));
	ActionManager::session_sensitive_actions.push_back (act);
	//act = ActionManager::register_action (common_actions, X_("AddMidiBus"), _("Add Midi Bus"), sigc::mem_fun(*this, &ARDOUR_UI::session_add_midi_bus));
	//ActionManager::session_sensitive_actions.push_back (act);
#endif
	act = ActionManager::register_action (common_actions, X_("Save"), _("Save"),  sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::save_state), string(""), false));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);

	Glib::RefPtr<ActionGroup> transport_actions = ActionGroup::create (X_("Transport"));

	/* do-nothing action for the "transport" menu bar item */

	ActionManager::register_action (transport_actions, X_("Transport"), _("Transport"));

	/* these two are not used by key bindings, instead use ToggleRoll for that. these two do show up in
	   menus and via button proxies.
	*/

	act = ActionManager::register_action (transport_actions, X_("Stop"), _("Stop"), sigc::mem_fun(*this, &ARDOUR_UI::transport_stop));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("Roll"), _("Roll"), sigc::mem_fun(*this, &ARDOUR_UI::transport_roll));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	act = ActionManager::register_toggle_action (transport_actions, X_("AlwaysPlayRange"), _("Always Play Range"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_always_play_range));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	act = ActionManager::register_action (transport_actions, X_("ToggleRoll"), _("Start/Stop"), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::toggle_roll), false, false));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("ToggleRollMaybe"), _("Start/Continue/Stop"), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::toggle_roll), false, true));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("ToggleRollForgetCapture"), _("Stop and Forget Capture"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::toggle_roll), true, false));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	/* these two behave as follows:

	   - if transport speed != 1.0 or != -1.0, change speed to 1.0 or -1.0 (respectively)
	   - otherwise do nothing
	*/

	act = ActionManager::register_action (transport_actions, X_("TransitionToRoll"), _("Transition To Roll"), sigc::bind (sigc::mem_fun (*editor, &PublicEditor::transition_to_rolling), true));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	act = ActionManager::register_action (transport_actions, X_("TransitionToReverse"), _("Transition To Reverse"), sigc::bind (sigc::mem_fun (*editor, &PublicEditor::transition_to_rolling), false));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	act = ActionManager::register_action (transport_actions, X_("Loop"), _("Play Loop Range"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_session_auto_loop));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("PlaySelection"), _("Play Selected Range"), sigc::mem_fun(*this, &ARDOUR_UI::transport_play_selection));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	act = ActionManager::register_action (transport_actions, X_("Record"), _("Enable Record"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::transport_record), false));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("record-roll"), _("Start Recording"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::transport_record), true));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("Rewind"), _("Rewind"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::transport_rewind), 0));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("RewindSlow"), _("Rewind (Slow)"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::transport_rewind), -1));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("RewindFast"), _("Rewind (Fast)"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::transport_rewind), 1));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("Forward"), _("Forward"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::transport_forward), 0));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("ForwardSlow"), _("Forward (Slow)"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::transport_forward), -1));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("ForwardFast"), _("Forward (Fast)"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::transport_forward), 1));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("GotoZero"), _("Goto Zero"), sigc::mem_fun(*this, &ARDOUR_UI::transport_goto_zero));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("GotoStart"), _("Goto Start"), sigc::mem_fun(*this, &ARDOUR_UI::transport_goto_start));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("GotoEnd"), _("Goto End"), sigc::mem_fun(*this, &ARDOUR_UI::transport_goto_end));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("GotoWallClock"), _("Goto Wall Clock"), sigc::mem_fun(*this, &ARDOUR_UI::transport_goto_wallclock));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	act = ActionManager::register_action (transport_actions, X_("focus-on-clock"), _("Focus On Clock"), sigc::mem_fun(*this, &ARDOUR_UI::focus_on_clock));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	act = ActionManager::register_action (transport_actions, X_("primary-clock-timecode"), _("Timecode"), sigc::bind (sigc::mem_fun(primary_clock, &AudioClock::set_mode), AudioClock::Timecode));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("primary-clock-bbt"), _("Bars & Beats"), sigc::bind (sigc::mem_fun(primary_clock, &AudioClock::set_mode), AudioClock::BBT));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("primary-clock-minsec"), _("Minutes & Seconds"), sigc::bind (sigc::mem_fun(primary_clock, &AudioClock::set_mode), AudioClock::MinSec));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("primary-clock-samples"), _("Samples"), sigc::bind (sigc::mem_fun(primary_clock, &AudioClock::set_mode), AudioClock::Frames));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (transport_actions, X_("secondary-clock-timecode"), _("Timecode"), sigc::bind (sigc::mem_fun(secondary_clock, &AudioClock::set_mode), AudioClock::Timecode));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("secondary-clock-bbt"), _("Bars & Beats"), sigc::bind (sigc::mem_fun(secondary_clock, &AudioClock::set_mode), AudioClock::BBT));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("secondary-clock-minsec"), _("Minutes & Seconds"), sigc::bind (sigc::mem_fun(secondary_clock, &AudioClock::set_mode), AudioClock::MinSec));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("secondary-clock-samples"), _("Samples"), sigc::bind (sigc::mem_fun(secondary_clock, &AudioClock::set_mode), AudioClock::Frames));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_toggle_action (transport_actions, X_("TogglePunchIn"), _("Punch In"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_punch_in));
	act->set_short_label (_("In"));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (transport_actions, X_("TogglePunchOut"), _("Punch Out"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_punch_out));
	act->set_short_label (_("Out"));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (transport_actions, X_("TogglePunch"), _("Punch In/Out"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_punch));
	act->set_short_label (_("In/Out"));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (transport_actions, X_("ToggleClick"), _("Click"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_click));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (transport_actions, X_("ToggleAutoInput"), _("Auto Input"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_auto_input));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (transport_actions, X_("ToggleAutoPlay"), _("Auto Play"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_auto_play));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (transport_actions, X_("ToggleAutoReturn"), _("Auto Return"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_auto_return));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	act = ActionManager::register_toggle_action (transport_actions, X_("ToggleVideoSync"), _("Sync Startup to Video"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_video_sync));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (transport_actions, X_("ToggleTimeMaster"), _("Time Master"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_time_master));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (transport_actions, X_("ToggleExternalSync"), "", sigc::mem_fun(*this, &ARDOUR_UI::toggle_external_sync));
	ActionManager::session_sensitive_actions.push_back (act);

	for (int i = 1; i <= 32; ++i) {
		string const a = string_compose (X_("ToggleRecordEnableTrack%1"), i);
		string const n = string_compose (_("Toggle Record Enable Track %1"), i);
		act = ActionManager::register_action (common_actions, a.c_str(), n.c_str(), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::toggle_record_enable), i - 1));
		ActionManager::session_sensitive_actions.push_back (act);
	}

	Glib::RefPtr<ActionGroup> shuttle_actions = ActionGroup::create ("ShuttleActions");

	shuttle_actions->add (Action::create (X_("SetShuttleUnitsPercentage"), _("Percentage")), hide_return (sigc::bind (sigc::mem_fun (*Config, &RCConfiguration::set_shuttle_units), Percentage)));
	shuttle_actions->add (Action::create (X_("SetShuttleUnitsSemitones"), _("Semitones")), hide_return (sigc::bind (sigc::mem_fun (*Config, &RCConfiguration::set_shuttle_units), Semitones)));

	Glib::RefPtr<ActionGroup> option_actions = ActionGroup::create ("options");

	act = ActionManager::register_toggle_action (option_actions, X_("SendMTC"), _("Send MTC"), sigc::mem_fun (*this, &ARDOUR_UI::toggle_send_mtc));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (option_actions, X_("SendMMC"), _("Send MMC"), sigc::mem_fun (*this, &ARDOUR_UI::toggle_send_mmc));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (option_actions, X_("UseMMC"), _("Use MMC"), sigc::mem_fun (*this, &ARDOUR_UI::toggle_use_mmc));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (option_actions, X_("SendMidiClock"), _("Send MIDI Clock"), sigc::mem_fun (*this, &ARDOUR_UI::toggle_send_midi_clock));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (option_actions, X_("SendMIDIfeedback"), _("Send MIDI Feedback"), sigc::mem_fun (*this, &ARDOUR_UI::toggle_send_midi_feedback));
	ActionManager::session_sensitive_actions.push_back (act);


	if (getenv ("ARDOUR_BUNDLED")) {
		act = ActionManager::register_toggle_action (main_actions, X_("EnableTranslation"), _("Enable Translations"), mem_fun (*this, &ARDOUR_UI::toggle_translations));
		if (act) {
			RefPtr<ToggleAction> ract = RefPtr<ToggleAction>::cast_dynamic (act);
			if (ract) {
				ract->set_active (!ARDOUR::translations_are_disabled());
			}
		}
	}

	/* MIDI */

	Glib::RefPtr<ActionGroup> midi_actions = ActionGroup::create (X_("MIDI"));
	ActionManager::register_action (midi_actions, X_("panic"), _("Panic"), sigc::mem_fun(*this, &ARDOUR_UI::midi_panic));

	ActionManager::add_action_group (shuttle_actions);
	ActionManager::add_action_group (option_actions);
	ActionManager::add_action_group (jack_actions);
	ActionManager::add_action_group (transport_actions);
	ActionManager::add_action_group (main_actions);
	ActionManager::add_action_group (main_menu_actions);
	ActionManager::add_action_group (common_actions);
	ActionManager::add_action_group (midi_actions);
}

void
ARDOUR_UI::set_jack_buffer_size (pframes_t nframes)
{
	Glib::RefPtr<Action> action;
	const char* action_name = 0;

	switch (nframes) {
	case 32:
		action_name = X_("JACKLatency32");
		break;
	case 64:
		action_name = X_("JACKLatency64");
		break;
	case 128:
		action_name = X_("JACKLatency128");
		break;
	case 256:
		action_name = X_("JACKLatency256");
		break;
	case 512:
		action_name = X_("JACKLatency512");
		break;
	case 1024:
		action_name = X_("JACKLatency1024");
		break;
	case 2048:
		action_name = X_("JACKLatency2048");
		break;
	case 4096:
		action_name = X_("JACKLatency4096");
		break;
	case 8192:
		action_name = X_("JACKLatency8192");
		break;
	default:
		/* XXX can we do anything useful ? */
		break;
	}

	if (action_name) {

		action = ActionManager::get_action (X_("JACK"), action_name);

		if (action) {
			Glib::RefPtr<RadioAction> ract = Glib::RefPtr<RadioAction>::cast_dynamic (action);

			if (ract && ract->get_active()) {
				engine->request_buffer_size (nframes);
				update_sample_rate (0);
			}
		}
	}
}

void
ARDOUR_UI::build_menu_bar ()
{
	menu_bar = dynamic_cast<MenuBar*> (ActionManager::get_widget (X_("/Main")));
	menu_bar->set_name ("MainMenuBar");

	/*
	 * This is needed because this property does not get installed
	 * until the Menu GObject class is registered, which happens
	 * when the first menu instance is created.
	 */
	// XXX bug in gtkmm causes this to popup an error message
	// Gtk::Settings::get_default()->property_gtk_can_change_accels() = true;
	// so use this instead ...
	gtk_settings_set_long_property (gtk_settings_get_default(), "gtk-can-change-accels", 1, "Ardour:designers");

	EventBox* ev = manage (new EventBox);
	ev->show ();
	CairoHPacker* hbox = manage (new CairoHPacker);
	hbox->show ();
	hbox->set_border_width (3);

	VBox* vbox = manage (new VBox);
	vbox->pack_start (*hbox, true, false);
	vbox->show();

	ev->add (*vbox);

	wall_clock_label.set_name ("WallClock");
	wall_clock_label.set_use_markup ();
	disk_space_label.set_name ("WallClock");
	disk_space_label.set_use_markup ();
	cpu_load_label.set_name ("CPULoad");
	cpu_load_label.set_use_markup ();
	buffer_load_label.set_name ("BufferLoad");
	buffer_load_label.set_use_markup ();
	sample_rate_label.set_name ("SampleRate");
	sample_rate_label.set_use_markup ();
	format_label.set_name ("Format");
	format_label.set_use_markup ();

#ifndef TOP_MENUBAR
 	menu_hbox.pack_start (*menu_bar, false, false);
#else
	use_menubar_as_top_menubar ();
#endif

	bool wall_clock = false;
	bool disk_space = false;

 	if (!Profile->get_small_screen()) {
#ifndef GTKOSX
		// OSX provides its own wallclock, thank you very much
		wall_clock = true;
#endif
		disk_space = true;
	}
	
	hbox->pack_end (wall_clock_label, false, false, 2);
	hbox->pack_end (disk_space_label, false, false, 4);
	hbox->pack_end (cpu_load_label, false, false, 4);
	hbox->pack_end (buffer_load_label, false, false, 4);
	hbox->pack_end (sample_rate_label, false, false, 4);
	hbox->pack_end (format_label, false, false, 4);

	menu_hbox.pack_end (*ev, false, false, 6);

	menu_bar_base.set_name ("MainMenuBar");
	menu_bar_base.add (menu_hbox);

	_status_bar_visibility.add (&wall_clock_label,  X_("WallClock"), _("Wall Clock"), wall_clock);
	_status_bar_visibility.add (&disk_space_label,  X_("Disk"),      _("Disk Space"), disk_space);
	_status_bar_visibility.add (&cpu_load_label,    X_("DSP"),       _("DSP"), true);
	_status_bar_visibility.add (&buffer_load_label, X_("Buffers"),   _("Buffers"), true);
	_status_bar_visibility.add (&sample_rate_label, X_("JACK"),      _("JACK Sampling Rate and Latency"), true);
	_status_bar_visibility.add (&format_label,      X_("Format"),    _("File Format"), true);

	ev->signal_button_press_event().connect (sigc::mem_fun (_status_bar_visibility, &VisibilityGroup::button_press_event));
}

void
ARDOUR_UI::use_menubar_as_top_menubar ()
{
	Gtk::Widget* widget;
	Application* app = Application::instance ();

	/* Quit will be taken of separately */

	if ((widget = ActionManager::get_widget ("/ui/Main/Session/Quit"))) {
		widget->hide ();
	}

	GtkApplicationMenuGroup* group = app->add_app_menu_group ();

	if ((widget = ActionManager::get_widget ("/ui/Main/Session/About"))) {
		app->add_app_menu_item (group, dynamic_cast<MenuItem*>(widget));
	}

	if ((widget = ActionManager::get_widget ("/ui/Main/Session/ToggleRCOptionsEditor"))) {
		app->add_app_menu_item (group, dynamic_cast<MenuItem*>(widget));
	}

	app->set_menu_bar (*menu_bar);
}

void
ARDOUR_UI::big_clock_catch_focus ()
{
	PublicEditor::instance().reset_focus ();
}

void
ARDOUR_UI::setup_clock ()
{
	ARDOUR_UI::Clock.connect (sigc::mem_fun (big_clock, &AudioClock::set));

	big_clock->set_corner_radius (0.0);
	big_clock->set_fixed_width (false);
	big_clock->mode_changed.connect (sigc::mem_fun (*this, &ARDOUR_UI::big_clock_reset_aspect_ratio));

	big_clock_window->set (new Window (WINDOW_TOPLEVEL), false);

	big_clock_window->get()->set_keep_above (true);
	big_clock_window->get()->set_border_width (0);
	big_clock_window->get()->add (*big_clock);

	big_clock_window->get()->set_title (_("Big Clock"));
	big_clock_window->get()->signal_realize().connect (sigc::mem_fun (*this, &ARDOUR_UI::big_clock_realized));
	big_clock_window->get()->signal_key_press_event().connect (sigc::bind (sigc::ptr_fun (relay_key_press), big_clock_window->get()), false);
	big_clock_window->get()->signal_size_allocate().connect (sigc::mem_fun (*this, &ARDOUR_UI::big_clock_size_allocate));

	big_clock_window->get()->signal_unmap().connect (sigc::bind (sigc::ptr_fun(&ActionManager::uncheck_toggleaction), X_("<Actions>/Common/ToggleBigClock")));
	big_clock_window->get()->signal_unmap().connect (sigc::mem_fun (*this, &ARDOUR_UI::big_clock_catch_focus));

	manage_window (*big_clock_window->get());
}

void
ARDOUR_UI::big_clock_reset_aspect_ratio ()
{
	Gtk::Requisition req;
	big_clock->size_request (req);
	float aspect = req.width/(float)req.height;
	Gdk::Geometry geom;

	geom.min_aspect = aspect;
	geom.max_aspect = aspect;

	big_clock_window->get()->set_geometry_hints (*big_clock, geom, Gdk::HINT_ASPECT);
}

void
ARDOUR_UI::big_clock_realized ()
{
	int x, y, w, d;

	set_decoration (big_clock_window->get(), (Gdk::DECOR_BORDER|Gdk::DECOR_RESIZEH));
	big_clock_window->get()->get_window()->get_geometry (x, y, w, big_clock_height, d);

	big_clock_reset_aspect_ratio ();

	original_big_clock_height = big_clock_height;
	original_big_clock_width = w;

	Pango::FontDescription fd (big_clock->get_style()->get_font());
	original_big_clock_font_size = fd.get_size ();

	if (!fd.get_size_is_absolute ()) {
		original_big_clock_font_size /= PANGO_SCALE;
	}

	big_clock_window->setup ();
}

void
ARDOUR_UI::float_big_clock (Gtk::Window* parent)
{
	if (big_clock_window->get()) {
		if (parent) {
			big_clock_window->get()->set_transient_for (*parent);
		} else {
			gtk_window_set_transient_for (big_clock_window->get()->gobj(), (GtkWindow*) 0);
		}
	}
}

void
ARDOUR_UI::big_clock_size_allocate (Gtk::Allocation&)
{
	if (!big_clock_resize_in_progress) {
		Glib::signal_idle().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::idle_big_clock_text_resizer), 0, 0));
		big_clock_resize_in_progress = true;
	}
}

bool
ARDOUR_UI::idle_big_clock_text_resizer (int, int)
{
	big_clock_resize_in_progress = false;

	Glib::RefPtr<Gdk::Window> win = big_clock_window->get()->get_window();
	Pango::FontDescription fd (big_clock->get_style()->get_font());
	int current_size = fd.get_size ();
	int x, y, w, h, d;

	if (!fd.get_size_is_absolute ()) {
		current_size /= PANGO_SCALE;
	}

	win->get_geometry (x, y, w, h, d);

	double scale  = min (((double) w / (double) original_big_clock_width),
	                     ((double) h / (double) original_big_clock_height));

	int size = (int) lrintf (original_big_clock_font_size * scale);

	if (size != current_size) {

		string family = fd.get_family();
		char buf[family.length()+16];
		snprintf (buf, family.length()+16, "%s %d", family.c_str(), size);

		try {
			Pango::FontDescription fd (buf);
			Glib::RefPtr<Gtk::RcStyle> rcstyle = big_clock->get_modifier_style ();
			rcstyle->set_font (fd);
			big_clock->modify_style (rcstyle);
		}

		catch (...) {
			/* oh well, do nothing */
		}
	}

	return false;
}

void
ARDOUR_UI::save_ardour_state ()
{
	if (!keyboard || !mixer || !editor) {
		return;
	}

	/* XXX this is all a bit dubious. add_extra_xml() uses
	   a different lifetime model from add_instant_xml().
	*/

	XMLNode* node = new XMLNode (keyboard->get_state());
	Config->add_extra_xml (*node);
	Config->add_extra_xml (get_transport_controllable_state());

	XMLNode* window_node = new XMLNode (X_("UI"));
	window_node->add_property (_status_bar_visibility.get_state_name().c_str(), _status_bar_visibility.get_state_value ());

	for (list<WindowProxyBase*>::iterator i = _window_proxies.begin(); i != _window_proxies.end(); ++i) {
		if ((*i)->rc_configured()) {
			window_node->add_child_nocopy (*((*i)->get_state ()));
		}
	}

	/* tearoffs */

	XMLNode* tearoff_node = new XMLNode (X_("Tearoffs"));

	if (transport_tearoff) {
		XMLNode* t = new XMLNode (X_("transport"));
		transport_tearoff->add_state (*t);
		tearoff_node->add_child_nocopy (*t);
	}

	if (mixer && mixer->monitor_section()) {
		XMLNode* t = new XMLNode (X_("monitor-section"));
		mixer->monitor_section()->tearoff().add_state (*t);
		tearoff_node->add_child_nocopy (*t);
	}

	if (editor && editor->mouse_mode_tearoff()) {
		XMLNode* t = new XMLNode (X_("mouse-mode"));
		editor->mouse_mode_tearoff ()->add_state (*t);
		tearoff_node->add_child_nocopy (*t);
	}

	window_node->add_child_nocopy (*tearoff_node);

	Config->add_extra_xml (*window_node);

	if (_startup && _startup->engine_control() && _startup->engine_control()->was_used()) {
		Config->add_extra_xml (_startup->engine_control()->get_state());
	}
	Config->save_state();
	if (ui_config->dirty()) {
		ui_config->save_state ();
	}

	XMLNode& enode (static_cast<Stateful*>(editor)->get_state());
	XMLNode& mnode (mixer->get_state());

	if (_session) {
		_session->add_instant_xml (enode);
		_session->add_instant_xml (mnode);
		if (location_ui->get ()) {
			_session->add_instant_xml (location_ui->get()->ui().get_state ());
		}
	} else {
		Config->add_instant_xml (enode);
		Config->add_instant_xml (mnode);
		if (location_ui->get ()) {
			Config->add_instant_xml (location_ui->get()->ui().get_state ());
		}
	}

	Keyboard::save_keybindings ();
}

void
ARDOUR_UI::toggle_global_port_matrix (ARDOUR::DataType t)
{
	std::string const action = string_compose ("toggle-%1-connection-manager", t.to_string ());

	if (_global_port_matrix[t]->get() == 0) {
		_global_port_matrix[t]->set (new GlobalPortMatrixWindow (_session, t));
		_global_port_matrix[t]->get()->signal_unmap().connect(sigc::bind (sigc::ptr_fun (&ActionManager::uncheck_toggleaction), string_compose (X_("<Actions>/Common/%1"), action)));
	}

	RefPtr<Action> act = ActionManager::get_action (X_("Common"), action.c_str());
	if (act) {
		RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic (act);

		if (tact->get_active()) {
			_global_port_matrix[t]->get()->present ();
		} else {
			_global_port_matrix[t]->get()->hide ();
		}
	}
}

void
ARDOUR_UI::resize_text_widgets ()
{
	set_size_request_to_display_given_text (cpu_load_label, "DSP: 100.0%", 2, 2);
	set_size_request_to_display_given_text (buffer_load_label, "Buffers: p:100% c:100%", 2, 2);
}

void
ARDOUR_UI::focus_on_clock ()
{
	if (editor && primary_clock) {
		editor->present ();
		primary_clock->focus ();
	}
}
