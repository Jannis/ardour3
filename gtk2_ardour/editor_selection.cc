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

*/

#include <algorithm>
#include <cstdlib>

#include "pbd/stacktrace.h"

#include "ardour/session.h"
#include "ardour/playlist.h"
#include "ardour/route_group.h"
#include "ardour/profile.h"

#include "editor.h"
#include "actions.h"
#include "audio_time_axis.h"
#include "audio_region_view.h"
#include "audio_streamview.h"
#include "automation_line.h"
#include "control_point.h"
#include "editor_regions.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace Editing;

struct TrackViewByPositionSorter
{
    bool operator() (const TimeAxisView* a, const TimeAxisView *b) {
	    return a->y_position() < b->y_position();
    }
};

bool
Editor::extend_selection_to_track (TimeAxisView& view)
{
	if (selection->selected (&view)) {
		/* already selected, do nothing */
		return false;
	}

	if (selection->tracks.empty()) {

		if (!selection->selected (&view)) {
			selection->set (&view);
			return true;
		} else {
			return false;
		}
	}

	/* something is already selected, so figure out which range of things to add */

	TrackViewList to_be_added;
	TrackViewList sorted = track_views;
	TrackViewByPositionSorter cmp;
	bool passed_clicked = false;
	bool forwards = true;

	sorted.sort (cmp);

	if (!selection->selected (&view)) {
		to_be_added.push_back (&view);
	}

	/* figure out if we should go forward or backwards */

	for (TrackViewList::iterator i = sorted.begin(); i != sorted.end(); ++i) {

		if ((*i) == &view) {
			passed_clicked = true;
		}

		if (selection->selected (*i)) {
			if (passed_clicked) {
				forwards = true;
			} else {
				forwards = false;
			}
			break;
		}
	}

	passed_clicked = false;

	if (forwards) {

		for (TrackViewList::iterator i = sorted.begin(); i != sorted.end(); ++i) {

			if ((*i) == &view) {
				passed_clicked = true;
				continue;
			}

			if (passed_clicked) {
				if ((*i)->hidden()) {
					continue;
				}
				if (selection->selected (*i)) {
					break;
				} else if (!(*i)->hidden()) {
					to_be_added.push_back (*i);
				}
			}
		}

	} else {

		for (TrackViewList::reverse_iterator r = sorted.rbegin(); r != sorted.rend(); ++r) {

			if ((*r) == &view) {
				passed_clicked = true;
				continue;
			}

			if (passed_clicked) {

				if ((*r)->hidden()) {
					continue;
				}

				if (selection->selected (*r)) {
					break;
				} else if (!(*r)->hidden()) {
					to_be_added.push_back (*r);
				}
			}
		}
	}

	if (!to_be_added.empty()) {
		selection->add (to_be_added);
		return true;
	}

	return false;
}

void
Editor::select_all_tracks ()
{
	TrackViewList visible_views;
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		if ((*i)->marked_for_display()) {
			visible_views.push_back (*i);
		}
	}
	selection->set (visible_views);
}

/** Select clicked_axisview, unless there are no currently selected
 *  tracks, in which case nothing will happen unless `force' is true.
 */
