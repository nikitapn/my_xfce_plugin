/*  Status Bar Plugin for XFCE Panel
 *
 *  Copyright (C) 2025 
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
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/libxfce4panel.h>
#include <pango/pango.h>
#include <curl/curl.h>
#include <libudev.h>
#include <json-glib/json-glib.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

#include "sample.h"
#include "sample-dialogs.h"

/* default settings */
#define DEFAULT_WEATHER_LOCATION NULL
#define DEFAULT_EXCHANGE_API_KEY NULL
#define DEFAULT_UPDATE_INTERVAL 60
#define DEFAULT_SHOW_WEATHER TRUE
#define DEFAULT_SHOW_EXCHANGE TRUE
#define DEFAULT_SHOW_BATTERY TRUE
#define DEFAULT_SHOW_MEMORY TRUE
#define DEFAULT_SHOW_DATE TRUE

/* prototypes */
static void sample_construct (XfcePanelPlugin *plugin);
static gboolean update_display (SamplePlugin *sample);
static void update_block (SamplePlugin *sample, BlockId block_id, const char *text);

/* Thread functions */
static gpointer date_thread_func (gpointer data);
static gpointer memory_thread_func (gpointer data);
static gpointer weather_thread_func (gpointer data);
static gpointer exchange_thread_func (gpointer data);
static gpointer battery_thread_func (gpointer data);

/* Utility functions */
static size_t write_response_callback (void *contents, size_t size, size_t nmemb, void *userp);
static gchar* get_weather_data (const gchar *location);
static gchar* get_exchange_data (const gchar *api_key);
static void get_battery_info (gchar **capacity, gchar **status);
static void get_memory_info (gchar **memory_text);

/* register the plugin */
XFCE_PANEL_PLUGIN_REGISTER (sample_construct);

/* Update a specific block with new data */
static void
update_block (SamplePlugin *sample, BlockId block_id, const char *text)
{
    if (!sample || block_id >= BLOCK_COUNT || !text)
        return;
    
    pthread_mutex_lock(&sample->mutex);
    
    /* Validate markup before storing it */
    gchar *validated_text = NULL;
    if (pango_parse_markup(text, -1, 0, NULL, NULL, NULL, NULL)) {
        validated_text = g_strdup(text);
    } else {
        /* If markup is invalid, escape the text */
        validated_text = g_markup_escape_text(text, -1);
        g_warning("Invalid markup in block %d: %s", block_id, text);
    }
    
    int len = strlen(validated_text);
    if (len >= MAX_BLOCK_SIZE) {
        len = MAX_BLOCK_SIZE - 1;
    }
    
    sample->blocks[block_id].len = len;
    memcpy(sample->blocks[block_id].data, validated_text, len);
    sample->blocks[block_id].data[len] = '\0';
    
    g_free(validated_text);
    pthread_mutex_unlock(&sample->mutex);
    
    /* Schedule GUI update */
    g_idle_add((GSourceFunc)update_display, sample);
}

/* Update the display with current block data */
static gboolean
update_display (SamplePlugin *sample)
{
    if (!sample || !sample->label)
        return FALSE;
    
    pthread_mutex_lock(&sample->mutex);
    
    GString *display_text = g_string_new("");
    
    /* Combine all enabled blocks */
    for (int i = 0; i < BLOCK_COUNT; i++) {
        gboolean show_block = FALSE;
        
        switch (i) {
            case BLOCK_WEATHER:
                show_block = sample->show_weather;
                break;
            case BLOCK_EXCHANGE_RATE:
                show_block = sample->show_exchange;
                break;
            case BLOCK_BATTERY:
                show_block = sample->show_battery;
                break;
            case BLOCK_MEMORY:
                show_block = sample->show_memory;
                break;
            case BLOCK_DATE:
                show_block = sample->show_date;
                break;
        }
        
        if (show_block && sample->blocks[i].len > 0) {
            if (display_text->len > 0) {
                g_string_append(display_text, " | ");
            }
            g_string_append(display_text, sample->blocks[i].data);
        }
    }
    
    pthread_mutex_unlock(&sample->mutex);
    
    /* Validate the complete markup before setting it */
    if (display_text->len > 0) {
        if (pango_parse_markup(display_text->str, -1, 0, NULL, NULL, NULL, NULL)) {
            /* Update label with markup support for colors */
            gtk_label_set_markup(GTK_LABEL(sample->label), display_text->str);
        } else {
            /* If markup is invalid, show as plain text */
            g_warning("Invalid final markup: %s", display_text->str);
            gtk_label_set_text(GTK_LABEL(sample->label), "Status Error");
        }
    } else {
        gtk_label_set_text(GTK_LABEL(sample->label), "Loading...");
    }
    
    g_string_free(display_text, TRUE);
    
    return FALSE; /* Don't repeat this idle callback */
}



