#include "gtkentryaccel.h"
#include <glib/gi18n.h>

#define GTK_ENTRY_ACCEL_MODIFIER_MASK (GDK_MODIFIER_MASK & \
                                       ~GDK_LOCK_MASK & \
                                       ~GDK_MOD2_MASK & \
                                       ~GDK_MOD3_MASK & \
                                       ~GDK_MOD4_MASK & \
                                       ~GDK_MOD5_MASK & \
                                       ~GDK_HYPER_MASK)

#define GTK_TYPE_ENTRY_ACCEL_POST_ACTION (gtk_entry_accel_post_action_get_type ())
#define GTK_ENTRY_ACCEL_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_ENTRY_ACCEL, GtkEntryAccelPrivate))

struct _GtkEntryAccelPrivate
{
  gchar *accel;

  guint key;
  guint code;
  GdkModifierType mask;

  GdkDevice *keyboard;
  GdkDevice *pointer;

  gboolean left_shift : 1;
  gboolean right_shift : 1;
  gboolean left_control : 1;
  gboolean right_control : 1;
  gboolean left_alt : 1;
  gboolean right_alt : 1;
  gboolean left_super : 1;
  gboolean right_super : 1;
};

G_DEFINE_TYPE (GtkEntryAccel, gtk_entry_accel, GTK_TYPE_ENTRY);

enum
{
  PROP_0,
  PROP_ACCEL,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL };

