/* ATK -  Accessibility Toolkit
 * Copyright 2001 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#include "atkcomponent.h"

static AtkObject* atk_component_real_get_accessible_at_point (AtkComponent *component,
                                                              gint         x,
                                                              gint         y);

GType
atk_component_get_type ()
{
  static GType type = 0;

  if (!type) {
    static const GTypeInfo tinfo =
    {
      sizeof (AtkComponentIface),
      NULL,
      NULL,

    };

    type = g_type_register_static (G_TYPE_INTERFACE, "AtkComponent", &tinfo, 0);
  }

  return type;
}

/**
 *atk_component_add_focus_handler:
 *@component: The #AtkComponent to attach the @handler to
 *@handler: The #AtkFocusHandler to be attached to @component
 *
 * Add the specified handler to the set of functions to be called 
 * when this object receives focus (in or out) events.
 *
 * Returns:
 **/
guint
atk_component_add_focus_handler (AtkComponent    *component,
                                 AtkFocusHandler handler)
{
  AtkComponentIface *iface = NULL;
  g_return_val_if_fail (component != NULL, 0);
  g_return_val_if_fail (ATK_IS_COMPONENT (component), 0);

  iface = ATK_COMPONENT_GET_IFACE (component);

  if (iface->add_focus_handler)
    return (iface->add_focus_handler) (component, handler);
  else
    return 0;
}

/**
 *atk_component_remove_focus_handler:
 *@component: the #AtkComponent to remove the focus handler from
 *@handler_id: the handler id of the focus handler to be removed
 * from @component
 *
 * Remove the handler specified by @handler_id from the list of
 * functions to be executed when this object receives focus (in or out)
 **/
void
atk_component_remove_focus_handler (AtkComponent    *component,
                                    guint           handler_id)
{
  AtkComponentIface *iface = NULL;
  g_return_if_fail (component != NULL);
  g_return_if_fail (ATK_IS_COMPONENT (component));

  iface = ATK_COMPONENT_GET_IFACE (component);

  if (iface->remove_focus_handler)
    (iface->remove_focus_handler) (component, handler_id);
}

/**
 *atk_component_contains:
 *@component: the #AtkComponent
 *@x: x coordinate relative to the coordinate system of @component
 *@y: y coordinate relative to the coordinate system of @component
 *
 * Checks whether the specified point is within the extent of the @component,
 * the points x and y coordinates are defined to be relative to the 
 * coordinate system of the @component.
 *
 * Returns: a #gboolean indicating whether the specified point is within
 *  the extent of the @component
 **/
gboolean
atk_component_contains (AtkComponent    *component,
                        gint            x,
                        gint            y)
{
  AtkComponentIface *iface = NULL;
  g_return_val_if_fail (component != NULL, FALSE);
  g_return_val_if_fail (ATK_IS_COMPONENT (component), FALSE);

  iface = ATK_COMPONENT_GET_IFACE (component);

  if (iface->contains)
    return (iface->contains) (component, x, y);
  else
    return FALSE;
}

/**
 *atk_component_get_accessible_at_point:
 *@component: the #AtkComponent
 *@x: local x coordinate
 *@y: local y coordinate
 *
 * Gets the accessible child, if one exists, contained at the local
 * coordinate point specified by @x and @y.
 *
 *Returns: the accessible child, if one exists, contained at the local
 * coordinate point specified by @x and @y.
 **/
AtkObject*
atk_component_get_accessible_at_point (AtkComponent    *component,
                                       gint            x,
                                       gint            y)
{
  AtkComponentIface *iface = NULL;
  g_return_val_if_fail (component != NULL, NULL);
  g_return_val_if_fail (ATK_IS_COMPONENT (component), NULL);

  iface = ATK_COMPONENT_GET_IFACE (component);

  if (iface->get_accessible_at_point)
    return (iface->get_accessible_at_point) (component, x, y);
  else
  {
    /*
     * if this method is not overridden use the default implementation.
     */
    return atk_component_real_get_accessible_at_point (component, x, y);
  }
}

/**
 *atk_component_get_extents:
 *@component: an #AtkComponent
 *@x: address of #gint to put x coordinate
 *@y: address of #gint to put y coordinate
 *@width: address of #gint to put width
 *@height: address of #gint to put height
 *
 * Gets the rectangle which gives the extent of the @component.
 *
 **/
void
atk_component_get_extents    (AtkComponent    *component,
                              gint            *x,
                              gint            *y,
                              gint            *width,
                              gint            *height)
{
  AtkComponentIface *iface = NULL;
  g_return_if_fail (component != NULL);
  g_return_if_fail (ATK_IS_COMPONENT (component));

  iface = ATK_COMPONENT_GET_IFACE (component);

  if (iface->get_extents)
    (iface->get_extents) (component, x, y, width, height);
}