/* Plugin Core Functions */

void
sample_save (XfcePanelPlugin *plugin,
             SamplePlugin    *sample)
{
    XfceRc *rc;
    gchar  *file;

    /* get the config file location */
    file = xfce_panel_plugin_save_location (plugin, TRUE);

    if (G_UNLIKELY (file == NULL))
    {
        DBG ("Failed to open config file");
        return;
    }

    /* open the config file, read/write */
    rc = xfce_rc_simple_open (file, FALSE);
    g_free (file);

    if (G_LIKELY (rc != NULL))
    {
        /* save the settings */
        if (sample->weather_location)
            xfce_rc_write_entry (rc, "weather_location", sample->weather_location);
        
        if (sample->exchange_api_key)
            xfce_rc_write_entry (rc, "exchange_api_key", sample->exchange_api_key);
        
        xfce_rc_write_int_entry  (rc, "update_interval", sample->update_interval);
        xfce_rc_write_bool_entry (rc, "show_weather", sample->show_weather);
        xfce_rc_write_bool_entry (rc, "show_exchange", sample->show_exchange);
        xfce_rc_write_bool_entry (rc, "show_battery", sample->show_battery);
        xfce_rc_write_bool_entry (rc, "show_memory", sample->show_memory);
        xfce_rc_write_bool_entry (rc, "show_date", sample->show_date);

        /* close the rc file */
        xfce_rc_close (rc);
    }
}

static void
sample_read (SamplePlugin *sample)
{
    XfceRc      *rc;
    gchar       *file;
    const gchar *value;

    /* get the plugin config file location */
    file = xfce_panel_plugin_save_location (sample->plugin, TRUE);

    if (G_LIKELY (file != NULL))
    {
        /* open the config file, readonly */
        rc = xfce_rc_simple_open (file, TRUE);

        /* cleanup */
        g_free (file);

        if (G_LIKELY (rc != NULL))
        {
            /* read the settings */
            value = xfce_rc_read_entry (rc, "weather_location", DEFAULT_WEATHER_LOCATION);
            sample->weather_location = g_strdup (value);

            value = xfce_rc_read_entry (rc, "exchange_api_key", DEFAULT_EXCHANGE_API_KEY);
            sample->exchange_api_key = g_strdup (value);

            sample->update_interval = xfce_rc_read_int_entry (rc, "update_interval", DEFAULT_UPDATE_INTERVAL);
            sample->show_weather = xfce_rc_read_bool_entry (rc, "show_weather", DEFAULT_SHOW_WEATHER);
            sample->show_exchange = xfce_rc_read_bool_entry (rc, "show_exchange", DEFAULT_SHOW_EXCHANGE);
            sample->show_battery = xfce_rc_read_bool_entry (rc, "show_battery", DEFAULT_SHOW_BATTERY);
            sample->show_memory = xfce_rc_read_bool_entry (rc, "show_memory", DEFAULT_SHOW_MEMORY);
            sample->show_date = xfce_rc_read_bool_entry (rc, "show_date", DEFAULT_SHOW_DATE);

            /* cleanup */
            xfce_rc_close (rc);

            /* leave the function, everything went well */
            return;
        }
    }

    /* something went wrong, apply default values */
    DBG ("Applying default settings");

    sample->weather_location = g_strdup (DEFAULT_WEATHER_LOCATION);
    sample->exchange_api_key = g_strdup (DEFAULT_EXCHANGE_API_KEY);
    sample->update_interval = DEFAULT_UPDATE_INTERVAL;
    sample->show_weather = DEFAULT_SHOW_WEATHER;
    sample->show_exchange = DEFAULT_SHOW_EXCHANGE;
    sample->show_battery = DEFAULT_SHOW_BATTERY;
    sample->show_memory = DEFAULT_SHOW_MEMORY;
    sample->show_date = DEFAULT_SHOW_DATE;
}

