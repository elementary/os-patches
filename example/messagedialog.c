#include <gtk/gtk.h>

#include "idomessagedialog.h"
#include "config.h"

static void
response_cb (GtkDialog *dialog,
             gint       response,
             gpointer   user_data)
{
  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
button_clicked_cb (GtkWidget *button, gpointer data)
{
  GtkWidget *window = (GtkWidget *) data;
  GtkWidget *dialog = ido_message_dialog_new (GTK_WINDOW (window),
                                              GTK_DIALOG_MODAL,
                                              GTK_MESSAGE_WARNING,
                                              GTK_BUTTONS_CLOSE,
                                              "This is a test of the emergency broadcasting system");
  gtk_message_dialog_format_secondary_markup (GTK_MESSAGE_DIALOG (dialog),
                                              "If this had been an actual emergency, you'd be dead already");
  g_signal_connect (G_OBJECT (dialog),
                    "response",
                    G_CALLBACK (response_cb), NULL);
  gtk_widget_show (dialog);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *button;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Message Dialogs");
  g_signal_connect (window, "destroy", gtk_main_quit, NULL);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  gtk_container_add (GTK_CONTAINER (window), vbox);

  button = gtk_button_new_with_label ("Confirmation dialog");
  g_signal_connect (button, "clicked",
                    G_CALLBACK (button_clicked_cb),
                    window);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
