/*
 * Copyright 2010 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * Boston, MA 02111-1307, USA.
 *
 */

#include <mx/mx.h>

static void
small_screen_cb (MxToggle   *toggle,
                 GParamSpec *pspec,
                 MxWindow   *window)
{
  mx_window_set_small_screen (window, mx_toggle_get_active (toggle));
}

static void
fullscreen_cb (MxToggle     *toggle,
               GParamSpec   *pspec,
               ClutterStage *stage)
{
  clutter_stage_set_fullscreen (stage, mx_toggle_get_active (toggle));
}

static void
icon_cb (MxToggle   *toggle,
         GParamSpec *pspec,
         MxWindow   *window)
{
  gboolean use_custom = mx_toggle_get_active (toggle);

  if (use_custom)
    {
      CoglHandle texture = cogl_texture_new_from_file ("redhand.png",
                                                       COGL_TEXTURE_NONE,
                                                       COGL_PIXEL_FORMAT_ANY,
                                                       NULL);
      if (texture)
        mx_window_set_icon_from_cogl_texture (window, texture);
      cogl_handle_unref (texture);
    }
  else
    mx_window_set_icon_name (window, "window-new");
}

static void
toolbar_cb (MxToggle   *toggle,
            GParamSpec *pspec,
            MxWindow   *window)
{
  mx_window_set_has_toolbar (window, mx_toggle_get_active (toggle));
}

static void
resizable_cb (MxToggle     *toggle,
              GParamSpec   *pspec,
              ClutterStage *stage)
{
  clutter_stage_set_user_resizable (stage, mx_toggle_get_active (toggle));
}

static void
close_dialog_cb (MxAction     *action,
                 ClutterActor *dialog)
{
  clutter_actor_hide (dialog);
}

static void
show_dialog_cb (MxButton     *button,
                ClutterActor *dialog)
{
  clutter_actor_show (dialog);
}

static void
rotate_clicked_cb (ClutterActor *button,
                   MxWindow     *window)
{
  MxWindowRotation rotation = mx_window_get_window_rotation (window);

  switch (rotation)
    {
    case MX_WINDOW_ROTATION_0:
      rotation = MX_WINDOW_ROTATION_90;
      break;
    case MX_WINDOW_ROTATION_90:
      rotation = MX_WINDOW_ROTATION_180;
      break;
    case MX_WINDOW_ROTATION_180:
      rotation = MX_WINDOW_ROTATION_270;
      break;
    case MX_WINDOW_ROTATION_270:
      rotation = MX_WINDOW_ROTATION_0;
      break;
    }

  mx_window_set_window_rotation (window, rotation);
}

