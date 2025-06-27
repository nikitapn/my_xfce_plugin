/*  $Id$
 *
 *  Copyright (C) 2019 John Doo <john@foo.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_XFCE_REVISION_H
#include "xfce-revision.h"
#endif

#include <string.h>
#include <gtk/gtk.h>

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4panel/libxfce4panel.h>

#include "sample.h"
#include "sample-dialogs.h"

/* the website url */
#define PLUGIN_WEBSITE "https://docs.xfce.org/panel-plugins/xfce4-sample-plugin"



static void
sample_configure_response (GtkWidget    *dialog,
                           gint          response,
                           SamplePlugin *sample)
{
  gboolean result;

  if (response == GTK_RESPONSE_HELP)
    {
      /* show help */
#if LIBXFCE4UI_CHECK_VERSION(4, 21, 0)
      result = g_spawn_command_line_async ("xfce-open --launch WebBrowser " PLUGIN_WEBSITE, NULL);
#else
      result = g_spawn_command_line_async ("exo-open --launch WebBrowser " PLUGIN_WEBSITE, NULL);
#endif
      if (G_UNLIKELY (result == FALSE))
        g_warning (_("Unable to open the following url: %s"), PLUGIN_WEBSITE);
    }
  else
    {
      /* Get widget pointers */
      GtkWidget *weather_location_entry = g_object_get_data(G_OBJECT(dialog), "weather_location_entry");
      GtkWidget *exchange_api_key_entry = g_object_get_data(G_OBJECT(dialog), "exchange_api_key_entry");
      GtkWidget *show_weather_check = g_object_get_data(G_OBJECT(dialog), "show_weather_check");
      GtkWidget *show_exchange_check = g_object_get_data(G_OBJECT(dialog), "show_exchange_check");
      GtkWidget *show_battery_check = g_object_get_data(G_OBJECT(dialog), "show_battery_check");
      GtkWidget *show_memory_check = g_object_get_data(G_OBJECT(dialog), "show_memory_check");
      GtkWidget *show_date_check = g_object_get_data(G_OBJECT(dialog), "show_date_check");

      /* Update settings */
      g_free(sample->weather_location);
      sample->weather_location = g_strdup(gtk_entry_get_text(GTK_ENTRY(weather_location_entry)));
      
      g_free(sample->exchange_api_key);
      sample->exchange_api_key = g_strdup(gtk_entry_get_text(GTK_ENTRY(exchange_api_key_entry)));
      
      sample->show_weather = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(show_weather_check));
      sample->show_exchange = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(show_exchange_check));
      sample->show_battery = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(show_battery_check));
      sample->show_memory = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(show_memory_check));
      sample->show_date = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(show_date_check));

      /* remove the dialog data from the plugin */
      g_object_set_data (G_OBJECT (sample->plugin), "dialog", NULL);

      /* unlock the panel menu */
      xfce_panel_plugin_unblock_menu (sample->plugin);

      /* save the plugin */
      sample_save (sample->plugin, sample);

      /* destroy the properties dialog */
      gtk_widget_destroy (dialog);
    }
}



