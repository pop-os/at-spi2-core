/*
 * AT-SPI - Assistive Technology Service Provider Interface
 * (Gnome Accessibility Project; http://developer.gnome.org/projects/gap)
 *
 * Copyright 2024 System76 Inc.
 *
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "atspi-device-cosmic.h"
#include "atspi-private.h"
#include "glib-unix.h"

#include "cosmic-atspi.h"

#include <libei.h>
#include <poll.h>
#include <sys/mman.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#define ATSPI_VIRTUAL_MODIFIER_MASK 0x0000f000

typedef struct _AtspiDeviceCosmicPrivate AtspiDeviceCosmicPrivate;
struct _AtspiDeviceCosmicPrivate
{
  struct wl_display *wl_display;
  struct wl_event_queue *wl_event_queue;
  struct cosmic_atspi_manager_v1 *atspi_manager;
  int wayland_source_id;
  struct ei *ei;
  int ei_source_id;
  struct xkb_context *xkb_context;
  struct xkb_keymap *xkb_keymap;
  struct xkb_state *xkb_state;
  GSList *modifiers;
};

GObjectClass *device_cosmic_parent_class;

G_DEFINE_TYPE_WITH_CODE (AtspiDeviceCosmic, atspi_device_cosmic, ATSPI_TYPE_DEVICE, G_ADD_PRIVATE (AtspiDeviceCosmic))

typedef struct
{
  guint keycode;
  guint modifier;
} AtspiLibeiKeyModifier;

static guint
find_virtual_mapping (AtspiDeviceCosmic *libei_device, gint keycode)
{
  AtspiDeviceCosmicPrivate *priv = atspi_device_cosmic_get_instance_private (libei_device);
  GSList *l;

  for (l = priv->modifiers; l; l = l->next)
    {
      AtspiLibeiKeyModifier *entry = l->data;
      if (entry->keycode == keycode)
        return entry->modifier;
    }

  return 0;
}

static gboolean
check_virtual_modifier (AtspiDeviceCosmic *libei_device, guint modifier)
{
  AtspiDeviceCosmicPrivate *priv = atspi_device_cosmic_get_instance_private (libei_device);
  GSList *l;

  if (modifier == (1 << ATSPI_MODIFIER_NUMLOCK))
    return TRUE;

  for (l = priv->modifiers; l; l = l->next)
    {
      AtspiLibeiKeyModifier *entry = l->data;
      if (entry->modifier == modifier)
        return TRUE;
    }

  return FALSE;
}

static guint
get_unused_virtual_modifier (AtspiDeviceCosmic *libei_device)
{
  guint ret = 0x1000;

  while (ret < 0x10000)
    {
      if (!check_virtual_modifier (libei_device, ret))
        return ret;
      ret <<= 1;
    }

  return 0;
}

static gboolean dispatch_wayland(gint fd, GIOCondition condition, gpointer user_data) {
  AtspiDeviceCosmic *device = user_data;
  AtspiDeviceCosmicPrivate *priv = atspi_device_cosmic_get_instance_private (device);

  if (!(condition & G_IO_IN))
    return TRUE;

  wl_display_dispatch_queue (priv->wl_display, priv->wl_event_queue);

  return TRUE;
}

static gboolean dispatch_libei(gint fd, GIOCondition condition, gpointer user_data) {
  AtspiDeviceCosmic *device = user_data;
  AtspiDeviceCosmicPrivate *priv = atspi_device_cosmic_get_instance_private (device);

  if (!(condition & G_IO_IN))
    return TRUE;

  struct ei_event *event;

  ei_dispatch(priv->ei);

  while ((event = ei_get_event(priv->ei)) != NULL) {
    switch (ei_event_get_type(event)) {
      case EI_EVENT_SEAT_ADDED:
	ei_seat_bind_capabilities(ei_event_get_seat(event), EI_DEVICE_CAP_KEYBOARD, NULL);
	break;
      // TODO multiple devices
      case EI_EVENT_DEVICE_ADDED:
        struct ei_keymap *keymap = ei_device_keyboard_get_keymap(ei_event_get_device(event));
        int format = ei_keymap_get_type(keymap);
        int fd = ei_keymap_get_fd(keymap);
        size_t size = ei_keymap_get_size(keymap);
        char *buffer = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	// TODO: test that device is keybaord device, and has keymap?
	// if there's already and xkb_keymap/xkb_state, remove it.
	// produce error/warning if a device is added before first one removed?
        priv->xkb_keymap = xkb_keymap_new_from_buffer(priv->xkb_context, buffer, size, format, XKB_KEYMAP_COMPILE_NO_FLAGS);
        munmap(buffer, size);
        priv->xkb_state = xkb_state_new(priv->xkb_keymap);
	break;
      case EI_EVENT_KEYBOARD_MODIFIERS:
        if (!priv->xkb_state)
          continue;

        uint32_t group = ei_event_keyboard_get_xkb_group(event);
        xkb_state_update_mask(priv->xkb_state,
            ei_event_keyboard_get_xkb_mods_depressed(event),
            ei_event_keyboard_get_xkb_mods_latched(event),
            ei_event_keyboard_get_xkb_mods_locked(event),
            group, group, group);
        break;
      case EI_EVENT_KEYBOARD_KEY:
        if (!priv->xkb_state)
          continue;

        uint32_t keycode = ei_event_keyboard_get_key(event) + 8;
        bool pressed = ei_event_keyboard_get_key_is_press(event);
        int keysym = xkb_state_key_get_one_sym(priv->xkb_state, keycode);
        xkb_mod_mask_t modifiers = xkb_state_serialize_mods(priv->xkb_state, XKB_STATE_MODS_EFFECTIVE);
        gchar text[16];
        xkb_state_key_get_utf8(priv->xkb_state, keycode, text, 16);
        atspi_device_notify_key (ATSPI_DEVICE (device), pressed, keycode, keysym, modifiers, text);
        break;
      default:
        break;
    }
    ei_event_unref(event);
  }

  return TRUE;
}

xkb_keysym_t keycode_to_keysym(struct xkb_keymap *xkb_keymap, gint keycode) {
  const xkb_keysym_t *syms;
  // XXX layout?
  xkb_keymap_key_get_syms_by_level(xkb_keymap, keycode, 0, 0, &syms);
  if (syms)
    return syms[0];
  else
    return XKB_KEY_NoSymbol;
}

static void convert_mods_to_wl(AtspiDeviceCosmic *libei_device, guint mods, uint32_t *real_mods, struct wl_array *virtual_mods) {
  AtspiDeviceCosmicPrivate *priv = atspi_device_cosmic_get_instance_private (libei_device);

  wl_array_init(virtual_mods);

  for (GSList *l = priv->modifiers; l; l = l->next)
    {
      AtspiLibeiKeyModifier *entry = l->data;
      if (entry->modifier & mods) {
        uint32_t keycode = entry->keycode - 8;
        void *keycode_ptr = wl_array_add(virtual_mods, 4);
	if (keycode_ptr != NULL)
	    memcpy(keycode_ptr, &keycode, 4);
      }
    }

  *real_mods = mods & ~ATSPI_VIRTUAL_MODIFIER_MASK;
}

static gboolean
atspi_device_cosmic_add_key_grab (AtspiDevice *device, AtspiKeyDefinition *kd)
{
  AtspiDeviceCosmic *libei_device = ATSPI_DEVICE_LIBEI (device);
  AtspiDeviceCosmicPrivate *priv = atspi_device_cosmic_get_instance_private (libei_device);

  uint32_t real_mods;
  struct wl_array virtual_mods;
  convert_mods_to_wl(libei_device, kd->modifiers, &real_mods, &virtual_mods);
  cosmic_atspi_manager_v1_add_key_grab(priv->atspi_manager, real_mods, &virtual_mods, kd->keycode - 8);
  wl_display_flush(priv->wl_display);
  wl_array_release(&virtual_mods);

  return TRUE;
}

static void
atspi_device_cosmic_remove_key_grab (AtspiDevice *device, guint id)
{
  AtspiDeviceCosmic *libei_device = ATSPI_DEVICE_LIBEI (device);
  AtspiDeviceCosmicPrivate *priv = atspi_device_cosmic_get_instance_private (libei_device);

  AtspiKeyDefinition *kd;
  kd = atspi_device_get_grab_by_id (device, id);

  uint32_t real_mods;
  struct wl_array virtual_mods;
  convert_mods_to_wl(libei_device, kd->modifiers, &real_mods, &virtual_mods);
  cosmic_atspi_manager_v1_remove_key_grab(priv->atspi_manager, real_mods, &virtual_mods, kd->keycode - 8);
  wl_display_flush(priv->wl_display);
  wl_array_release(&virtual_mods);
}

static gboolean
atspi_device_cosmic_grab_keyboard (AtspiDevice *device)
{
  AtspiDeviceCosmic *libei_device = ATSPI_DEVICE_LIBEI (device);
  AtspiDeviceCosmicPrivate *priv = atspi_device_cosmic_get_instance_private (libei_device);

  cosmic_atspi_manager_v1_grab_keyboard(priv->atspi_manager);

  return TRUE;
}

static void
atspi_device_cosmic_ungrab_keyboard (AtspiDevice *device)
{
  AtspiDeviceCosmic *libei_device = ATSPI_DEVICE_LIBEI (device);
  AtspiDeviceCosmicPrivate *priv = atspi_device_cosmic_get_instance_private (libei_device);

  cosmic_atspi_manager_v1_grab_keyboard(priv->atspi_manager);
}

static guint
atspi_device_cosmic_map_modifier (AtspiDevice *device, gint keycode)
{
  AtspiDeviceCosmic *libei_device = ATSPI_DEVICE_LIBEI (device);
  AtspiDeviceCosmicPrivate *priv = atspi_device_cosmic_get_instance_private (libei_device);
  guint ret;
  AtspiLibeiKeyModifier *entry;

  ret = find_virtual_mapping (libei_device, keycode);
  if (ret)
    return ret;

  ret = get_unused_virtual_modifier (libei_device);

  entry = g_new (AtspiLibeiKeyModifier, 1);
  entry->keycode = keycode;
  entry->modifier = ret;
  priv->modifiers = g_slist_append (priv->modifiers, entry);

  return ret;
}

static void
atspi_device_cosmic_unmap_modifier (AtspiDevice *device, gint keycode)
{
  AtspiDeviceCosmic *libei_device = ATSPI_DEVICE_LIBEI (device);
  AtspiDeviceCosmicPrivate *priv = atspi_device_cosmic_get_instance_private (libei_device);

  GSList *l;

  for (l = priv->modifiers; l; l = l->next)
    {
      AtspiLibeiKeyModifier *entry = l->data;
      if (entry->keycode == keycode)
        {
          priv->modifiers = g_slist_remove (priv->modifiers, entry);
          g_free (entry);
          return;
        }
    }
}

static guint
atspi_device_cosmic_get_modifier (AtspiDevice *device, gint keycode)
{
  AtspiDeviceCosmic *libei_device = ATSPI_DEVICE_LIBEI (device);

  return find_virtual_mapping (libei_device, keycode);
}

static guint
atspi_device_cosmic_get_locked_modifiers (AtspiDevice *device)
{
  AtspiDeviceCosmic *libei_device = ATSPI_DEVICE_LIBEI (device);
  AtspiDeviceCosmicPrivate *priv = atspi_device_cosmic_get_instance_private (libei_device);

  if (priv->xkb_state)
    return xkb_state_serialize_mods (priv->xkb_state, XKB_STATE_MODS_LOCKED);
  else
    return 0;
}

static void
atspi_device_cosmic_generate_mouse_event (AtspiDevice *device, AtspiAccessible *obj, gint x, gint y, const gchar *name, GError **error)
{
  AtspiPoint *p;

  p = atspi_component_get_position (ATSPI_COMPONENT (obj), ATSPI_COORD_TYPE_SCREEN, error);
  if (p->y == -1 && atspi_accessible_get_role (obj, NULL) == ATSPI_ROLE_APPLICATION)
    {
      g_clear_error (error);
      g_free (p);
      AtspiAccessible *child = atspi_accessible_get_child_at_index (obj, 0, NULL);
      if (child)
        {
          p = atspi_component_get_position (ATSPI_COMPONENT (child), ATSPI_COORD_TYPE_SCREEN, error);
          g_object_unref (child);
        }
    }

  if (p->y == -1 || p->x == -1)
    return;

  x += p->x;
  y += p->y;
  g_free (p);

  /* TODO: do this in process */
  atspi_generate_mouse_event (x, y, name, error);
}