/**
 *atk_component_get_position:
 *@component: an #AtkComponent
 *@x: address of #gint to put x coordinate position
 *@y: address of #gint to put y coordinate position
 *
 * Gets the position of @component relative to the parent in the form of 
 * a point specifying @component's top-left corner in the screen's
 * coordinate space.
 **/
void
atk_component_get_position   (AtkComponent    *component,
                              gint            *x,
                              gint            *y)
{
  AtkComponentIface *iface = NULL;
  g_return_if_fail (component != NULL);
  g_return_if_fail (ATK_IS_COMPONENT (component));

  iface = ATK_COMPONENT_GET_IFACE (component);

  if (iface->get_position)
    (iface->get_position) (component, x, y);
}

/**
 *atk_component_get_position_on_screen:
 *@component: an #AtkComponent
 *@x: address of #gint to put x coordinate position
 *@y: address of #gint to put y coordinate position
 *
 * Gets the position of the @component on the screen
 **/
void
atk_component_get_position_on_screen (AtkComponent    *component,
                                      gint            *x,
                                      gint            *y)
{
  AtkComponentIface *iface = NULL;
  g_return_if_fail (component != NULL);
  g_return_if_fail (ATK_IS_COMPONENT (component));

  iface = ATK_COMPONENT_GET_IFACE (component);

  if (iface->get_position_on_screen)
    return (iface->get_position_on_screen) (component, x, y);
}

/**
 *atk_component_get_size:
 *@component: an #AtkComponent
 *@width: address of #gint to put width of @component
 *@height: address of #gint to put height of @component
 *
 * Gets the size of the @component.
 **/
void
atk_component_get_size       (AtkComponent    *component,
                              gint            *x,
                              gint            *y)
{
  AtkComponentIface *iface = NULL;
  g_return_if_fail (component != NULL);
  g_return_if_fail (ATK_IS_COMPONENT (component));

  iface = ATK_COMPONENT_GET_IFACE (component);

  if (iface->get_size)
    (iface->get_size) (component, x, y);
}

/**
 *atk_component_grab_focus:
 *@component: an #AtkComponent
 *
 * Grabs focus for this @component
 **/
void
atk_component_grab_focus (AtkComponent    *component)
{
  AtkComponentIface *iface = NULL;
  g_return_if_fail (component != NULL);
  g_return_if_fail (ATK_IS_COMPONENT (component));

  iface = ATK_COMPONENT_GET_IFACE (component);

  if (iface->grab_focus)
    (iface->grab_focus) (component);
}

/**
 *atk_component_set_extents:
 *@component: an #AtkComponent
 *@x: x coordinate to set for @component
 *@y: y coordinate to set for @component
 *@width: width to set for @component
 *@height: height to set for @component
 *
 * Sets the extents of @component
 **/
void
atk_component_set_extents   (AtkComponent    *component,
                             gint            x,
                             gint            y,
                             gint            width,
                             gint            height)
{
  AtkComponentIface *iface = NULL;
  g_return_if_fail (component != NULL);
  g_return_if_fail (ATK_IS_COMPONENT (component));

  iface = ATK_COMPONENT_GET_IFACE (component);

  if (iface->set_extents)
    (iface->set_extents) (component, x, y, width, height);
}

/**
 *atk_component_set_position:
 *@component: an #AtkComponent
 *@x: x coordinate
 *@y: y coordinate
 *
 * Sets the postition of @component
 **/
void
atk_component_set_position   (AtkComponent    *component,
                              gint            x,
                              gint            y)
{
  AtkComponentIface *iface = NULL;
  g_return_if_fail (component != NULL);
  g_return_if_fail (ATK_IS_COMPONENT (component));

  iface = ATK_COMPONENT_GET_IFACE (component);

  if (iface->set_position)
    (iface->set_position) (component, x, y);
}

/**
 *atk_component_set_size:
 *@component: an #AtkComponent
 *@width: width to set for @component
 *@height: height to set for @component
 *
 * Set the size of the @component
 **/
void
atk_component_set_size       (AtkComponent    *component,
                              gint            x,
                              gint            y)
{
  AtkComponentIface *iface = NULL;
  g_return_if_fail (component != NULL);
  g_return_if_fail (ATK_IS_COMPONENT (component));

  iface = ATK_COMPONENT_GET_IFACE (component);

  if (iface->set_size)
    (iface->set_size) (component, x, y);
}

static AtkObject* 
atk_component_real_get_accessible_at_point (AtkComponent *component,
                                            gint         x,
                                            gint         y)
{
  gint count, i;

  count = atk_object_get_n_accessible_children (ATK_OBJECT (component));

  g_return_val_if_fail (count != 0, NULL);

  for (i = 0; i < count; i++)
  {
    AtkObject *obj;

    obj = atk_object_ref_accessible_child (ATK_OBJECT (component), i);

    if (obj != NULL)
    {
      if (atk_component_contains (ATK_COMPONENT (obj), x, y))
      {
        g_object_unref (obj);
        return obj;
      }
      else
      {
        g_object_unref (obj);
      }
    }
  }
  return NULL;
}