void
Editor::set_selected_track_as_side_effect (Selection::Operation op, bool force)
{
        cerr << "E::sstase ca = " << clicked_axisview << " cr = " << clicked_routeview 
             << " op = " << op << " force = " << force
             << endl;

	if (!clicked_axisview) {
		return;
	}

#if 1
        if (!clicked_routeview) {
                return;
        }

        RouteGroup* group = clicked_routeview->route()->route_group();
        
	switch (op) {
	case Selection::Toggle: 
		if (selection->selected (clicked_axisview)) {
			if (_session->all_route_group().is_active()) {
				for (TrackViewList::iterator i = track_views.begin(); i != track_views.end (); ++i) {
                                        selection->remove(*i);
				}
			} else if (group && group->is_active()) {
				for (TrackViewList::iterator i = track_views.begin(); i != track_views.end (); ++i) {
					if ((*i)->route_group() == group)
						selection->remove(*i);
				}
			} else {
				selection->remove (clicked_axisview);
                        }
		} else {
			if (_session->all_route_group().is_active()) {
				for (TrackViewList::iterator i = track_views.begin(); i != track_views.end (); ++i) {
						selection->add(*i);
				}
			} else if (group && group->is_active()) {
				for (TrackViewList::iterator i = track_views.begin(); i != track_views.end (); ++i) {
					if ( (*i)->route_group() == group)
						selection->add(*i);
				}
                        } else {
                                selection->add (clicked_axisview);
                        }
		}
                break;
	
	case Selection::Add: 
		selection->clear();
		cerr << ("Editor::set_selected_track_as_side_effect  case  Selection::Add  not yet implemented\n");
                break;
		
	case Selection::Set:
		selection->clear();
		if (_session->all_route_group().is_active()) {
			for (TrackViewList::iterator i = track_views.begin(); i != track_views.end (); ++i) {
					selection->add(*i);
			}
		} else if (group && group->is_active()) {
			for (TrackViewList::iterator i  = track_views.begin(); i != track_views.end (); ++i) {
				if ((*i)->route_group() == group)
					selection->add(*i);
			}
                } else {
			selection->set (clicked_axisview);
                }
                break;
	
	case Selection::Extend: 
		selection->clear();
		cerr << ("Editor::set_selected_track_as_side_effect  case  Selection::Add  not yet implemented\n");
                break;
	}

#else // the older version

	if (!selection->tracks.empty()) {
		if (!selection->selected (clicked_axisview)) {
			selection->add (clicked_axisview);
		}

	} else if (force) {
		selection->set (clicked_axisview);
	}
#endif
}

void
Editor::set_selected_track (TimeAxisView& view, Selection::Operation op, bool no_remove)
{
	switch (op) {
	case Selection::Toggle:
		if (selection->selected (&view)) {
			if (!no_remove) {
				selection->remove (&view);
			}
		} else {
			selection->add (&view);
		}
		break;

	case Selection::Add:
		if (!selection->selected (&view)) {
			selection->add (&view);
		}
		break;

	case Selection::Set:
		selection->set (&view);
		break;

	case Selection::Extend:
		extend_selection_to_track (view);
		break;
	}
}

void
Editor::set_selected_track_from_click (bool press, Selection::Operation op, bool no_remove)
{
	if (!clicked_routeview) {
		return;
	}

	if (!press) {
		return;
	}

	set_selected_track (*clicked_routeview, op, no_remove);
}

bool
Editor::set_selected_control_point_from_click (Selection::Operation op, bool /*no_remove*/)
{
	if (!clicked_control_point) {
		return false;
	}
	
	switch (op) {
	case Selection::Set:
		selection->set (clicked_control_point);
		break;
	case Selection::Add:
		selection->add (clicked_control_point);
		break;
	case Selection::Toggle:
		selection->toggle (clicked_control_point);
		break;
	case Selection::Extend:
		/* XXX */
		break;
	}

	return true;
}

void
Editor::get_onscreen_tracks (TrackViewList& tvl)
{
 	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
 		if ((*i)->y_position() < _canvas_height) {
 			tvl.push_back (*i);
 		}
 	}
}

/** Call a slot for a given `basis' track and also for any track that is in the same
 *  active route group with a particular set of properties.
 *
 *  @param sl Slot to call.
 *  @param basis Basis track.
 *  @param prop Properties that active edit groups must share to be included in the map.
 */

void
Editor::mapover_tracks (sigc::slot<void, RouteTimeAxisView&, uint32_t> sl, TimeAxisView* basis, PBD::PropertyID prop) const
{
	RouteTimeAxisView* route_basis = dynamic_cast<RouteTimeAxisView*> (basis);

	if (route_basis == 0) {
		return;
	}

	set<RouteTimeAxisView*> tracks;
	tracks.insert (route_basis);

	RouteGroup* group = route_basis->route()->route_group();

	if (group && group->enabled_property(prop) && group->enabled_property (Properties::active.property_id) ) {

		/* the basis is a member of an active route group, with the appropriate
		   properties; find other members */

		for (TrackViewList::const_iterator i = track_views.begin(); i != track_views.end(); ++i) {
			RouteTimeAxisView* v = dynamic_cast<RouteTimeAxisView*> (*i);
			if (v && v->route()->route_group() == group) {
				tracks.insert (v);
			}
		}
	}

	/* call the slots */
	uint32_t const sz = tracks.size ();
	
	for (set<RouteTimeAxisView*>::iterator i = tracks.begin(); i != tracks.end(); ++i) {
		sl (**i, sz);
	}
}