enum
{
  SIGNAL_KEY_PRESSED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

static GType
gtk_entry_accel_post_action_get_type (void)
{
  static GType type = 0;

  if (G_UNLIKELY (type == 0))
    {
      static const GEnumValue values[] = {
        { GTK_ENTRY_ACCEL_UPDATE, "GTK_ENTRY_ACCEL_UPDATE", "update" },
        { GTK_ENTRY_ACCEL_CANCEL, "GTK_ENTRY_ACCEL_CANCEL", "cancel" },
        { GTK_ENTRY_ACCEL_IGNORE, "GTK_ENTRY_ACCEL_IGNORE", "ignore" },
        { GTK_ENTRY_ACCEL_PASS_THROUGH, "GTK_ENTRY_ACCEL_PASS_THROUGH", "pass-through" },
        { 0, NULL, NULL }
      };

      type = g_enum_register_static (g_intern_static_string ("GtkEntryAccelPostAction"), values);
    }

  return type;
}

static void
gtk_entry_accel_reset_modifier_states (GtkEntryAccel *entry)
{
  g_return_if_fail (GTK_IS_ENTRY_ACCEL (entry));

  entry->priv->left_shift = FALSE;
  entry->priv->right_shift = FALSE;
  entry->priv->left_control = FALSE;
  entry->priv->right_control = FALSE;
  entry->priv->left_alt = FALSE;
  entry->priv->right_alt = FALSE;
  entry->priv->left_super = FALSE;
  entry->priv->right_super = FALSE;
}

static gboolean
gtk_entry_accel_get_modifier_state (GtkEntryAccel *entry,
                                    guint          key)
{
  g_return_val_if_fail (GTK_IS_ENTRY_ACCEL (entry), FALSE);

  switch (key)
    {
    case GDK_KEY_Shift_L:
      return entry->priv->left_shift;
    case GDK_KEY_Shift_R:
      return entry->priv->right_shift;
    case GDK_KEY_Control_L:
      return entry->priv->left_control;
    case GDK_KEY_Control_R:
      return entry->priv->right_control;
    case GDK_KEY_Meta_L:
    case GDK_KEY_Alt_L:
      return entry->priv->left_alt;
    case GDK_KEY_Meta_R:
    case GDK_KEY_Alt_R:
      return entry->priv->right_alt;
    case GDK_KEY_Super_L:
      return entry->priv->left_super;
    case GDK_KEY_Super_R:
      return entry->priv->right_super;
    }

  return FALSE;
}

static void
gtk_entry_accel_set_modifier_state (GtkEntryAccel *entry,
                                    guint          key,
                                    gboolean       state)
{
  g_return_if_fail (GTK_IS_ENTRY_ACCEL (entry));

  switch (key)
    {
    case GDK_KEY_Shift_L:
      entry->priv->left_shift = state;
      break;
    case GDK_KEY_Shift_R:
      entry->priv->right_shift = state;
      break;
    case GDK_KEY_Control_L:
      entry->priv->left_control = state;
      break;
    case GDK_KEY_Control_R:
      entry->priv->right_control = state;
      break;
    case GDK_KEY_Meta_L:
    case GDK_KEY_Alt_L:
      entry->priv->left_alt = state;
      break;
    case GDK_KEY_Meta_R:
    case GDK_KEY_Alt_R:
      entry->priv->right_alt = state;
      break;
    case GDK_KEY_Super_L:
      entry->priv->left_super = state;
      break;
    case GDK_KEY_Super_R:
      entry->priv->right_super = state;
      break;
    }
}

static void
gtk_entry_accel_update_text (GtkEntryAccel *entry)
{
  if (entry->priv->keyboard == NULL || entry->priv->pointer == NULL)
    {
      if (entry->priv->key != 0 || entry->priv->code != 0 || entry->priv->mask != 0)
        {
          gchar *label = gtk_accelerator_get_label_with_keycode (NULL,
                                                                 entry->priv->key,
                                                                 entry->priv->code,
                                                                 entry->priv->mask);

          gtk_entry_set_text (GTK_ENTRY (entry), label);

          g_free (label);
        }
      else
        gtk_entry_set_text (GTK_ENTRY (entry), "");
    }
  else
    gtk_entry_set_text (GTK_ENTRY (entry), _("New accelerator…"));
}

static void
gtk_entry_accel_set_key (GtkEntryAccel   *entry,
                         guint            key,
                         guint            code,
                         GdkModifierType  mask)
{
  if (key != entry->priv->key || code != entry->priv->code || mask != entry->priv->mask)
    {
      entry->priv->key = key;
      entry->priv->code = code;
      entry->priv->mask = mask;

      g_free (entry->priv->accel);

      if (key != 0 || code != 0 || mask != 0)
        entry->priv->accel = gtk_accelerator_name_with_keycode (NULL, key, code, mask);
      else
        entry->priv->accel = NULL;

      g_object_notify_by_pspec (G_OBJECT (entry), properties[PROP_ACCEL]);
    }

  gtk_entry_accel_update_text (entry);
}

static void
gtk_entry_accel_ungrab_input (GtkEntryAccel *entry,
                              GdkEvent      *event)
{
  guint32 time = gdk_event_get_time (event);

  if (entry->priv->keyboard != NULL && entry->priv->pointer != NULL)
    gtk_grab_remove (GTK_WIDGET (entry));

  if (entry->priv->keyboard != NULL)
    {
      gdk_device_ungrab (entry->priv->keyboard, time);
      g_clear_object (&entry->priv->keyboard);
    }

  if (entry->priv->pointer != NULL)
    {
      gdk_device_ungrab (entry->priv->pointer, time);
      g_clear_object (&entry->priv->pointer);
    }

  gtk_entry_accel_reset_modifier_states (entry);
  gtk_entry_accel_update_text (entry);
}

static void
gtk_entry_accel_grab_input (GtkEntryAccel *entry,
                            GdkEvent      *event)
{
  GdkWindow *window = NULL;
  GdkDevice *device = NULL;
  GdkDevice *keyboard = NULL;
  GdkDevice *pointer = NULL;
  guint32 time;

  if (entry->priv->keyboard != NULL && entry->priv->pointer != NULL)
    return;

  gtk_entry_accel_ungrab_input (entry, event);

  if (event != NULL)
    device = gdk_event_get_device (event);

  if (device == NULL)
    device = gtk_get_current_event_device ();

  if (device == NULL)
    return;

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      keyboard = device;
      pointer = gdk_device_get_associated_device (device);
    }
  else
    {
      pointer = device;
      keyboard = gdk_device_get_associated_device (device);
    }