static void
atspi_device_cosmic_finalize (GObject *object)
{
  AtspiDeviceCosmic *device = ATSPI_DEVICE_LIBEI (object);
  AtspiDeviceCosmicPrivate *priv = atspi_device_cosmic_get_instance_private (device);

  if (priv->xkb_state)
    xkb_state_unref (priv->xkb_state);
  if (priv->xkb_keymap)
    xkb_keymap_unref (priv->xkb_keymap);
  if (priv->xkb_context)
    xkb_context_unref (priv->xkb_context);

  g_slist_free_full (priv->modifiers, g_free);
  priv->modifiers = NULL;

  if (priv->ei_source_id)
    g_source_remove (priv->ei_source_id);
  if (priv->ei)
    ei_unref(priv->ei);

  if (priv->wayland_source_id)
    g_source_remove (priv->wayland_source_id);

  if (priv->wl_event_queue)
    wl_event_queue_destroy (priv->wl_event_queue);
  if (priv->wl_display)
    wl_display_disconnect (priv->wl_display);

  // TODO xkb, parent class

  device_cosmic_parent_class->finalize (object);
}

void cosmic_atspi_handle_key_events_eis(void *data,
		       struct cosmic_atspi_manager_v1 *cosmic_atspi_manager_v1,
		       int32_t fd) {
  AtspiDeviceCosmic *device = data;
  AtspiDeviceCosmicPrivate *priv = atspi_device_cosmic_get_instance_private (device);

  // TODO error?
  priv->ei = ei_new_receiver (NULL);
  ei_setup_backend_fd (priv->ei, fd);
  priv->ei_source_id = g_unix_fd_add (ei_get_fd (priv->ei), G_IO_IN, dispatch_libei, device);
}