void
Editor::mapped_get_equivalent_regions (RouteTimeAxisView& tv, uint32_t, RegionView * basis, vector<RegionView*>* all_equivs) const
{
	boost::shared_ptr<Playlist> pl;
	vector<boost::shared_ptr<Region> > results;
	RegionView* marv;
	boost::shared_ptr<Track> tr;

	if ((tr = tv.track()) == 0) {
		/* bus */
		return;
	}

	if (&tv == &basis->get_time_axis_view()) {
		/* looking in same track as the original */
		return;
	}

	if ((pl = tr->playlist()) != 0) {
		pl->get_equivalent_regions (basis->region(), results);
	}

	for (vector<boost::shared_ptr<Region> >::iterator ir = results.begin(); ir != results.end(); ++ir) {
		if ((marv = tv.view()->find_view (*ir)) != 0) {
			all_equivs->push_back (marv);
		}
	}
}

void
Editor::get_equivalent_regions (RegionView* basis, vector<RegionView*>& equivalent_regions, PBD::PropertyID property) const
{
	mapover_tracks (sigc::bind (sigc::mem_fun (*this, &Editor::mapped_get_equivalent_regions), basis, &equivalent_regions), &basis->get_trackview(), property);

	/* add clicked regionview since we skipped all other regions in the same track as the one it was in */

	equivalent_regions.push_back (basis);
}

RegionSelection
Editor::get_equivalent_regions (RegionSelection & basis, PBD::PropertyID prop) const
{
	RegionSelection equivalent;

	for (RegionSelection::const_iterator i = basis.begin(); i != basis.end(); ++i) {

		vector<RegionView*> eq;

		mapover_tracks (
			sigc::bind (sigc::mem_fun (*this, &Editor::mapped_get_equivalent_regions), *i, &eq),
			&(*i)->get_trackview(), prop
			);

		for (vector<RegionView*>::iterator j = eq.begin(); j != eq.end(); ++j) {
			equivalent.add (*j);
		}

		equivalent.add (*i);
	}

	return equivalent;
}


int
Editor::get_regionview_count_from_region_list (boost::shared_ptr<Region> region)
{
	int region_count = 0;

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {

		RouteTimeAxisView* tatv;

		if ((tatv = dynamic_cast<RouteTimeAxisView*> (*i)) != 0) {

			boost::shared_ptr<Playlist> pl;
			vector<boost::shared_ptr<Region> > results;
			RegionView* marv;
			boost::shared_ptr<Track> tr;

			if ((tr = tatv->track()) == 0) {
				/* bus */
				continue;
			}

			if ((pl = (tr->playlist())) != 0) {
				pl->get_region_list_equivalent_regions (region, results);
			}

			for (vector<boost::shared_ptr<Region> >::iterator ir = results.begin(); ir != results.end(); ++ir) {
				if ((marv = tatv->view()->find_view (*ir)) != 0) {
					region_count++;
				}
			}

		}
	}

	return region_count;
}