void
sample_configure (XfcePanelPlugin *plugin,
                  SamplePlugin    *sample)
{
  GtkWidget *dialog;
  GtkWidget *grid;
  GtkWidget *label;
  GtkWidget *weather_location_entry;
  GtkWidget *exchange_api_key_entry;
  GtkWidget *show_weather_check;
  GtkWidget *show_exchange_check;
  GtkWidget *show_battery_check;
  GtkWidget *show_memory_check;
  GtkWidget *show_date_check;
  int row = 0;

  /* block the plugin menu */
  xfce_panel_plugin_block_menu (plugin);

  /* create the dialog */
  dialog = xfce_titled_dialog_new_with_mixed_buttons (_("Status Bar Plugin"),
                                                      GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin))),
                                                      GTK_DIALOG_DESTROY_WITH_PARENT,
                                                      "help-browser-symbolic", _("_Help"), GTK_RESPONSE_HELP,
                                                      "window-close-symbolic", _("_Close"), GTK_RESPONSE_OK,
                                                      NULL);

  /* center dialog on the screen */
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);

  /* set dialog icon */
  gtk_window_set_icon_name (GTK_WINDOW (dialog), "xfce4-settings");

  /* Create main grid */
  grid = gtk_grid_new();
  gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
  gtk_widget_set_margin_start(grid, 12);
  gtk_widget_set_margin_end(grid, 12);
  gtk_widget_set_margin_top(grid, 12);
  gtk_widget_set_margin_bottom(grid, 12);

  /* Weather location setting */
  label = gtk_label_new(_("Weather Location (lat,long):"));
  gtk_label_set_xalign(GTK_LABEL(label), 0.0);
  gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
  
  weather_location_entry = gtk_entry_new();
  if (sample->weather_location) {
    gtk_entry_set_text(GTK_ENTRY(weather_location_entry), sample->weather_location);
  }
  gtk_entry_set_placeholder_text(GTK_ENTRY(weather_location_entry), "e.g., 37.7749,-122.4194");
  gtk_grid_attach(GTK_GRID(grid), weather_location_entry, 1, row, 1, 1);
  row++;

  /* Exchange API key setting */
  label = gtk_label_new(_("Exchange API Key:"));
  gtk_label_set_xalign(GTK_LABEL(label), 0.0);
  gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
  
  exchange_api_key_entry = gtk_entry_new();
  if (sample->exchange_api_key) {
    gtk_entry_set_text(GTK_ENTRY(exchange_api_key_entry), sample->exchange_api_key);
  }
  gtk_entry_set_placeholder_text(GTK_ENTRY(exchange_api_key_entry), "OpenExchangeRates API key");
  gtk_entry_set_visibility(GTK_ENTRY(exchange_api_key_entry), FALSE); /* Hide for security */
  gtk_grid_attach(GTK_GRID(grid), exchange_api_key_entry, 1, row, 1, 1);
  row++;

  /* Separator */
  gtk_grid_attach(GTK_GRID(grid), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), 0, row, 2, 1);
  row++;

  /* Show options */
  label = gtk_label_new(_("Display Components:"));
  gtk_label_set_xalign(GTK_LABEL(label), 0.0);
  gtk_widget_set_margin_top(label, 6);
  gtk_grid_attach(GTK_GRID(grid), label, 0, row, 2, 1);
  row++;

  show_weather_check = gtk_check_button_new_with_label(_("Show Weather"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(show_weather_check), sample->show_weather);
  gtk_grid_attach(GTK_GRID(grid), show_weather_check, 0, row, 2, 1);
  row++;

  show_exchange_check = gtk_check_button_new_with_label(_("Show Exchange Rates"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(show_exchange_check), sample->show_exchange);
  gtk_grid_attach(GTK_GRID(grid), show_exchange_check, 0, row, 2, 1);
  row++;

  show_battery_check = gtk_check_button_new_with_label(_("Show Battery"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(show_battery_check), sample->show_battery);
  gtk_grid_attach(GTK_GRID(grid), show_battery_check, 0, row, 2, 1);
  row++;

  show_memory_check = gtk_check_button_new_with_label(_("Show Memory Usage"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(show_memory_check), sample->show_memory);
  gtk_grid_attach(GTK_GRID(grid), show_memory_check, 0, row, 2, 1);
  row++;

  show_date_check = gtk_check_button_new_with_label(_("Show Date/Time"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(show_date_check), sample->show_date);
  gtk_grid_attach(GTK_GRID(grid), show_date_check, 0, row, 2, 1);
  row++;

  /* Add grid to dialog */
  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), grid, TRUE, TRUE, 0);
  gtk_widget_show_all(grid);

  /* Store widget pointers for response handler */
  g_object_set_data(G_OBJECT(dialog), "weather_location_entry", weather_location_entry);
  g_object_set_data(G_OBJECT(dialog), "exchange_api_key_entry", exchange_api_key_entry);
  g_object_set_data(G_OBJECT(dialog), "show_weather_check", show_weather_check);
  g_object_set_data(G_OBJECT(dialog), "show_exchange_check", show_exchange_check);
  g_object_set_data(G_OBJECT(dialog), "show_battery_check", show_battery_check);
  g_object_set_data(G_OBJECT(dialog), "show_memory_check", show_memory_check);
  g_object_set_data(G_OBJECT(dialog), "show_date_check", show_date_check);

  /* link the dialog to the plugin, so we can destroy it when the plugin
   * is closed, but the dialog is still open */
  g_object_set_data (G_OBJECT (plugin), "dialog", dialog);

  /* connect the response signal to the dialog */
  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK(sample_configure_response), sample);

  /* show the entire dialog */
  gtk_widget_show (dialog);
}



void
sample_about (XfcePanelPlugin *plugin)
{
  /* about dialog code. you can use the GtkAboutDialog
   * or the XfceAboutInfo widget */
  const gchar *auth[] =
    {
      "Status Bar Plugin Developer",
      NULL
    };

  gtk_show_about_dialog (NULL,
                         "logo-icon-name", "xfce4-sample-plugin",
                         "license",        xfce_get_license_text (XFCE_LICENSE_TEXT_GPL),
                         "version",        VERSION_FULL,
                         "program-name",   "Status Bar Plugin",
                         "comments",       _("A comprehensive status bar plugin showing weather, exchange rates, battery, memory, and date/time"),
                         "website",        PLUGIN_WEBSITE,
                         "copyright",      "Copyright \xc2\xa9 2025 Status Bar Plugin",
                         "authors",        auth,
                         NULL);
}