static void
startup_cb (MxApplication *app)
{
  MxWindow *window;
  ClutterActor *stage, *toggle, *label, *table, *button, *icon, *dialog;

  window = mx_application_create_window (app, "Test Window");
  stage = (ClutterActor *)mx_window_get_clutter_stage (window);
  mx_window_set_icon_name (window, "window-new");

  clutter_actor_set_size (stage, 480, 320);

  table = mx_table_new ();
  mx_table_set_column_spacing (MX_TABLE (table), 8);
  mx_table_set_row_spacing (MX_TABLE (table), 12);
  mx_window_set_child (window, table);

  toggle = mx_toggle_new ();
  label = mx_label_new_with_text ("Toggle small-screen mode");
  g_signal_connect (toggle, "notify::active",
                    G_CALLBACK (small_screen_cb), window);
  mx_table_insert_actor_with_properties (MX_TABLE (table),
                                         toggle,
                                         0, 0,
                                         "x-expand", TRUE,
                                         "x-align", MX_ALIGN_END,
                                         "x-fill", FALSE,
                                         NULL);
  mx_table_insert_actor_with_properties (MX_TABLE (table),
                                         label,
                                         0, 1,
                                         "x-expand", TRUE,
                                         "x-align", MX_ALIGN_START,
                                         "y-fill", FALSE,
                                         "x-fill", FALSE,
                                         NULL);

  toggle = mx_toggle_new ();
  label = mx_label_new_with_text ("Toggle full-screen mode");
  g_signal_connect (toggle, "notify::active",
                    G_CALLBACK (fullscreen_cb), stage);
  mx_table_insert_actor_with_properties (MX_TABLE (table),
                                         toggle,
                                         1, 0,
                                         "x-expand", TRUE,
                                         "x-align", MX_ALIGN_END,
                                         "x-fill", FALSE,
                                         NULL);
  mx_table_insert_actor_with_properties (MX_TABLE (table),
                                         label,
                                         1, 1,
                                         "x-expand", TRUE,
                                         "x-align", MX_ALIGN_START,
                                         "y-fill", FALSE,
                                         "x-fill", FALSE,
                                         NULL);

  toggle = mx_toggle_new ();
  label = mx_label_new_with_text ("Toggle custom window icon");
  g_signal_connect (toggle, "notify::active",
                    G_CALLBACK (icon_cb), window);
  mx_table_insert_actor_with_properties (MX_TABLE (table),
                                         toggle,
                                         2, 0,
                                         "x-expand", TRUE,
                                         "x-align", MX_ALIGN_END,
                                         "x-fill", FALSE,
                                         NULL);
  mx_table_insert_actor_with_properties (MX_TABLE (table),
                                         label,
                                         2, 1,
                                         "x-expand", TRUE,
                                         "x-align", MX_ALIGN_START,
                                         "y-fill", FALSE,
                                         "x-fill", FALSE,
                                         NULL);

  toggle = mx_toggle_new ();
  mx_toggle_set_active (MX_TOGGLE (toggle), TRUE);
  label = mx_label_new_with_text ("Toggle user-resizable");
  g_signal_connect (toggle, "notify::active",
                    G_CALLBACK (resizable_cb), stage);
  mx_table_insert_actor_with_properties (MX_TABLE (table),
                                         toggle,
                                         3, 0,
                                         "x-expand", TRUE,
                                         "x-align", MX_ALIGN_END,
                                         "x-fill", FALSE,
                                         NULL);
  mx_table_insert_actor_with_properties (MX_TABLE (table),
                                         label,
                                         3, 1,
                                         "x-expand", TRUE,
                                         "x-align", MX_ALIGN_START,
                                         "y-fill", FALSE,
                                         "x-fill", FALSE,
                                         NULL);

  toggle = mx_toggle_new ();
  mx_toggle_set_active (MX_TOGGLE (toggle), TRUE);
  label = mx_label_new_with_text ("Toggle toolbar");
  g_signal_connect (toggle, "notify::active",
                    G_CALLBACK (toolbar_cb), window);
  mx_table_insert_actor_with_properties (MX_TABLE (table),
                                         toggle,
                                         4, 0,
                                         "x-expand", TRUE,
                                         "x-align", MX_ALIGN_END,
                                         "x-fill", FALSE,
                                         NULL);
  mx_table_insert_actor_with_properties (MX_TABLE (table),
                                         label,
                                         4, 1,
                                         "x-expand", TRUE,
                                         "x-align", MX_ALIGN_START,
                                         "y-fill", FALSE,
                                         "x-fill", FALSE,
                                         NULL);

  /* dialog test */
  dialog = mx_dialog_new ();
  mx_dialog_set_transient_parent (MX_DIALOG (dialog),
                                  CLUTTER_ACTOR (mx_window_get_clutter_stage (window)));
  mx_dialog_add_action (MX_DIALOG (dialog), mx_action_new_full ("close",
                                                                "Close",
                                                                G_CALLBACK (close_dialog_cb),
                                                                dialog));
  clutter_actor_add_child (dialog, mx_label_new_with_text ("Dialog Window"));

  button = mx_button_new_with_label ("Show Dialog");
  g_signal_connect (button, "clicked", G_CALLBACK (show_dialog_cb), dialog);
  mx_table_insert_actor_with_properties (MX_TABLE (table),
                                         button,
                                         5, 0,
                                         "column-span", 2,
                                         "x-expand", TRUE,
                                         "x-align", MX_ALIGN_MIDDLE,
                                         "y-fill", FALSE,
                                         "x-fill", FALSE,
                                         NULL);


  icon = mx_icon_new ();
  mx_icon_set_icon_name (MX_ICON (icon), "object-rotate-right");
  mx_icon_set_icon_size (MX_ICON (icon), 16);
  button = mx_button_new ();
  clutter_actor_add_child (button, icon);
  g_signal_connect (button, "clicked", G_CALLBACK (rotate_clicked_cb), window);
  clutter_actor_add_child (
    CLUTTER_ACTOR (mx_window_get_toolbar (window)), button);
  clutter_actor_set_x_align (button, CLUTTER_ACTOR_ALIGN_END);
  clutter_actor_set_y_align (button, CLUTTER_ACTOR_ALIGN_CENTER);

  clutter_actor_show (stage);
}

int
main (int argc, char **argv)
{
  MxApplication *app;

  app = mx_application_new ("org.clutter-project.Mx.TestWindow", 0);

  g_signal_connect_after (app, "startup", G_CALLBACK (startup_cb), NULL);

  g_application_run (G_APPLICATION (app), argc, argv);

  return 0;
}