bool
Editor::set_selected_regionview_from_click (bool press, Selection::Operation op, bool /*no_track_remove*/)
{
	vector<RegionView*> all_equivalent_regions;
	bool commit = false;

	if (!clicked_regionview || !clicked_routeview) {
		return false;
	}

	if (press) {
		button_release_can_deselect = false;
	}

	if (op == Selection::Toggle || op == Selection::Set) {


		switch (op) {
		case Selection::Toggle:

			if (selection->selected (clicked_regionview)) {
				if (press) {

					/* whatever was clicked was selected already; do nothing here but allow
					   the button release to deselect it
					*/

					button_release_can_deselect = true;

				} else {

					if (button_release_can_deselect) {

						/* just remove this one region, but only on a permitted button release */

						selection->remove (clicked_regionview);
						commit = true;

						/* no more deselect action on button release till a new press
						   finds an already selected object.
						*/

						button_release_can_deselect = false;
					}
				}

			} else {

				if (press) {

					if (selection->selected (clicked_routeview)) {
						get_equivalent_regions (clicked_regionview, all_equivalent_regions, ARDOUR::Properties::select.property_id);
					} else {
						all_equivalent_regions.push_back (clicked_regionview);
					}

					/* add all the equivalent regions, but only on button press */

					if (!all_equivalent_regions.empty()) {
						commit = true;
					}

					selection->add (all_equivalent_regions);
				}
			}
			break;

		case Selection::Set:
			if (!selection->selected (clicked_regionview)) {
				get_equivalent_regions (clicked_regionview, all_equivalent_regions, ARDOUR::Properties::select.property_id);
				selection->set (all_equivalent_regions);
				commit = true;
			} else {
				/* no commit necessary: clicked on an already selected region */
				goto out;
			}
			break;

		default:
			/* silly compiler */
			break;
		}

	} else if (op == Selection::Extend) {

		list<Selectable*> results;
		nframes64_t last_frame;
		nframes64_t first_frame;
		bool same_track = false;

		/* 1. find the last selected regionview in the track that was clicked in */

		last_frame = 0;
		first_frame = max_frames;

		for (RegionSelection::iterator x = selection->regions.begin(); x != selection->regions.end(); ++x) {
			if (&(*x)->get_time_axis_view() == &clicked_regionview->get_time_axis_view()) {

				if ((*x)->region()->last_frame() > last_frame) {
					last_frame = (*x)->region()->last_frame();
				}

				if ((*x)->region()->first_frame() < first_frame) {
					first_frame = (*x)->region()->first_frame();
				}

				same_track = true;
			}
		}

		if (same_track) {

			/* 2. figure out the boundaries for our search for new objects */

			switch (clicked_regionview->region()->coverage (first_frame, last_frame)) {
			case OverlapNone:
				if (last_frame < clicked_regionview->region()->first_frame()) {
					first_frame = last_frame;
					last_frame = clicked_regionview->region()->last_frame();
				} else {
					last_frame = first_frame;
					first_frame = clicked_regionview->region()->first_frame();
				}
				break;

			case OverlapExternal:
				if (last_frame < clicked_regionview->region()->first_frame()) {
					first_frame = last_frame;
					last_frame = clicked_regionview->region()->last_frame();
				} else {
					last_frame = first_frame;
					first_frame = clicked_regionview->region()->first_frame();
				}
				break;

			case OverlapInternal:
				if (last_frame < clicked_regionview->region()->first_frame()) {
					first_frame = last_frame;
					last_frame = clicked_regionview->region()->last_frame();
				} else {
					last_frame = first_frame;
					first_frame = clicked_regionview->region()->first_frame();
				}
				break;

			case OverlapStart:
			case OverlapEnd:
				/* nothing to do except add clicked region to selection, since it
				   overlaps with the existing selection in this track.
				*/
				break;
			}

		} else {

			/* click in a track that has no regions selected, so extend vertically
			   to pick out all regions that are defined by the existing selection
			   plus this one.
			*/


			first_frame = entered_regionview->region()->position();
			last_frame = entered_regionview->region()->last_frame();

			for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
				if ((*i)->region()->position() < first_frame) {
					first_frame = (*i)->region()->position();
				}
				if ((*i)->region()->last_frame() + 1 > last_frame) {
					last_frame = (*i)->region()->last_frame();
				}
			}
		}

		/* 2. find all the tracks we should select in */

		set<RouteTimeAxisView*> relevant_tracks;

		for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
			RouteTimeAxisView* r = dynamic_cast<RouteTimeAxisView*> (*i);
			if (r) {
				relevant_tracks.insert (r);
			}
		}
		
		set<RouteTimeAxisView*> already_in_selection;

		if (relevant_tracks.empty()) {

			/* no tracks selected .. thus .. if the
			   regionview we're in isn't selected
			   (i.e. we're about to extend to it), then
			   find all tracks between the this one and
			   any selected ones.
			*/

			if (!selection->selected (entered_regionview)) {

				RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (&entered_regionview->get_time_axis_view());

				if (rtv) {

					/* add this track to the ones we will search */

					relevant_tracks.insert (rtv);

					/* find the track closest to this one that
					   already a selected region.
					*/

					RouteTimeAxisView* closest = 0;
					int distance = INT_MAX;
					int key = rtv->route()->order_key ("editor");

					for (RegionSelection::iterator x = selection->regions.begin(); x != selection->regions.end(); ++x) {

						RouteTimeAxisView* artv = dynamic_cast<RouteTimeAxisView*>(&(*x)->get_time_axis_view());

						if (artv && artv != rtv) {

							pair<set<RouteTimeAxisView*>::iterator,bool> result;

							result = already_in_selection.insert (artv);

							if (result.second) {
								/* newly added to already_in_selection */

								int d = artv->route()->order_key ("editor");

								d -= key;

								if (abs (d) < distance) {
									distance = abs (d);
									closest = artv;
								}
							}
						}
					}

					if (closest) {

						/* now add all tracks between that one and this one */

						int okey = closest->route()->order_key ("editor");

						if (okey > key) {
							swap (okey, key);
						}

						for (TrackViewList::iterator x = track_views.begin(); x != track_views.end(); ++x) {
							RouteTimeAxisView* artv = dynamic_cast<RouteTimeAxisView*>(*x);
							if (artv && artv != rtv) {

								int k = artv->route()->order_key ("editor");

								if (k >= okey && k <= key) {

									/* in range but don't add it if
									   it already has tracks selected.
									   this avoids odd selection
									   behaviour that feels wrong.
									*/

									if (find (already_in_selection.begin(),
										  already_in_selection.end(),
										  artv) == already_in_selection.end()) {

										relevant_tracks.insert (artv);
									}
								}
							}
						}
					}
				}
			}
		}

		/* 3. find all selectable objects (regionviews in this case) between that one and the end of the
			   one that was clicked.
		*/

		for (set<RouteTimeAxisView*>::iterator t = relevant_tracks.begin(); t != relevant_tracks.end(); ++t) {
			(*t)->get_selectables (first_frame, last_frame, -1.0, -1.0, results);
		}

		/* 4. convert to a vector of regions */

		vector<RegionView*> regions;

		for (list<Selectable*>::iterator x = results.begin(); x != results.end(); ++x) {
			RegionView* arv;

			if ((arv = dynamic_cast<RegionView*>(*x)) != 0) {
				regions.push_back (arv);
			}
		}

		if (!regions.empty()) {
			selection->add (regions);
			commit = true;
		}
	}

  out:
	return commit;
}


