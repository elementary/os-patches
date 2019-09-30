/*
 * Copyright (C) 2010 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of either or both of the following licenses:
 *
 * 1) the GNU Lesser General Public License version 3, as published by the
 * Free Software Foundation; and/or
 * 2) the GNU Lesser General Public License version 2.1, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the applicable version of the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of both the GNU Lesser General Public
 * License version 3 and version 2.1 along with this program.  If not, see
 * <http://www.gnu.org/licenses/>
 *
 * Authors:
 *    Cody Russell <crussell@canonical.com>
 *
 * Design and specification:
 *    Matthew Paul Thomas <mpt@canonical.com>
 */

#ifndef __IDO_MESSAGE_DIALOG_H__
#define __IDO_MESSAGE_DIALOG_H__

#include <gtk/gtk.h>

#define IDO_TYPE_MESSAGE_DIALOG         (ido_message_dialog_get_type ())
#define IDO_MESSAGE_DIALOG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), IDO_TYPE_MESSAGE_DIALOG, IdoMessageDialog))
#define IDO_MESSAGE_DIALOG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), IDO_TYPE_MESSAGE_DIALOG, IdoMessageDialogClass))
#define IDO_IS_MESSAGE_DIALOG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), IDO_TYPE_MESSAGE_DIALOG))
#define IDO_IS_MESSAGE_DIALOG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), IDO_TYPE_MESSAGE_DIALOG))
#define IDO_MESSAGE_DIALOG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), IDO_TYPE_MESSAGE_DIALOG, IdoMessageDialogClass))

typedef struct _IdoMessageDialog        IdoMessageDialog;
typedef struct _IdoMessageDialogClass   IdoMessageDialogClass;

struct _IdoMessageDialog
{
  GtkMessageDialog parent_instance;
};

struct _IdoMessageDialogClass
{
  GtkMessageDialogClass parent_class;

  /* Padding for future expansion */
  void (*_ido_reserved1) (void);
  void (*_ido_reserved2) (void);
  void (*_ido_reserved3) (void);
  void (*_ido_reserved4) (void);
};

GType      ido_message_dialog_get_type (void) G_GNUC_CONST;

GtkWidget* ido_message_dialog_new      (GtkWindow      *parent,
                                        GtkDialogFlags  flags,
                                        GtkMessageType  type,
                                        GtkButtonsType  buttons,
                                        const gchar    *message_format,
                                        ...) G_GNUC_PRINTF (5, 6);

GtkWidget* ido_message_dialog_new_with_markup   (GtkWindow      *parent,
                                                 GtkDialogFlags  flags,
                                                 GtkMessageType  type,
                                                 GtkButtonsType  buttons,
                                                 const gchar    *message_format,
                                                 ...) G_GNUC_PRINTF (5, 6);

G_END_DECLS

#endif /* __IDO_MESSAGE_DIALOG_H__ */
