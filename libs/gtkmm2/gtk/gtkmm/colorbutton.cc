// Generated by gtkmmproc -- DO NOT MODIFY!


#include <gtkmm/colorbutton.h>
#include <gtkmm/private/colorbutton_p.h>

// -*- c++ -*-
/* $Id$ */

/*
 *
 * Copyright 2003 The gtkmm Development Team
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

#include <gtk/gtkcolorbutton.h>
#include <gdkmm/color.h>

namespace Gtk
{

Gdk::Color ColorButton::get_color() const
{
  Gdk::Color color;
  gtk_color_button_get_color(const_cast<GtkColorButton*>(gobj()), color.gobj());
  return color;
}

  
} // namespace Gtk


namespace
{


static const Glib::SignalProxyInfo ColorButton_signal_color_set_info =
{
  "color-set",
  (GCallback) &Glib::SignalProxyNormal::slot0_void_callback,
  (GCallback) &Glib::SignalProxyNormal::slot0_void_callback
};


} // anonymous namespace


namespace Glib
{

Gtk::ColorButton* wrap(GtkColorButton* object, bool take_copy)
{
  return dynamic_cast<Gtk::ColorButton *> (Glib::wrap_auto ((GObject*)(object), take_copy));
}

} /* namespace Glib */

namespace Gtk
{


/* The *_Class implementation: */

const Glib::Class& ColorButton_Class::init()
{
  if(!gtype_) // create the GType if necessary
  {
    // Glib::Class has to know the class init function to clone custom types.
    class_init_func_ = &ColorButton_Class::class_init_function;

    // This is actually just optimized away, apparently with no harm.
    // Make sure that the parent type has been created.
    //CppClassParent::CppObjectType::get_type();

    // Create the wrapper type, with the same class/instance size as the base type.
    register_derived_type(gtk_color_button_get_type());

    // Add derived versions of interfaces, if the C type implements any interfaces:
  }

  return *this;
}

void ColorButton_Class::class_init_function(void* g_class, void* class_data)
{
  BaseClassType *const klass = static_cast<BaseClassType*>(g_class);
  CppClassParent::class_init_function(klass, class_data);

#ifdef GLIBMM_VFUNCS_ENABLED
#endif //GLIBMM_VFUNCS_ENABLED

#ifdef GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
  klass->color_set = &color_set_callback;
#endif //GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
}

#ifdef GLIBMM_VFUNCS_ENABLED
#endif //GLIBMM_VFUNCS_ENABLED

#ifdef GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
void ColorButton_Class::color_set_callback(GtkColorButton* self)
{
  Glib::ObjectBase *const obj_base = static_cast<Glib::ObjectBase*>(
      Glib::ObjectBase::_get_current_wrapper((GObject*)self));

  // Non-gtkmmproc-generated custom classes implicitly call the default
  // Glib::ObjectBase constructor, which sets is_derived_. But gtkmmproc-
  // generated classes can use this optimisation, which avoids the unnecessary
  // parameter conversions if there is no possibility of the virtual function
  // being overridden:
  if(obj_base && obj_base->is_derived_())
  {
    CppObjectType *const obj = dynamic_cast<CppObjectType* const>(obj_base);
    if(obj) // This can be NULL during destruction.
    {
      #ifdef GLIBMM_EXCEPTIONS_ENABLED
      try // Trap C++ exceptions which would normally be lost because this is a C callback.
      {
      #endif //GLIBMM_EXCEPTIONS_ENABLED
        // Call the virtual member method, which derived classes might override.
        obj->on_color_set();
        return;
      #ifdef GLIBMM_EXCEPTIONS_ENABLED
      }
      catch(...)
      {
        Glib::exception_handlers_invoke();
      }
      #endif //GLIBMM_EXCEPTIONS_ENABLED
    }
  }
  
  BaseClassType *const base = static_cast<BaseClassType*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(self)) // Get the parent class of the object class (The original underlying C class).
    );

  // Call the original underlying C function:
  if(base && base->color_set)
    (*base->color_set)(self);
}
#endif //GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED


Glib::ObjectBase* ColorButton_Class::wrap_new(GObject* o)
{
  return manage(new ColorButton((GtkColorButton*)(o)));

}


/* The implementation: */

ColorButton::ColorButton(const Glib::ConstructParams& construct_params)
:
  Gtk::Button(construct_params)
{
  }

ColorButton::ColorButton(GtkColorButton* castitem)
:
  Gtk::Button((GtkButton*)(castitem))
{
  }

ColorButton::~ColorButton()
{
  destroy_();
}

ColorButton::CppClassType ColorButton::colorbutton_class_; // initialize static member

GType ColorButton::get_type()
{
  return colorbutton_class_.init().get_type();
}

GType ColorButton::get_base_type()
{
  return gtk_color_button_get_type();
}