void
Editor::set_selected_regionview_from_region_list (boost::shared_ptr<Region> region, Selection::Operation op)
{
	vector<RegionView*> all_equivalent_regions;

	get_regions_corresponding_to (region, all_equivalent_regions);

	if (all_equivalent_regions.empty()) {
		return;
	}

	begin_reversible_command (_("set selected regions"));

	switch (op) {
	case Selection::Toggle:
		/* XXX this is not correct */
		selection->toggle (all_equivalent_regions);
		break;
	case Selection::Set:
		selection->set (all_equivalent_regions);
		break;
	case Selection::Extend:
		selection->add (all_equivalent_regions);
		break;
	case Selection::Add:
		selection->add (all_equivalent_regions);
		break;
	}

	commit_reversible_command () ;
}

bool
Editor::set_selected_regionview_from_map_event (GdkEventAny* /*ev*/, StreamView* sv, boost::weak_ptr<Region> weak_r)
{
	RegionView* rv;
	boost::shared_ptr<Region> r (weak_r.lock());

	if (!r) {
		return true;
	}

	if ((rv = sv->find_view (r)) == 0) {
		return true;
	}

	/* don't reset the selection if its something other than
	   a single other region.
	*/

	if (selection->regions.size() > 1) {
		return true;
	}

	begin_reversible_command (_("set selected regions"));

	selection->set (rv);

	commit_reversible_command () ;

	return true;
}

void
Editor::track_selection_changed ()
{
	switch (selection->tracks.size()) {
	case 0:
		break;
	default:
		set_selected_mixer_strip (*(selection->tracks.front()));
		break;
	}

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {

                bool yn = (find (selection->tracks.begin(), selection->tracks.end(), *i) != selection->tracks.end());
                
                (*i)->set_selected (yn);
                
                TimeAxisView::Children c = (*i)->get_child_list ();
                for (TimeAxisView::Children::iterator j = c.begin(); j != c.end(); ++j) {
                        (*j)->set_selected (find (selection->tracks.begin(), selection->tracks.end(), j->get()) != selection->tracks.end());
                }

                if (yn) {
                        (*i)->reshow_selection (selection->time);
                } else {
                        (*i)->hide_selection ();
                }
        }

	ActionManager::set_sensitive (ActionManager::track_selection_sensitive_actions, !selection->tracks.empty());
}