static const struct cosmic_atspi_manager_v1_listener cosmic_atspi_listener = {
	cosmic_atspi_handle_key_events_eis
};

static void
registry_handle_global(void *data, struct wl_registry *registry,
                       uint32_t name, const char *interface, uint32_t version) {
  AtspiDeviceCosmic *device = data;
  AtspiDeviceCosmicPrivate *priv = atspi_device_cosmic_get_instance_private (device);

  if (strcmp(interface, "cosmic_atspi_manager_v1") == 0) {
    priv->atspi_manager = wl_registry_bind(registry, name, &cosmic_atspi_manager_v1_interface, 1);
    cosmic_atspi_manager_v1_add_listener(priv->atspi_manager, &cosmic_atspi_listener, data);
  }
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
                              uint32_t name)
{ }

static const struct wl_registry_listener registry_listener = {
   registry_handle_global,
   registry_handle_global_remove
};

static void
atspi_device_cosmic_init (AtspiDeviceCosmic *device)
{
  AtspiDeviceCosmicPrivate *priv = atspi_device_cosmic_get_instance_private (device);

  priv->wl_display = wl_display_connect(NULL);
  // TODO error
  priv->wl_event_queue = wl_display_create_queue_with_name (priv->wl_display, "atspi display queue");
  struct wl_registry *wl_registry = wl_display_get_registry(priv->wl_display);
  wl_proxy_set_queue ((struct wl_proxy *)wl_registry, priv->wl_event_queue);
  wl_registry_add_listener(wl_registry, &registry_listener, device);
  // Roundtrip to bind global
  wl_display_roundtrip_queue(priv->wl_display, priv->wl_event_queue);
  // TODO test that global was bound

  priv->wayland_source_id = g_unix_fd_add(wl_display_get_fd(priv->wl_display), G_IO_IN, dispatch_wayland, device);

  priv->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
}