  if (gdk_device_get_source (keyboard) != GDK_SOURCE_KEYBOARD)
    return;

  window = gtk_widget_get_window (GTK_WIDGET (entry));
  time = gdk_event_get_time (event);

  if (gdk_device_grab (keyboard,
                       window,
                       GDK_OWNERSHIP_WINDOW,
                       FALSE,
                       GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK,
                       NULL,
                       time) != GDK_GRAB_SUCCESS)
    return;

  if (gdk_device_grab (pointer,
                       window,
                       GDK_OWNERSHIP_WINDOW,
                       FALSE,
                       GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK,
                       NULL,
                       time) != GDK_GRAB_SUCCESS)
    {
      gdk_device_ungrab (keyboard, time);

      return;
    }

  gtk_grab_add (GTK_WIDGET (entry));

  entry->priv->keyboard = g_object_ref (keyboard);
  entry->priv->pointer = g_object_ref (pointer);
}

static void
gtk_entry_accel_dispose (GObject *object)
{
  GtkEntryAccel *entry = GTK_ENTRY_ACCEL (object);

  gtk_entry_accel_ungrab_input (entry, NULL);

  G_OBJECT_CLASS (gtk_entry_accel_parent_class)->dispose (object);
}

static void
gtk_entry_accel_finalize (GObject *object)
{
  GtkEntryAccel *entry = GTK_ENTRY_ACCEL (object);

  g_free (entry->priv->accel);

  G_OBJECT_CLASS (gtk_entry_accel_parent_class)->finalize (object);
}