void
Editor::time_selection_changed ()
{
	if (Profile->get_sae()) {
		return;
	}

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		(*i)->hide_selection ();
	}

	for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
		(*i)->show_selection (selection->time);
	}

	if (selection->time.empty()) {
		ActionManager::set_sensitive (ActionManager::time_selection_sensitive_actions, false);
	} else {
		ActionManager::set_sensitive (ActionManager::time_selection_sensitive_actions, true);
	}
}

void
Editor::sensitize_the_right_region_actions (bool have_selected_regions)
{
	for (vector<Glib::RefPtr<Action> >::iterator x = ActionManager::region_selection_sensitive_actions.begin();
	     x != ActionManager::region_selection_sensitive_actions.end(); ++x) {

		string accel_path = (*x)->get_accel_path ();
		AccelKey key;

		/* if there is an accelerator, it should always be sensitive
		   to allow for keyboard ops on entered regions.
		*/

		bool known = ActionManager::lookup_entry (accel_path, key);

		if (known && ((key.get_key() != GDK_VoidSymbol) && (key.get_key() != 0))) {
			(*x)->set_sensitive (true);
		} else {
			(*x)->set_sensitive (have_selected_regions);
		}
	}
}


void
Editor::region_selection_changed ()
{
	_regions->block_change_connection (true);
	editor_regions_selection_changed_connection.block(true);

	_regions->unselect_all ();

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		(*i)->set_selected_regionviews (selection->regions);
	}

	_regions->set_selected (selection->regions);

	sensitize_the_right_region_actions (!selection->regions.empty());

	_regions->block_change_connection (false);
	editor_regions_selection_changed_connection.block(false);
}

void
Editor::point_selection_changed ()
{
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		(*i)->set_selected_points (selection->points);
	}
}

void
Editor::select_all_in_track (Selection::Operation op)
{
	list<Selectable *> touched;

	if (!clicked_routeview) {
		return;
	}

	clicked_routeview->get_selectables (0, max_frames, 0, DBL_MAX, touched);

	switch (op) {
	case Selection::Toggle:
		selection->add (touched);
		break;
	case Selection::Set:
		selection->set (touched);
		break;
	case Selection::Extend:
		/* meaningless, because we're selecting everything */
		break;
	case Selection::Add:
		selection->add (touched);
		break;
	}
}

void
Editor::select_all (Selection::Operation op)
{
	list<Selectable *> touched;

	for (TrackViewList::iterator iter = track_views.begin(); iter != track_views.end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}
		(*iter)->get_selectables (0, max_frames, 0, DBL_MAX, touched);
	}
	begin_reversible_command (_("select all"));
	switch (op) {
	case Selection::Add:
		selection->add (touched);
		break;
	case Selection::Toggle:
		selection->add (touched);
		break;
	case Selection::Set:
		selection->set (touched);
		break;
	case Selection::Extend:
		/* meaningless, because we're selecting everything */
		break;
	}
	commit_reversible_command ();
}
void
Editor::invert_selection_in_track ()
{
	list<Selectable *> touched;

	if (!clicked_routeview) {
		return;
	}

	clicked_routeview->get_inverted_selectables (*selection, touched);
	selection->set (touched);
}

void
Editor::invert_selection ()
{
	list<Selectable *> touched;

	for (TrackViewList::iterator iter = track_views.begin(); iter != track_views.end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}
		(*iter)->get_inverted_selectables (*selection, touched);
	}

	selection->set (touched);
}

/** @param start Start time in session frames.
 *  @param end End time in session frames.
 *  @param top Top (lower) y limit in trackview coordinates (ie 0 at the top of the track view)
 *  @param bottom Bottom (higher) y limit in trackview coordinates (ie 0 at the top of the track view)
 *  @param preserve_if_selected true to leave the current selection alone if we're adding to the selection and all of the selectables
 *  within the region are already selected.
 */
bool
Editor::select_all_within (
	framepos_t start, framepos_t end, double top, double bot, const TrackViewList& tracklist, Selection::Operation op, bool preserve_if_selected
	)
{
	list<Selectable*> found;

	for (TrackViewList::const_iterator iter = tracklist.begin(); iter != tracklist.end(); ++iter) {
		
		if ((*iter)->hidden()) {
			continue;
		}

		(*iter)->get_selectables (start, end, top, bot, found);
	}

	if (found.empty()) {
		return false;
	}

	if (preserve_if_selected && op != Selection::Toggle) {
		list<Selectable*>::iterator i = found.begin();
		while (i != found.end() && (*i)->get_selected()) {
			++i;
		}

		if (i == found.end()) {
			return false;
		}
	}

	begin_reversible_command (_("select all within"));
	switch (op) {
	case Selection::Add:
		selection->add (found);
		break;
	case Selection::Toggle:
		selection->toggle (found);
		break;
	case Selection::Set:
		selection->set (found);
		break;
	case Selection::Extend:
		/* not defined yet */
		break;
	}

	commit_reversible_command ();

	return !found.empty();
}