static void
start_threads (SamplePlugin *sample)
{
    /* Initialize all blocks */
    for (int i = 0; i < BLOCK_COUNT; i++) {
        sample->blocks[i].len = 0;
        sample->blocks[i].data[0] = '\0';
    }
    
    /* Start threads */
    if (sample->show_date) {
        sample->date_thread.running = TRUE;
        sample->date_thread.thread = g_thread_new("date_thread", date_thread_func, sample);
    }
    
    if (sample->show_memory) {
        sample->memory_thread.running = TRUE;
        sample->memory_thread.thread = g_thread_new("memory_thread", memory_thread_func, sample);
    }
    
    if (sample->show_weather && sample->weather_location) {
        sample->weather_thread.running = TRUE;
        sample->weather_thread.thread = g_thread_new("weather_thread", weather_thread_func, sample);
    }
    
    if (sample->show_exchange && sample->exchange_api_key) {
        sample->exchange_thread.running = TRUE;
        sample->exchange_thread.thread = g_thread_new("exchange_thread", exchange_thread_func, sample);
    }
    
    if (sample->show_battery) {
        sample->battery_thread.running = TRUE;
        sample->battery_thread.thread = g_thread_new("battery_thread", battery_thread_func, sample);
    }
}

static void
stop_threads (SamplePlugin *sample)
{
    /* Stop all threads */
    sample->date_thread.running = FALSE;
    sample->memory_thread.running = FALSE;
    sample->weather_thread.running = FALSE;
    sample->exchange_thread.running = FALSE;
    sample->battery_thread.running = FALSE;
    
    /* Wait for threads to finish */
    if (sample->date_thread.thread) {
        g_thread_join(sample->date_thread.thread);
        sample->date_thread.thread = NULL;
    }
    
    if (sample->memory_thread.thread) {
        g_thread_join(sample->memory_thread.thread);
        sample->memory_thread.thread = NULL;
    }
    
    if (sample->weather_thread.thread) {
        g_thread_join(sample->weather_thread.thread);
        sample->weather_thread.thread = NULL;
    }
    
    if (sample->exchange_thread.thread) {
        g_thread_join(sample->exchange_thread.thread);
        sample->exchange_thread.thread = NULL;
    }
    
    if (sample->battery_thread.thread) {
        g_thread_join(sample->battery_thread.thread);
        sample->battery_thread.thread = NULL;
    }
}

static SamplePlugin *
sample_new (XfcePanelPlugin *plugin)
{
    SamplePlugin   *sample;
    GtkOrientation  orientation;

    /* allocate memory for the plugin structure */
    sample = g_slice_new0 (SamplePlugin);

    /* pointer to plugin */
    sample->plugin = plugin;

    /* read the user settings */
    sample_read (sample);

    /* Initialize mutex */
    pthread_mutex_init(&sample->mutex, NULL);

    /* get the current orientation */
    orientation = xfce_panel_plugin_get_orientation (plugin);

    /* create some panel widgets */
    sample->ebox = gtk_event_box_new ();
    gtk_widget_show (sample->ebox);

    sample->hvbox = gtk_box_new (orientation, 2);
    gtk_widget_show (sample->hvbox);
    gtk_container_add (GTK_CONTAINER (sample->ebox), sample->hvbox);

    /* Create main label */
    sample->label = gtk_label_new (_("Loading..."));
    gtk_widget_show (sample->label);
    gtk_box_pack_start (GTK_BOX (sample->hvbox), sample->label, FALSE, FALSE, 0);

    /* Start update threads */
    start_threads(sample);

    return sample;
}

static void
sample_free (XfcePanelPlugin *plugin,
             SamplePlugin    *sample)
{
    GtkWidget *dialog;

    /* Stop threads first */
    stop_threads(sample);

    /* check if the dialog is still open. if so, destroy it */
    dialog = g_object_get_data (G_OBJECT (plugin), "dialog");
    if (G_UNLIKELY (dialog != NULL))
        gtk_widget_destroy (dialog);

    /* destroy the panel widgets */
    gtk_widget_destroy (sample->hvbox);

    /* cleanup the settings */
    if (G_LIKELY (sample->weather_location != NULL))
        g_free (sample->weather_location);
    if (G_LIKELY (sample->exchange_api_key != NULL))
        g_free (sample->exchange_api_key);

    /* Destroy mutex */
    pthread_mutex_destroy(&sample->mutex);

    /* free the plugin structure */
    g_slice_free (SamplePlugin, sample);
}

static void
sample_orientation_changed (XfcePanelPlugin *plugin,
                            GtkOrientation   orientation,
                            SamplePlugin    *sample)
{
    /* change the orientation of the box */
    gtk_orientable_set_orientation(GTK_ORIENTABLE(sample->hvbox), orientation);
}

