// -*- c++ -*-
// Generated by gtkmmproc -- DO NOT MODIFY!
#ifndef _GTKMM_FONTBUTTON_H
#define _GTKMM_FONTBUTTON_H


#include <glibmm.h>

/* $Id$ */

/* fontbutton.h
 * 
 * Copyright (C) 2003 The gtkmm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <gtkmm/button.h>


#ifndef DOXYGEN_SHOULD_SKIP_THIS
typedef struct _GtkFontButton GtkFontButton;
typedef struct _GtkFontButtonClass GtkFontButtonClass;
#endif /* DOXYGEN_SHOULD_SKIP_THIS */


namespace Gtk
{ class FontButton_Class; } // namespace Gtk
namespace Gtk
{

/** A button to launch a font selection dialog.
 * 
 * The Gtk::FontButton is a button which displays the currently selected font and
 * allows to open a font selection dialog to change the font. It is suitable widget
 * for selecting a font in a preference dialog.
 * @ingroup Widgets
 */

class FontButton : public Button
{
  public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  typedef FontButton CppObjectType;
  typedef FontButton_Class CppClassType;
  typedef GtkFontButton BaseObjectType;
  typedef GtkFontButtonClass BaseClassType;
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

  virtual ~FontButton();

#ifndef DOXYGEN_SHOULD_SKIP_THIS

private:
  friend class FontButton_Class;
  static CppClassType fontbutton_class_;

  // noncopyable
  FontButton(const FontButton&);
  FontButton& operator=(const FontButton&);

protected:
  explicit FontButton(const Glib::ConstructParams& construct_params);
  explicit FontButton(GtkFontButton* castitem);

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  static GType get_type()      G_GNUC_CONST;
  static GType get_base_type() G_GNUC_CONST;
#endif

  ///Provides access to the underlying C GtkObject.
  GtkFontButton*       gobj()       { return reinterpret_cast<GtkFontButton*>(gobject_); }

  ///Provides access to the underlying C GtkObject.
  const GtkFontButton* gobj() const { return reinterpret_cast<GtkFontButton*>(gobject_); }


public:
  //C++ methods used to invoke GTK+ virtual functions:
#ifdef GLIBMM_VFUNCS_ENABLED
#endif //GLIBMM_VFUNCS_ENABLED

protected:
  //GTK+ Virtual Functions (override these to change behaviour):
#ifdef GLIBMM_VFUNCS_ENABLED
#endif //GLIBMM_VFUNCS_ENABLED

  //Default Signal Handlers::
#ifdef GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  virtual void on_font_set();
#endif //GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED


private:

public:
  FontButton();
  explicit FontButton(const Glib::ustring& font_name);
  
  
  /** Retrieves the title of the font selection dialog.
   * @return An internal copy of the title string which must not be freed.
   * 
   * @newin2p4.
   */
  Glib::ustring get_title() const;
  
  /** Sets the title for the font selection dialog.  
   * 
   * @newin2p4
   * @param title A string containing the font selection dialog title.
   */
  void set_title(const Glib::ustring& title);
  
  /** Returns: whether the selected font is used in the label.
   * @return Whether the selected font is used in the label.
   * 
   * @newin2p4.
   */
  bool get_use_font() const;
  
  /** If @a use_font  is <tt>true</tt>, the font name will be written using the selected font.  
   * 
   * @newin2p4
   * @param use_font If <tt>true</tt>, font name will be written using font chosen.
   */
  void set_use_font(bool use_font = true);
  
  /** Returns: whether the selected size is used in the label.
   * @return Whether the selected size is used in the label.
   * 
   * @newin2p4.
   */
  bool get_use_size() const;
  
  /** If @a use_size  is <tt>true</tt>, the font name will be written using the selected size.
   * 
   * @newin2p4
   * @param use_size If <tt>true</tt>, font name will be written using the selected size.
   */
  void set_use_size(bool use_size = true);
  
  /** Retrieves the name of the currently selected font.
   * @return An internal copy of the font name which must not be freed.
   * 
   * @newin2p4.
   */
  Glib::ustring get_font_name() const;
  
  /** Sets or updates the currently-displayed font in font picker dialog.
   * @param fontname Name of font to display in font selection dialog.
   * @return Return value of Gtk::FontSelectionDialog::set_font_name() if the
   * font selection dialog exists, otherwise <tt>false</tt>.
   * 
   * @newin2p4.
   */
  bool set_font_name(const Glib::ustring& fontname);
  
  /** Return value: whether the font style will be shown in the label.
   * @return Whether the font style will be shown in the label.
   * 
   * @newin2p4.
   */
  bool get_show_style() const;
  
  /** If @a show_style  is <tt>true</tt>, the font style will be displayed along with name of the selected font.
   * 
   * @newin2p4
   * @param show_style <tt>true</tt> if font style should be displayed in label.
   */
  void set_show_style(bool show_style = true);
  
  /** Return value: whether the font size will be shown in the label.
   * @return Whether the font size will be shown in the label.
   * 
   * @newin2p4.
   */
  bool get_show_size() const;
  
  /** If @a show_size  is <tt>true</tt>, the font size will be displayed along with the name of the selected font.
   * 
   * @newin2p4
   * @param show_size <tt>true</tt> if font size should be displayed in dialog.
   */
  void set_show_size(bool show_size = true);