void
Editor::set_selection_from_region ()
{
	if (selection->regions.empty()) {
		return;
	}

	selection->set (selection->regions.start(), selection->regions.end_frame());
	if (!Profile->get_sae()) {
		set_mouse_mode (Editing::MouseRange, false);
	}
}

void
Editor::set_selection_from_punch()
{
	Location* location;

	if ((location = _session->locations()->auto_punch_location()) == 0)  {
		return;
	}

	set_selection_from_range (*location);
}

void
Editor::set_selection_from_loop()
{
	Location* location;

	if ((location = _session->locations()->auto_loop_location()) == 0)  {
		return;
	}
	set_selection_from_range (*location);
}

void
Editor::set_selection_from_range (Location& loc)
{
	begin_reversible_command (_("set selection from range"));
	selection->set (loc.start(), loc.end());
	commit_reversible_command ();

	if (!Profile->get_sae()) {
		set_mouse_mode (Editing::MouseRange, false);
	}
}

void
Editor::select_all_selectables_using_time_selection ()
{
	list<Selectable *> touched;

	if (selection->time.empty()) {
		return;
	}

	nframes64_t start = selection->time[clicked_selection].start;
	nframes64_t end = selection->time[clicked_selection].end;

	if (end - start < 1)  {
		return;
	}

	TrackViewList* ts;

	if (selection->tracks.empty()) {
		ts = &track_views;
	} else {
		ts = &selection->tracks;
	}

	for (TrackViewList::iterator iter = ts->begin(); iter != ts->end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}
		(*iter)->get_selectables (start, end - 1, 0, DBL_MAX, touched);
	}

	begin_reversible_command (_("select all from range"));
	selection->set (touched);
	commit_reversible_command ();
}


void
Editor::select_all_selectables_using_punch()
{
	Location* location = _session->locations()->auto_punch_location();
	list<Selectable *> touched;

	if (location == 0 || (location->end() - location->start() <= 1))  {
		return;
	}


	TrackViewList* ts;

	if (selection->tracks.empty()) {
		ts = &track_views;
	} else {
		ts = &selection->tracks;
	}

	for (TrackViewList::iterator iter = ts->begin(); iter != ts->end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}
		(*iter)->get_selectables (location->start(), location->end() - 1, 0, DBL_MAX, touched);
	}
	begin_reversible_command (_("select all from punch"));
	selection->set (touched);
	commit_reversible_command ();

}

void
Editor::select_all_selectables_using_loop()
{
	Location* location = _session->locations()->auto_loop_location();
	list<Selectable *> touched;

	if (location == 0 || (location->end() - location->start() <= 1))  {
		return;
	}


	TrackViewList* ts;

	if (selection->tracks.empty()) {
		ts = &track_views;
	} else {
		ts = &selection->tracks;
	}

	for (TrackViewList::iterator iter = ts->begin(); iter != ts->end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}
		(*iter)->get_selectables (location->start(), location->end() - 1, 0, DBL_MAX, touched);
	}
	begin_reversible_command (_("select all from loop"));
	selection->set (touched);
	commit_reversible_command ();

}

void
Editor::select_all_selectables_using_cursor (EditorCursor *cursor, bool after)
{
        nframes64_t start;
	nframes64_t end;
	list<Selectable *> touched;

	if (after) {
		begin_reversible_command (_("select all after cursor"));
		start = cursor->current_frame ;
		end = _session->current_end_frame();
	} else {
		if (cursor->current_frame > 0) {
			begin_reversible_command (_("select all before cursor"));
			start = 0;
			end = cursor->current_frame - 1;
		} else {
			return;
		}
	}


	TrackViewList* ts;

	if (selection->tracks.empty()) {
		ts = &track_views;
	} else {
		ts = &selection->tracks;
	}

	for (TrackViewList::iterator iter = ts->begin(); iter != ts->end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}
		(*iter)->get_selectables (start, end, 0, DBL_MAX, touched);
	}
	selection->set (touched);
	commit_reversible_command ();
}

