#ifndef __GTK_ENTRY_ACCEL_H__
#define __GTK_ENTRY_ACCEL_H__

#include <gtk/gtk.h>

#define GTK_TYPE_ENTRY_ACCEL            (gtk_entry_accel_get_type ())
#define GTK_ENTRY_ACCEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_ENTRY_ACCEL, GtkEntryAccel))
#define GTK_IS_ENTRY_ACCEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_ENTRY_ACCEL))
#define GTK_ENTRY_ACCEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_ENTRY_ACCEL, GtkEntryAccelClass))
#define GTK_IS_ENTRY_ACCEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_ENTRY_ACCEL))
#define GTK_ENTRY_ACCEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_ENTRY_ACCEL, GtkEntryAccelClass))

typedef struct _GtkEntryAccel           GtkEntryAccel;
typedef struct _GtkEntryAccelClass      GtkEntryAccelClass;
typedef struct _GtkEntryAccelPrivate    GtkEntryAccelPrivate;
typedef enum   _GtkEntryAccelPostAction GtkEntryAccelPostAction;

struct _GtkEntryAccel
{
  GtkEntry parent_instance;

  /*< private >*/
  GtkEntryAccelPrivate *priv;
};

struct _GtkEntryAccelClass
{
  GtkEntryClass parent_class;

  /*< public >*/
  GtkEntryAccelPostAction (* key_pressed) (GtkEntryAccel   *entry,
                                           guint           *key,
                                           guint           *code,
                                           GdkModifierType *mask);
};

enum _GtkEntryAccelPostAction
{
  GTK_ENTRY_ACCEL_UPDATE,
  GTK_ENTRY_ACCEL_CANCEL,
  GTK_ENTRY_ACCEL_IGNORE,
  GTK_ENTRY_ACCEL_PASS_THROUGH
};

GType         gtk_entry_accel_get_type  (void);

GtkWidget *   gtk_entry_accel_new       (void);

const gchar * gtk_entry_accel_get_accel (GtkEntryAccel *entry);

void          gtk_entry_accel_set_accel (GtkEntryAccel *entry,
                                         const gchar   *accel);

#endif /* __GTK_ENTRY_ACCEL_H__ */
