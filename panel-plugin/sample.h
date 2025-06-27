/*  $Id$
 *
 *  Copyright (C) 2012 John Doo <john@foo.org>
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

#ifndef __SAMPLE_H__
#define __SAMPLE_H__

#include <gtk/gtk.h>
#include <libxfce4panel/libxfce4panel.h>
#include <pthread.h>
#include <time.h>

G_BEGIN_DECLS

#define MAX_BLOCK_SIZE 256

/* Block identifiers for different status components */
typedef enum {
    BLOCK_WEATHER = 0,
    BLOCK_EXCHANGE_RATE,
    BLOCK_BATTERY,
    BLOCK_MEMORY,
    BLOCK_DATE,
    BLOCK_COUNT
} BlockId;

/* Structure to hold individual block data */
typedef struct {
    int  len;
    char data[MAX_BLOCK_SIZE];
} BlockData;

/* Status bar update thread data */
typedef struct {
    GThread     *thread;
    gboolean     running;
    gint         update_interval;
    gpointer     user_data;
} StatusThread;

/* plugin structure */
typedef struct
{
    XfcePanelPlugin *plugin;

    /* panel widgets */
    GtkWidget       *ebox;
    GtkWidget       *hvbox;
    GtkWidget       *label;

    /* Status bar data */
    BlockData       blocks[BLOCK_COUNT];
    pthread_mutex_t mutex;
    
    /* Update threads */
    StatusThread    date_thread;
    StatusThread    memory_thread;
    StatusThread    weather_thread;
    StatusThread    exchange_thread;
    StatusThread    battery_thread;
    
    /* Settings */
    gchar           *weather_location;    /* latitude,longitude */
    gchar           *exchange_api_key;    /* OpenExchangeRates API key */
    gint             update_interval;     /* Base update interval in seconds */
    gboolean         show_weather;
    gboolean         show_exchange;
    gboolean         show_battery;
    gboolean         show_memory;
    gboolean         show_date;
}
SamplePlugin;



void
sample_save (XfcePanelPlugin *plugin,
             SamplePlugin    *sample);

G_END_DECLS

#endif /* !__SAMPLE_H__ */