void
Editor::select_all_selectables_using_edit (bool after)
{
        nframes64_t start;
	nframes64_t end;
	list<Selectable *> touched;

	if (after) {
		begin_reversible_command (_("select all after edit"));
		start = get_preferred_edit_position();
		end = _session->current_end_frame();
	} else {
		if ((end = get_preferred_edit_position()) > 1) {
			begin_reversible_command (_("select all before edit"));
			start = 0;
			end -= 1;
		} else {
			return;
		}
	}


	TrackViewList* ts;

	if (selection->tracks.empty()) {
		ts = &track_views;
	} else {
		ts = &selection->tracks;
	}

	for (TrackViewList::iterator iter = ts->begin(); iter != ts->end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}
		(*iter)->get_selectables (start, end, 0, DBL_MAX, touched);
	}
	selection->set (touched);
	commit_reversible_command ();
}

void
Editor::select_all_selectables_between (bool /*within*/)
{
        nframes64_t start;
	nframes64_t end;
	list<Selectable *> touched;

	if (!get_edit_op_range (start, end)) {
		return;
	}

	TrackViewList* ts;

	if (selection->tracks.empty()) {
		ts = &track_views;
	} else {
		ts = &selection->tracks;
	}

	for (TrackViewList::iterator iter = ts->begin(); iter != ts->end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}
		(*iter)->get_selectables (start, end, 0, DBL_MAX, touched);
	}

	selection->set (touched);
}

void
Editor::select_range_between ()
{
        nframes64_t start;
	nframes64_t end;

        if (mouse_mode == MouseRange && !selection->time.empty()) {
                selection->clear_time ();
        }

	if (!get_edit_op_range (start, end)) {
		return;
	}

	set_mouse_mode (MouseRange);
	selection->set (start, end);
}

bool
Editor::get_edit_op_range (nframes64_t& start, nframes64_t& end) const
{
	nframes64_t m;
	bool ignored;

	/* in range mode, use any existing selection */

	if (mouse_mode == MouseRange && !selection->time.empty()) {
		/* we know that these are ordered */
		start = selection->time.start();
		end = selection->time.end_frame();
		return true;
	}

	if (!mouse_frame (m, ignored)) {
		/* mouse is not in a canvas, try playhead+selected marker.
		   this is probably most true when using menus.
		 */

		if (selection->markers.empty()) {
			return false;
		}

		start = selection->markers.front()->position();
		end = _session->audible_frame();

	} else {

		switch (_edit_point) {
		case EditAtPlayhead:
			if (selection->markers.empty()) {
				/* use mouse + playhead */
				start = m;
				end = _session->audible_frame();
			} else {
				/* use playhead + selected marker */
				start = _session->audible_frame();
				end = selection->markers.front()->position();
			}
			break;

		case EditAtMouse:
			/* use mouse + selected marker */
			if (selection->markers.empty()) {
				start = m;
				end = _session->audible_frame();
			} else {
				start = selection->markers.front()->position();
				end = m;
			}
			break;

		case EditAtSelectedMarker:
			/* use mouse + selected marker */
			if (selection->markers.empty()) {

				MessageDialog win (_("No edit range defined"),
						   false,
						   MESSAGE_INFO,
						   BUTTONS_OK);

				win.set_secondary_text (
					_("the edit point is Selected Marker\nbut there is no selected marker."));


				win.set_default_response (RESPONSE_CLOSE);
				win.set_position (Gtk::WIN_POS_MOUSE);
				win.show_all();

				win.run ();

				return false; // NO RANGE
			}
			start = selection->markers.front()->position();
			end = m;
			break;
		}
	}

	if (start == end) {
		return false;
	}

	if (start > end) {
		swap (start, end);
	}

	/* turn range into one delimited by start...end,
	   not start...end-1
	*/

	end++;

	return true;
}

void
Editor::deselect_all ()
{
	selection->clear ();
}

long
Editor::select_range_around_region (RegionView* rv)
{
	assert (rv);
	
	selection->set (&rv->get_time_axis_view());
	
	selection->time.clear ();
	boost::shared_ptr<Region> r = rv->region ();
	return selection->set (r->position(), r->position() + r->length());
}
