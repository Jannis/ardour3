/*
    Copyright (C) 2007 Paul Davis 

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

#ifndef __ardour_ui_bundle_manager_h__
#define __ardour_ui_bundle_manager_h__

#include <gtkmm/treeview.h>
#include <gtkmm/liststore.h>
#include "ardour_dialog.h"
#include "port_matrix.h"

namespace ARDOUR {
	class Session;
	class Bundle;
}

class BundleEditorMatrix : public PortMatrix
{
  public:
	BundleEditorMatrix (ARDOUR::Session &, boost::shared_ptr<ARDOUR::Bundle>);

	void set_state (int, std::string const &, bool, uint32_t);
	bool get_state (int, std::string const &) const;
	uint32_t n_rows () const;
	uint32_t maximum_rows () const;
	uint32_t minimum_rows () const;
	std::string row_name (int) const;
	void add_row ();
	void remove_row (int);
	std::string row_descriptor () const;

  private:

	boost::shared_ptr<ARDOUR::UserBundle> _bundle;
};

class BundleEditor : public ArdourDialog
{
  public:
	BundleEditor (ARDOUR::Session &, boost::shared_ptr<ARDOUR::UserBundle>, bool);

  protected:
	void on_map ();

  private:
	void name_changed ();
	void input_or_output_changed ();
	void type_changed ();
	
	BundleEditorMatrix _matrix;
	boost::shared_ptr<ARDOUR::UserBundle> _bundle;
	Gtk::Entry _name;
	Gtk::ComboBoxText _input_or_output;
	Gtk::ComboBoxText _type;
};

class BundleManager : public ArdourDialog
{
  public:
	BundleManager (ARDOUR::Session &);

  private:

	void new_clicked ();
	void edit_clicked ();
	void delete_clicked ();
	void add_bundle (boost::shared_ptr<ARDOUR::Bundle>);
	void bundle_name_changed (boost::shared_ptr<ARDOUR::UserBundle>);
	void set_button_sensitivity ();

	class ModelColumns : public Gtk::TreeModelColumnRecord
	{
	public:
		ModelColumns () {
			add (name);
			add (bundle);
		}
		
		Gtk::TreeModelColumn<Glib::ustring> name;
		Gtk::TreeModelColumn<boost::shared_ptr<ARDOUR::UserBundle> > bundle;
	};
	
	Gtk::TreeView _tree_view;
	Glib::RefPtr<Gtk::ListStore> _list_model;
	ModelColumns _list_model_columns;
	ARDOUR::Session& _session;
	Gtk::Button edit_button;
	Gtk::Button delete_button;
};

#endif
