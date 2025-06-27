# XFCE Status Bar Plugin

This is a comprehensive status bar plugin for XFCE Panel that displays:

- **Weather Information** - Current temperature with weather icons
- **Exchange Rates** - TRY and RUB exchange rates (USD base)
- **Battery Status** - Battery level with charging indicator  
- **Memory Usage** - Current RAM usage
- **Date/Time** - Current date and time with day/night icons

## Features

- Real-time data updates using background threads
- Configurable display components
- Color-coded status indicators
- API integration for weather and exchange rates
- System monitoring (battery, memory)

## Installation

### Build from Source

```bash
cd /home/nikita/projects/xfce4-sample-plugin
meson compile -C build
sudo meson install -C build
```

### Add to XFCE Panel

1. Right-click on the XFCE panel
2. Go to "Panel" → "Add New Items"
3. Find "Status Bar Plugin" in the list
4. Click "Add" to add it to your panel

## Configuration

Right-click on the plugin in the panel and select "Properties" to configure:

### API Keys and Location

- **Weather Location**: Enter your coordinates in the format `latitude,longitude` (e.g., `37.7749,-122.4194` for San Francisco)
- **Exchange API Key**: Get a free API key from [OpenExchangeRates](https://openexchangerates.org/) and enter it here

### Display Components

Toggle which components you want to show:
- ☑️ Show Weather
- ☑️ Show Exchange Rates  
- ☑️ Show Battery
- ☑️ Show Memory Usage
- ☑️ Show Date/Time

## Environment Variables (Alternative)

You can also set these environment variables instead of using the GUI:

```bash
export MY_LOCATION="37.7749,-122.4194"
export OPENEXCHANGERATES_API_KEY="your_api_key_here"
```

## Dependencies

The plugin requires these libraries:
- GTK+ 3.24+
- libxfce4panel 4.16+
- libcurl (for HTTP requests)
- libudev (for battery monitoring)
- json-glib (for JSON parsing)

## API Services

### Weather Data
- **Service**: [Open-Meteo](https://open-meteo.com/)
- **Cost**: Free, no API key required
- **Data**: Current weather conditions

### Exchange Rates  
- **Service**: [OpenExchangeRates](https://openexchangerates.org/)
- **Cost**: Free tier available (1000 requests/month)
- **Data**: USD-based currency exchange rates

## Technical Details

### Architecture
- Multi-threaded design using GLib threads
- Thread-safe updates using mutex locks
- Idle callbacks for GUI updates
- Configurable update intervals

### Update Frequencies
- **Date/Time**: Every minute
- **Memory**: Every 5 seconds  
- **Battery**: Every 10 seconds
- **Weather**: Every 30 minutes
- **Exchange Rates**: Every 30 minutes

### File Locations
- **Plugin Binary**: `/usr/local/lib/xfce4/panel/plugins/libsample.so`
- **Desktop File**: `/usr/local/share/xfce4/panel/plugins/sample.desktop`
- **Config**: `~/.config/xfce4/panel/`

## Troubleshooting

### Plugin Not Appearing
1. Make sure XFCE panel is restarted: `xfce4-panel -r`
2. Check if plugin is installed: `ls /usr/local/lib/xfce4/panel/plugins/`

### Weather Not Working
1. Verify your coordinates are correct
2. Check internet connectivity
3. Ensure Open-Meteo API is accessible

### Exchange Rates Not Working  
1. Verify your API key is valid
2. Check OpenExchangeRates quota limits
3. Ensure internet connectivity

### No Battery Information
- Plugin looks for `/sys/class/power_supply/BAT0/`
- Desktop systems may not have battery information

## Migration from DWM Status Bar

This plugin ports functionality from a DWM status bar script with these changes:

1. **Threading**: Changed from boost::asio to GLib threads
2. **JSON Parsing**: Changed from nlohmann/json to json-glib  
3. **UI Framework**: Changed from X11/dwm to GTK/XFCE
4. **Configuration**: Added GUI configuration dialog
5. **Display**: Uses GTK labels with Pango markup instead of X11 window names

## Contributing

To extend the plugin:

1. Add new block types to `BlockId` enum in `sample.h`
2. Create thread functions in `sample.c`
3. Add configuration options in `sample-dialogs.c`
4. Update settings save/load functions

## License

GPL-2.0-or-later (same as XFCE)