  #ifdef GLIBMM_PROPERTIES_ENABLED
/** The title of the font selection dialog.
   *
   * You rarely need to use properties because there are get_ and set_ methods for almost all of them.
   * @return A PropertyProxy that allows you to get or set the property of the value, or receive notification when
   * the value of the property changes.
   */
  Glib::PropertyProxy<Glib::ustring> property_title() ;
#endif //#GLIBMM_PROPERTIES_ENABLED

#ifdef GLIBMM_PROPERTIES_ENABLED
/** The title of the font selection dialog.
   *
   * You rarely need to use properties because there are get_ and set_ methods for almost all of them.
   * @return A PropertyProxy that allows you to get or set the property of the value, or receive notification when
   * the value of the property changes.
   */
  Glib::PropertyProxy_ReadOnly<Glib::ustring> property_title() const;
#endif //#GLIBMM_PROPERTIES_ENABLED

  #ifdef GLIBMM_PROPERTIES_ENABLED
/** The name of the selected font.
   *
   * You rarely need to use properties because there are get_ and set_ methods for almost all of them.
   * @return A PropertyProxy that allows you to get or set the property of the value, or receive notification when
   * the value of the property changes.
   */
  Glib::PropertyProxy<Glib::ustring> property_font_name() ;
#endif //#GLIBMM_PROPERTIES_ENABLED

#ifdef GLIBMM_PROPERTIES_ENABLED
/** The name of the selected font.
   *
   * You rarely need to use properties because there are get_ and set_ methods for almost all of them.
   * @return A PropertyProxy that allows you to get or set the property of the value, or receive notification when
   * the value of the property changes.
   */
  Glib::PropertyProxy_ReadOnly<Glib::ustring> property_font_name() const;
#endif //#GLIBMM_PROPERTIES_ENABLED

  #ifdef GLIBMM_PROPERTIES_ENABLED
/** Whether the label is drawn in the selected font.
   *
   * You rarely need to use properties because there are get_ and set_ methods for almost all of them.
   * @return A PropertyProxy that allows you to get or set the property of the value, or receive notification when
   * the value of the property changes.
   */
  Glib::PropertyProxy<bool> property_use_font() ;
#endif //#GLIBMM_PROPERTIES_ENABLED

#ifdef GLIBMM_PROPERTIES_ENABLED
/** Whether the label is drawn in the selected font.
   *
   * You rarely need to use properties because there are get_ and set_ methods for almost all of them.
   * @return A PropertyProxy that allows you to get or set the property of the value, or receive notification when
   * the value of the property changes.
   */
  Glib::PropertyProxy_ReadOnly<bool> property_use_font() const;
#endif //#GLIBMM_PROPERTIES_ENABLED

  #ifdef GLIBMM_PROPERTIES_ENABLED
/** Whether the label is drawn with the selected font size.
   *
   * You rarely need to use properties because there are get_ and set_ methods for almost all of them.
   * @return A PropertyProxy that allows you to get or set the property of the value, or receive notification when
   * the value of the property changes.
   */
  Glib::PropertyProxy<bool> property_use_size() ;
#endif //#GLIBMM_PROPERTIES_ENABLED

#ifdef GLIBMM_PROPERTIES_ENABLED
/** Whether the label is drawn with the selected font size.
   *
   * You rarely need to use properties because there are get_ and set_ methods for almost all of them.
   * @return A PropertyProxy that allows you to get or set the property of the value, or receive notification when
   * the value of the property changes.
   */
  Glib::PropertyProxy_ReadOnly<bool> property_use_size() const;
#endif //#GLIBMM_PROPERTIES_ENABLED

  #ifdef GLIBMM_PROPERTIES_ENABLED
/** Whether the selected font style is shown in the label.
   *
   * You rarely need to use properties because there are get_ and set_ methods for almost all of them.
   * @return A PropertyProxy that allows you to get or set the property of the value, or receive notification when
   * the value of the property changes.
   */
  Glib::PropertyProxy<bool> property_show_style() ;
#endif //#GLIBMM_PROPERTIES_ENABLED

#ifdef GLIBMM_PROPERTIES_ENABLED
/** Whether the selected font style is shown in the label.
   *
   * You rarely need to use properties because there are get_ and set_ methods for almost all of them.
   * @return A PropertyProxy that allows you to get or set the property of the value, or receive notification when
   * the value of the property changes.
   */
  Glib::PropertyProxy_ReadOnly<bool> property_show_style() const;
#endif //#GLIBMM_PROPERTIES_ENABLED

  #ifdef GLIBMM_PROPERTIES_ENABLED
/** Whether selected font size is shown in the label.
   *
   * You rarely need to use properties because there are get_ and set_ methods for almost all of them.
   * @return A PropertyProxy that allows you to get or set the property of the value, or receive notification when
   * the value of the property changes.
   */
  Glib::PropertyProxy<bool> property_show_size() ;
#endif //#GLIBMM_PROPERTIES_ENABLED

#ifdef GLIBMM_PROPERTIES_ENABLED
/** Whether selected font size is shown in the label.
   *
   * You rarely need to use properties because there are get_ and set_ methods for almost all of them.
   * @return A PropertyProxy that allows you to get or set the property of the value, or receive notification when
   * the value of the property changes.
   */
  Glib::PropertyProxy_ReadOnly<bool> property_show_size() const;
#endif //#GLIBMM_PROPERTIES_ENABLED


  /** The font_set signal is emitted when the user selects a font. When handling this signal,
   * use get_font_name() to find out which font was just selected.
   *
   * @par Prototype:
   * <tt>void on_my_%font_set()</tt>
   */

  Glib::SignalProxy0< void > signal_font_set();
                                             

};


} // namespace Gtk


namespace Glib
{
  /** A Glib::wrap() method for this object.
   * 
   * @param object The C instance.
   * @param take_copy False if the result should take ownership of the C instance. True if it should take a new copy or ref.
   * @result A C++ instance that wraps this C instance.
   *
   * @relates Gtk::FontButton
   */
  Gtk::FontButton* wrap(GtkFontButton* object, bool take_copy = false);
} //namespace Glib


#endif /* _GTKMM_FONTBUTTON_H */