static gboolean
sample_size_changed (XfcePanelPlugin *plugin,
                     gint             size,
                     SamplePlugin    *sample)
{
    GtkOrientation orientation;

    /* get the orientation of the plugin */
    orientation = xfce_panel_plugin_get_orientation (plugin);

    /* set the widget size */
    if (orientation == GTK_ORIENTATION_HORIZONTAL)
        gtk_widget_set_size_request (GTK_WIDGET (plugin), -1, size);
    else
        gtk_widget_set_size_request (GTK_WIDGET (plugin), size, -1);

    /* we handled the orientation */
    return TRUE;
}



static void
sample_construct (XfcePanelPlugin *plugin)
{
    SamplePlugin *sample;

    /* setup translation domain */
    xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

    /* Initialize CURL globally */
    curl_global_init(CURL_GLOBAL_DEFAULT);

    /* create the plugin */
    sample = sample_new (plugin);

    /* add the ebox to the panel */
    gtk_container_add (GTK_CONTAINER (plugin), sample->ebox);

    /* show the panel's right-click menu on this ebox */
    xfce_panel_plugin_add_action_widget (plugin, sample->ebox);

    /* connect plugin signals */
    g_signal_connect (G_OBJECT (plugin), "free-data",
                      G_CALLBACK (sample_free), sample);

    g_signal_connect (G_OBJECT (plugin), "save",
                      G_CALLBACK (sample_save), sample);

    g_signal_connect (G_OBJECT (plugin), "size-changed",
                      G_CALLBACK (sample_size_changed), sample);

    g_signal_connect (G_OBJECT (plugin), "orientation-changed",
                      G_CALLBACK (sample_orientation_changed), sample);

    /* show the configure menu item and connect signal */
    xfce_panel_plugin_menu_show_configure (plugin);
    g_signal_connect (G_OBJECT (plugin), "configure-plugin",
                      G_CALLBACK (sample_configure), sample);

    /* show the about menu item and connect signal */
    xfce_panel_plugin_menu_show_about (plugin);
    g_signal_connect (G_OBJECT (plugin), "about",
                      G_CALLBACK (sample_about), NULL);
}

/* Utility Functions */

/* CURL write callback */
static size_t
write_response_callback (void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t total_size = size * nmemb;
    gchar **output = (gchar**)userp;
    gchar *temp = g_realloc(*output, total_size + 1);
    if (temp) {
        *output = temp;
        memcpy(*output, contents, total_size);
        (*output)[total_size] = '\0';
    }
    return total_size;
}

/* Get weather data from API */
static gchar*
get_weather_data (const gchar *location)
{
    if (!location) return NULL;
    
    CURL *curl;
    CURLcode res;
    gchar *response = NULL;
    gchar *url;
    gchar **parts;
    
    /* Parse latitude,longitude */
    parts = g_strsplit(location, ",", 2);
    if (!parts || !parts[0] || !parts[1]) {
        g_strfreev(parts);
        return NULL;
    }
    
    url = g_strdup_printf("https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s&current_weather=true",
                          g_strstrip(parts[0]), g_strstrip(parts[1]));
    g_strfreev(parts);
    
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            g_free(response);
            response = NULL;
        }
    }
    
    g_free(url);
    return response;
}

/* Get exchange rate data from API */
static gchar*
get_exchange_data (const gchar *api_key)
{
    if (!api_key) return NULL;
    
    CURL *curl;
    CURLcode res;
    gchar *response = NULL;
    gchar *url;
    
    url = g_strdup_printf("https://openexchangerates.org/api/latest.json?app_id=%s", api_key);
    
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            g_free(response);
            response = NULL;
        }
    }
    
    g_free(url);
    return response;
}

/* Get battery information */
static void
get_battery_info (gchar **capacity, gchar **status)
{
    FILE *fp;
    gchar buffer[64];
    
    *capacity = NULL;
    *status = NULL;
    
    /* Get capacity */
    fp = fopen("/sys/class/power_supply/BAT0/capacity", "r");
    if (fp) {
        if (fgets(buffer, sizeof(buffer), fp)) {
            g_strchomp(buffer);
            *capacity = g_strdup(buffer);
        }
        fclose(fp);
    }
    
    /* Get status */
    fp = fopen("/sys/class/power_supply/BAT0/status", "r");
    if (fp) {
        if (fgets(buffer, sizeof(buffer), fp)) {
            g_strchomp(buffer);
            *status = g_strdup(buffer);
        }
        fclose(fp);
    }
}