ColorButton::ColorButton()
:
  // Mark this class as non-derived to allow C++ vfuncs to be skipped.
  Glib::ObjectBase(0),
  Gtk::Button(Glib::ConstructParams(colorbutton_class_.init()))
{
  }

ColorButton::ColorButton(const Gdk::Color& color)
:
  // Mark this class as non-derived to allow C++ vfuncs to be skipped.
  Glib::ObjectBase(0),
  Gtk::Button(Glib::ConstructParams(colorbutton_class_.init(), "color", (color).gobj(), static_cast<char*>(0)))
{
  }

void ColorButton::set_color(const Gdk::Color& color)
{
gtk_color_button_set_color(gobj(), (color).gobj()); 
}

void ColorButton::set_alpha(guint16 alpha)
{
gtk_color_button_set_alpha(gobj(), alpha); 
}

guint16 ColorButton::get_alpha() const
{
  return gtk_color_button_get_alpha(const_cast<GtkColorButton*>(gobj()));
}

void ColorButton::set_use_alpha(bool use_alpha)
{
gtk_color_button_set_use_alpha(gobj(), static_cast<int>(use_alpha)); 
}

bool ColorButton::get_use_alpha() const
{
  return gtk_color_button_get_use_alpha(const_cast<GtkColorButton*>(gobj()));
}

void ColorButton::set_title(const Glib::ustring& title)
{
gtk_color_button_set_title(gobj(), title.c_str()); 
}

Glib::ustring ColorButton::get_title() const
{
  return Glib::convert_const_gchar_ptr_to_ustring(gtk_color_button_get_title(const_cast<GtkColorButton*>(gobj())));
}


Glib::SignalProxy0< void > ColorButton::signal_color_set()
{
  return Glib::SignalProxy0< void >(this, &ColorButton_signal_color_set_info);
}


#ifdef GLIBMM_PROPERTIES_ENABLED
Glib::PropertyProxy<bool> ColorButton::property_use_alpha() 
{
  return Glib::PropertyProxy<bool>(this, "use-alpha");
}
#endif //GLIBMM_PROPERTIES_ENABLED

#ifdef GLIBMM_PROPERTIES_ENABLED
Glib::PropertyProxy_ReadOnly<bool> ColorButton::property_use_alpha() const
{
  return Glib::PropertyProxy_ReadOnly<bool>(this, "use-alpha");
}
#endif //GLIBMM_PROPERTIES_ENABLED

#ifdef GLIBMM_PROPERTIES_ENABLED
Glib::PropertyProxy<Glib::ustring> ColorButton::property_title() 
{
  return Glib::PropertyProxy<Glib::ustring>(this, "title");
}
#endif //GLIBMM_PROPERTIES_ENABLED

#ifdef GLIBMM_PROPERTIES_ENABLED
Glib::PropertyProxy_ReadOnly<Glib::ustring> ColorButton::property_title() const
{
  return Glib::PropertyProxy_ReadOnly<Glib::ustring>(this, "title");
}
#endif //GLIBMM_PROPERTIES_ENABLED

#ifdef GLIBMM_PROPERTIES_ENABLED
Glib::PropertyProxy<Gdk::Color> ColorButton::property_color() 
{
  return Glib::PropertyProxy<Gdk::Color>(this, "color");
}
#endif //GLIBMM_PROPERTIES_ENABLED

#ifdef GLIBMM_PROPERTIES_ENABLED
Glib::PropertyProxy_ReadOnly<Gdk::Color> ColorButton::property_color() const
{
  return Glib::PropertyProxy_ReadOnly<Gdk::Color>(this, "color");
}
#endif //GLIBMM_PROPERTIES_ENABLED

#ifdef GLIBMM_PROPERTIES_ENABLED
Glib::PropertyProxy<guint16> ColorButton::property_alpha() 
{
  return Glib::PropertyProxy<guint16>(this, "alpha");
}
#endif //GLIBMM_PROPERTIES_ENABLED

#ifdef GLIBMM_PROPERTIES_ENABLED
Glib::PropertyProxy_ReadOnly<guint16> ColorButton::property_alpha() const
{
  return Glib::PropertyProxy_ReadOnly<guint16>(this, "alpha");
}
#endif //GLIBMM_PROPERTIES_ENABLED


#ifdef GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED
void Gtk::ColorButton::on_color_set()
{
  BaseClassType *const base = static_cast<BaseClassType*>(
      g_type_class_peek_parent(G_OBJECT_GET_CLASS(gobject_)) // Get the parent class of the object class (The original underlying C class).
  );

  if(base && base->color_set)
    (*base->color_set)(gobj());
}
#endif //GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED

#ifdef GLIBMM_VFUNCS_ENABLED
#endif //GLIBMM_VFUNCS_ENABLED


} // namespace Gtk