static void
atspi_device_cosmic_class_init (AtspiDeviceCosmicClass *klass)
{
  AtspiDeviceClass *device_class = ATSPI_DEVICE_CLASS (klass);
  GObjectClass *object_class = (GObjectClass *) klass;

  device_cosmic_parent_class = g_type_class_peek_parent (klass);
  device_class->add_key_grab = atspi_device_cosmic_add_key_grab;
  device_class->remove_key_grab = atspi_device_cosmic_remove_key_grab;
  device_class->grab_keyboard = atspi_device_cosmic_grab_keyboard;
  device_class->ungrab_keyboard = atspi_device_cosmic_ungrab_keyboard;
  device_class->map_modifier = atspi_device_cosmic_map_modifier;
  device_class->unmap_modifier = atspi_device_cosmic_unmap_modifier;
  device_class->get_modifier = atspi_device_cosmic_get_modifier;
  device_class->get_locked_modifiers = atspi_device_cosmic_get_locked_modifiers;
  device_class->generate_mouse_event = atspi_device_cosmic_generate_mouse_event;
  object_class->finalize = atspi_device_cosmic_finalize;
}

AtspiDeviceCosmic *
atspi_device_cosmic_new ()
{
  AtspiDeviceCosmic *device = g_object_new (atspi_device_cosmic_get_type (), NULL);

  return device;
}