static void
gtk_entry_accel_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkEntryAccel *entry = GTK_ENTRY_ACCEL (object);
  const gchar *accel;

  switch (property_id)
    {
    case PROP_ACCEL:
      accel = gtk_entry_accel_get_accel (entry);
      g_value_set_string (value, accel != NULL ? accel : "");
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_entry_accel_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkEntryAccel *entry = GTK_ENTRY_ACCEL (object);

  switch (property_id)
    {
    case PROP_ACCEL:
      gtk_entry_accel_set_accel (entry, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static gboolean
gtk_entry_accel_button_press_event (GtkWidget      *widget,
                                    GdkEventButton *event)
{
  if (event->button == 1)
    {
      GtkEntryAccel *entry = GTK_ENTRY_ACCEL (widget);

      if (entry->priv->keyboard != NULL && entry->priv->pointer != NULL)
        gtk_entry_accel_ungrab_input (entry, (GdkEvent *) event);
      else
        gtk_entry_accel_grab_input (entry, (GdkEvent *) event);

      gtk_entry_accel_update_text (entry);
    }

  return TRUE;
}

static gboolean
gtk_entry_accel_post_action_accumulator (GSignalInvocationHint *ihint,
                                         GValue                *return_accu,
                                         const GValue          *handler_return,
                                         gpointer               data)
{
  GtkEntryAccelPostAction action = g_value_get_enum (return_accu);
  GtkEntryAccelPostAction current_action = g_value_get_enum (handler_return);

  if (action == GTK_ENTRY_ACCEL_UPDATE)
    action = current_action;

  g_value_set_enum (return_accu, action);

  return action == GTK_ENTRY_ACCEL_UPDATE;
}

static GtkEntryAccelPostAction
gtk_entry_accel_real_key_pressed (GtkEntryAccel   *entry,
                                  guint           *key,
                                  guint           *code,
                                  GdkModifierType *mask)
{
  return GTK_ENTRY_ACCEL_UPDATE;
}

static GtkEntryAccelPostAction
gtk_entry_accel_key_pressed (GtkEntryAccel   *entry,
                             guint           *key,
                             guint           *code,
                             GdkModifierType *mask)
{
  GtkEntryAccelPostAction action;

  g_signal_emit (entry,
                 signals[SIGNAL_KEY_PRESSED],
                 0,
                 key,
                 code,
                 mask,
                 &action);

  return action;
}

static gboolean
gtk_entry_accel_key_press_event (GtkWidget   *widget,
                                 GdkEventKey *event)
{
  GtkEntryAccel *entry = GTK_ENTRY_ACCEL (widget);
  guint key = event->keyval;
  guint mask = event->state & GTK_ENTRY_ACCEL_MODIFIER_MASK;
  gboolean grabbed = entry->priv->keyboard != NULL && entry->priv->pointer != NULL;

  gtk_entry_accel_set_modifier_state (entry, key, TRUE);

  return ((grabbed ? mask : (mask & ~GDK_SHIFT_MASK)) != 0 ||
          (key != GDK_KEY_Tab &&
           key != GDK_KEY_KP_Tab &&
           key != GDK_KEY_ISO_Left_Tab &&
           key != GDK_KEY_3270_BackTab) ||
          GTK_WIDGET_CLASS (gtk_entry_accel_parent_class)->key_press_event (widget, event));
}

static guint
gtk_entry_accel_get_mask_for_key (guint key)
{
  switch (key)
    {
    case GDK_KEY_Shift_L:
    case GDK_KEY_Shift_R:
      return GDK_SHIFT_MASK;
    case GDK_KEY_Control_L:
    case GDK_KEY_Control_R:
      return GDK_CONTROL_MASK;
    case GDK_KEY_Caps_Lock:
    case GDK_KEY_Shift_Lock:
      return GDK_LOCK_MASK;
    case GDK_KEY_Meta_L:
    case GDK_KEY_Meta_R:
      return GDK_META_MASK;
    case GDK_KEY_Alt_L:
    case GDK_KEY_Alt_R:
      return GDK_MOD1_MASK;
    case GDK_KEY_Super_L:
    case GDK_KEY_Super_R:
      return GDK_SUPER_MASK;
    case GDK_KEY_Hyper_L:
    case GDK_KEY_Hyper_R:
      return GDK_HYPER_MASK;
    }

  return 0;
}

static guint
gtk_entry_accel_get_mirrored_key (guint key)
{
  switch (key)
    {
    case GDK_KEY_Shift_L:
      return GDK_KEY_Shift_R;
    case GDK_KEY_Shift_R:
      return GDK_KEY_Shift_L;
    case GDK_KEY_Control_L:
      return GDK_KEY_Control_R;
    case GDK_KEY_Control_R:
      return GDK_KEY_Control_L;
    case GDK_KEY_Meta_L:
      return GDK_KEY_Meta_R;
    case GDK_KEY_Meta_R:
      return GDK_KEY_Meta_L;
    case GDK_KEY_Alt_L:
      return GDK_KEY_Alt_R;
    case GDK_KEY_Alt_R:
      return GDK_KEY_Alt_L;
    case GDK_KEY_Super_L:
      return GDK_KEY_Super_R;
    case GDK_KEY_Super_R:
      return GDK_KEY_Super_L;
    case GDK_KEY_Hyper_L:
      return GDK_KEY_Hyper_R;
    }

  return 0;
}

static gboolean
gtk_entry_accel_key_release_event (GtkWidget   *widget,
                                   GdkEventKey *event)
{
  GtkEntryAccel *entry = GTK_ENTRY_ACCEL (widget);
  guint key = event->keyval;
  guint code = event->hardware_keycode;
  guint mask = event->state & GTK_ENTRY_ACCEL_MODIFIER_MASK;

  if (entry->priv->keyboard != NULL && entry->priv->pointer != NULL)
    {
      switch (key)
        {
        case GDK_KEY_Meta_L:
          key = GDK_KEY_Alt_L;
          break;

        case GDK_KEY_Meta_R:
          key = GDK_KEY_Alt_R;
          break;
        }

      if (event->is_modifier && !gtk_entry_accel_get_modifier_state (entry, gtk_entry_accel_get_mirrored_key (key)))
        mask &= ~gtk_entry_accel_get_mask_for_key (key);

      gtk_entry_accel_ungrab_input (entry, (GdkEvent *) event);

      switch (gtk_entry_accel_key_pressed (entry, &key, &code, &mask))
        {
        case GTK_ENTRY_ACCEL_UPDATE:
          gtk_entry_accel_set_key (entry, key, code, mask);
        case GTK_ENTRY_ACCEL_CANCEL:
          gtk_entry_accel_ungrab_input (entry, (GdkEvent *) event);
        case GTK_ENTRY_ACCEL_IGNORE:
          return TRUE;
        }

      event->keyval = key;
      event->hardware_keycode = code;
      event->state = mask;

      gtk_entry_accel_ungrab_input (entry, (GdkEvent *) event);

      return GTK_WIDGET_CLASS (gtk_entry_accel_parent_class)->key_release_event (widget, event);
    }

  if (mask == 0 &&
      (key == GDK_KEY_Return ||
       key == GDK_KEY_KP_Enter ||
       key == GDK_KEY_ISO_Enter ||
       key == GDK_KEY_3270_Enter))
    {
      gtk_entry_accel_grab_input (entry, (GdkEvent *) event);
      gtk_entry_accel_update_text (entry);

      return TRUE;
    }

  return ((mask & ~GDK_SHIFT_MASK) != 0 ||
          (key != GDK_KEY_Tab &&
           key != GDK_KEY_KP_Tab &&
           key != GDK_KEY_ISO_Left_Tab &&
           key != GDK_KEY_3270_BackTab) ||
          GTK_WIDGET_CLASS (gtk_entry_accel_parent_class)->key_release_event (widget, event));
}

static void
gtk_entry_accel_class_init (GtkEntryAccelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gtk_entry_accel_dispose;
  object_class->finalize = gtk_entry_accel_finalize;
  object_class->get_property = gtk_entry_accel_get_property;
  object_class->set_property = gtk_entry_accel_set_property;
  widget_class->button_press_event = gtk_entry_accel_button_press_event;
  widget_class->key_press_event = gtk_entry_accel_key_press_event;
  widget_class->key_release_event = gtk_entry_accel_key_release_event;
  klass->key_pressed = gtk_entry_accel_real_key_pressed;

  properties[PROP_ACCEL] = g_param_spec_string ("accel",
                                                "Accelerator",
                                                "Current accelerator",
                                                NULL,
                                                G_PARAM_READWRITE);

  g_object_class_install_property (object_class,
                                   PROP_ACCEL,
                                   properties[PROP_ACCEL]);

  signals[SIGNAL_KEY_PRESSED] = g_signal_new ("key-pressed",
                                              G_OBJECT_CLASS_TYPE (klass),
                                              G_SIGNAL_RUN_LAST,
                                              G_STRUCT_OFFSET (GtkEntryAccelClass, key_pressed),
                                              gtk_entry_accel_post_action_accumulator,
                                              NULL,
                                              NULL,
                                              GTK_TYPE_ENTRY_ACCEL_POST_ACTION,
                                              3,
                                              G_TYPE_POINTER,
                                              G_TYPE_POINTER,
                                              G_TYPE_POINTER);

  g_type_class_add_private (klass, sizeof (GtkEntryAccelPrivate));
}

static void
gtk_entry_accel_init (GtkEntryAccel *self)
{
  self->priv = GTK_ENTRY_ACCEL_GET_PRIVATE (self);
}

GtkWidget *
gtk_entry_accel_new (void)
{
  return g_object_new (GTK_TYPE_ENTRY_ACCEL, NULL);
}

const gchar *
gtk_entry_accel_get_accel (GtkEntryAccel *entry)
{
  g_return_val_if_fail (GTK_IS_ENTRY_ACCEL (entry), NULL);

  return entry->priv->accel;
}

void
gtk_entry_accel_set_accel (GtkEntryAccel *entry,
                           const gchar   *accel)
{
  guint key = 0;
  guint *codes = NULL;
  GdkModifierType mask = 0;

  g_return_if_fail (GTK_IS_ENTRY_ACCEL (entry));

  if (accel != NULL)
    gtk_accelerator_parse_with_keycode (accel, &key, &codes, &mask);

  gtk_entry_accel_set_key (entry, key, codes != NULL ? codes[0] : 0, mask);

  g_free (codes);
}