/* Get memory information */
static void
get_memory_info (gchar **memory_text)
{
    FILE *fp;
    gchar line[256];
    gulong mem_total = 0, mem_free = 0, cached = 0, buffers = 0, mem_reclaimable = 0;
    
    *memory_text = NULL;
    
    fp = fopen("/proc/meminfo", "r");
    if (!fp) return;
    
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "MemTotal: %lu kB", &mem_total) == 1) continue;
        if (sscanf(line, "MemFree: %lu kB", &mem_free) == 1) continue;
        if (sscanf(line, "Cached: %lu kB", &cached) == 1) continue;
        if (sscanf(line, "Buffers: %lu kB", &buffers) == 1) continue;
        if (sscanf(line, "SReclaimable: %lu kB", &mem_reclaimable) == 1) continue;
    }
    
    fclose(fp);
    
    if (mem_total > 0) {
        gulong mem_cached_all = cached + mem_reclaimable;
        gulong mem_used = mem_total - mem_free - mem_cached_all;
        gdouble mem_used_gb = mem_used / 1024.0 / 1024.0;
        
        *memory_text = g_strdup_printf("<span color='#186da5'>🗄️ %.1fGB</span>", mem_used_gb);
    }
}

/* Thread Functions */

/* Date/Time thread */
static gpointer
date_thread_func (gpointer data)
{
    SamplePlugin *sample = (SamplePlugin *)data;
    
    while (sample->date_thread.running) {
        time_t now = time(NULL);
        struct tm *timeinfo = localtime(&now);
        
        gchar *date_str = g_strdup_printf(
            "<span color='#07d7e8'>📅</span> <span color='#10bbbb'>%s %s %d %s %02d:%02d</span>",
            (gchar*[]){"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"}[timeinfo->tm_wday],
            (gchar*[]){"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"}[timeinfo->tm_mon],
            timeinfo->tm_mday,
            (timeinfo->tm_hour >= 8 && timeinfo->tm_hour < 21) ? "<span color='#edd238'>☀️</span>" : "<span color='#ecede8'>🌙</span>",
            timeinfo->tm_hour,
            timeinfo->tm_min
        );
        
        update_block(sample, BLOCK_DATE, date_str);
        g_free(date_str);
        
        /* Sleep until next minute */
        int sleep_time = 60 - timeinfo->tm_sec;
        for (int i = 0; i < sleep_time && sample->date_thread.running; i++) {
            g_usleep(1000000); /* 1 second */
        }
    }
    
    return NULL;
}

/* Memory thread */
static gpointer
memory_thread_func (gpointer data)
{
    SamplePlugin *sample = (SamplePlugin *)data;
    
    while (sample->memory_thread.running) {
        gchar *memory_text;
        get_memory_info(&memory_text);
        
        if (memory_text) {
            update_block(sample, BLOCK_MEMORY, memory_text);
            g_free(memory_text);
        }
        
        /* Update every 5 seconds */
        for (int i = 0; i < 5 && sample->memory_thread.running; i++) {
            g_usleep(1000000);
        }
    }
    
    return NULL;
}

/* Weather thread */
static gpointer
weather_thread_func (gpointer data)
{
    SamplePlugin *sample = (SamplePlugin *)data;
    
    while (sample->weather_thread.running) {
        if (sample->weather_location) {
            gchar *weather_json = get_weather_data(sample->weather_location);
            
            if (weather_json) {
                JsonParser *parser = json_parser_new();
                GError *error = NULL;
                
                if (json_parser_load_from_data(parser, weather_json, -1, &error)) {
                    JsonNode *root = json_parser_get_root(parser);
                    JsonObject *root_obj = json_node_get_object(root);
                    JsonObject *current_weather = json_object_get_object_member(root_obj, "current_weather");
                    
                    if (current_weather) {
                        gdouble temperature = json_object_get_double_member(current_weather, "temperature");
                        
                        const gchar *icon;
                        const gchar *color;
                        if (temperature < 0) {
                            icon = "❄️"; color = "#1e90ff";
                        } else if (temperature < 10) {
                            icon = "🥶"; color = "#00bfff";
                        } else if (temperature < 18) {
                            icon = "🌿"; color = "#32cd32";
                        } else if (temperature < 22) {
                            icon = "😊"; color = "#ffd700";
                        } else if (temperature < 30) {
                            icon = "🌡️"; color = "#ffa500";
                        } else {
                            icon = "🔥"; color = "#ff4500";
                        }
                        
                        gchar *weather_text = g_strdup_printf(
                            "<span color='%s'>%s %.1f°C</span>", 
                            color, icon, temperature
                        );
                        
                        update_block(sample, BLOCK_WEATHER, weather_text);
                        g_free(weather_text);
                    }
                }
                
                if (error) {
                    g_error_free(error);
                }
                g_object_unref(parser);
                g_free(weather_json);
            }
        }
        
        /* Update every 30 minutes */
        for (int i = 0; i < 1800 && sample->weather_thread.running; i++) {
            g_usleep(1000000);
        }
    }
    
    return NULL;
}

/* Exchange rate thread */
static gpointer
exchange_thread_func (gpointer data)
{
    SamplePlugin *sample = (SamplePlugin *)data;
    
    while (sample->exchange_thread.running) {
        if (sample->exchange_api_key) {
            gchar *exchange_json = get_exchange_data(sample->exchange_api_key);
            
            if (exchange_json) {
                JsonParser *parser = json_parser_new();
                GError *error = NULL;
                
                if (json_parser_load_from_data(parser, exchange_json, -1, &error)) {
                    JsonNode *root = json_parser_get_root(parser);
                    JsonObject *root_obj = json_node_get_object(root);
                    JsonObject *rates = json_object_get_object_member(root_obj, "rates");
                    
                    if (rates) {
                        GString *exchange_text = g_string_new("");
                        gboolean has_try = FALSE, has_rub = FALSE;
                        gdouble try_rate = 0.0, rub_rate = 0.0;
                        
                        /* Get TRY and RUB rates */
                        if (json_object_has_member(rates, "TRY")) {
                            try_rate = json_object_get_double_member(rates, "TRY");
                            has_try = TRUE;
                        }
                        
                        if (json_object_has_member(rates, "RUB")) {
                            rub_rate = json_object_get_double_member(rates, "RUB");
                            has_rub = TRUE;
                        }
                        
                        /* Build the exchange text carefully */
                        if (has_try) {
                            g_string_append_printf(exchange_text, 
                                "<span color='#07d7e8'>TRY</span> <span color='#10bbbb'>%.2f</span>", 
                                try_rate);
                        }
                        
                        if (has_rub) {
                            if (has_try) {
                                g_string_append(exchange_text, " ");
                            }
                            g_string_append_printf(exchange_text, 
                                "<span color='#07d7e8'>RUB</span> <span color='#10bbbb'>%.2f</span>", 
                                rub_rate);
                        }
                        
                        if (exchange_text->len > 0) {
                            update_block(sample, BLOCK_EXCHANGE_RATE, exchange_text->str);
                        }
                        
                        g_string_free(exchange_text, TRUE);
                    }
                }
                
                if (error) {
                    g_error_free(error);
                }
                g_object_unref(parser);
                g_free(exchange_json);
            }
        }
        
        /* Update every 30 minutes */
        for (int i = 0; i < 1800 && sample->exchange_thread.running; i++) {
            g_usleep(1000000);
        }
    }
    
    return NULL;
}

/* Battery thread */
static gpointer
battery_thread_func (gpointer data)
{
    SamplePlugin *sample = (SamplePlugin *)data;
    
    while (sample->battery_thread.running) {
        gchar *capacity_str, *status_str;
        get_battery_info(&capacity_str, &status_str);
        
        if (capacity_str) {
            int capacity = atoi(capacity_str);
            const gchar *icon, *color;
            
            if (capacity < 10) {
                icon = "🔋"; color = "#ff0000";
            } else if (capacity < 25) {
                icon = "🔋"; color = "#eb9634";
            } else if (capacity < 50) {
                icon = "🔋"; color = "#ebd334";
            } else if (capacity < 75) {
                icon = "🔋"; color = "#c6eb34";
            } else {
                icon = "🔋"; color = "#00ff00";
            }
            
            gchar *charging_icon = "";
            if (status_str && g_strcmp0(status_str, "Charging") == 0) {
                charging_icon = " <span color='#cccccc'>⚡</span>";
            }
            
            gchar *battery_text = g_strdup_printf(
                "<span color='%s'>%s %d%%</span>%s",
                color, icon, capacity, charging_icon
            );
            
            update_block(sample, BLOCK_BATTERY, battery_text);
            g_free(battery_text);
        }
        
        g_free(capacity_str);
        g_free(status_str);
        
        /* Update every 10 seconds */
        for (int i = 0; i < 10 && sample->battery_thread.running; i++) {
            g_usleep(1000000);
        }
    }
    
    return NULL;
}
