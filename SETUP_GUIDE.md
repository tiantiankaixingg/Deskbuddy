# Deskbuddy Setup Guide

This guide covers how to build and flash Deskbuddy, and how to configure the features available in the device web UI.

---

## What you need

- [ESP32-2432S028 board](https://www.aliexpress.com/item/1005010525144441.html) (the "Cheap Yellow Display" / CYD)
- USB-A to USB-micro data cable (not a charge-only cable) — only needed for the initial flash
- Python 3.9+ (for the configurator)
- [PlatformIO](https://platformio.org/) (CLI or VS Code extension)

---

## 1. Clone the repo

```bash
git clone https://github.com/tiantiankaixingg/Deskbuddy.git
cd Deskbuddy
```

---

## 2. Create your secrets file

Deskbuddy keeps WiFi credentials and your location out of version control.

```bash
cp include/secrets.h.example include/secrets.h
```

Open `include/secrets.h` and fill in your values:

```cpp
#define WIFI_SSID "your_wifi_name"
#define WIFI_PASS "your_wifi_password"

#define DEFAULT_LAT      52.3676f   // your latitude
#define DEFAULT_LNG       4.9041f   // your longitude
#define DEFAULT_LOCATION "Amsterdam"
```

You can find coordinates for your city on [Google Maps](https://maps.google.com): right-click any point and copy the coordinates.

> `secrets.h` is listed in `.gitignore` and will never be committed.

---

## 3. Flash: PlatformIO CLI (USB)

```bash
# Build only
pio run

# Build and flash
pio run --target upload

# Open serial monitor (shows IP address on first boot)
pio device monitor
```

---

## 4. Flash: Configurator {#configurator}

The configurator is a local Python web app that handles WiFi credentials, location, feature flags, building, and flashing — all from your browser.

```bash
python3 configurator.py
```

Then open [http://localhost:5000](http://localhost:5000).

From there you can:

- Set WiFi SSID and password (written to `secrets.h`, never committed)
- Set your location (city name, latitude, longitude)
- Enable optional feature modules
- **Compile** — build the firmware only
- **Compile & Flash** — build and upload over USB
- **Flash OTA** — push the last compiled firmware to the device over WiFi (no USB needed)

> The configurator needs PlatformIO installed and the board connected by USB for the USB flash options. OTA only requires the device to be on the same network.

---

## 5. OTA updates (after initial setup)

Once the device is on your network, you can update firmware without touching a USB cable.

**From the configurator:**
1. Click **Compile** to build the latest firmware
2. Click **Flash OTA** to push it to the device — it reboots automatically

**From the device web UI:**
1. Run `pio run` to build (or use the configurator)
2. Open `http://<device-ip>/update` in your browser
3. Select the `firmware.bin` from `.pio/build/esp32dev/` and click Flash

> Changes to `secrets.h` (WiFi credentials, default location) are compiled into the firmware, so those always require a rebuild followed by a USB or OTA flash. Everything else (themes, widgets, reminders, timezone, etc.) is saved to NVS on the device and can be changed live from the web UI without reflashing.

---

## 6. First boot

After a successful flash:

1. The display turns on and shows a splash/sync screen
2. The device connects to WiFi and syncs time via NTP
3. Weather data is fetched from [Open-Meteo](https://open-meteo.com/) (free, no API key needed)
4. The home screen appears

The device's local IP address is shown on the **Status page** (tap the nav bar). It also prints to the serial monitor on boot.

---

## 7. Device web UI

Open `http://<device-ip>` in any browser on the same network.

| Setting | Where |
|---------|-------|
| Background theme | Appearance section |
| Accent and text color | Appearance section |
| Home widget layout | Widget customization |
| Class schedule (Mon–Fri, up to 6 slots a day) | Class Schedule section |
| Context clock (second timezone) | Context Clock section |
| Location (name, lat, lng) | Location section |
| Timezone | Location section |
| Units (metric / imperial) | Location section |
| Eye break reminder | Reminders section |
| Movement reminder | Reminders section |
| Focus timer presets | Timer section |
| Nickname | General section |
| Sleep / dim behavior | Display section |
| Home Assistant config | HA section |

All settings are saved to the device's non-volatile storage (NVS) and survive reboots. No reflashing needed.

---

## 8. Context clock

The context clock shows a second timezone on the clock card, below the date row. Useful if you work with people in a different timezone.

Configure it in the **Context Clock** section of the device web UI:
- **Label** — short name shown on screen, e.g. `NYC`, `Tokyo`, `London` (max 8 characters)
- **Timezone** — picked from the same list as the primary timezone

Leave the label empty to disable it.

---

## 9. Reminders

Both reminders appear as a full-screen overlay on any page and wake the display if it is dimmed.

**Eye break (20-20-20 rule):** fires every N minutes, shows "Look away" with a 20-second countdown that auto-dismisses. Tap to dismiss early.

**Movement reminder:** fires every N minutes, shows "Time to move!" — tap anywhere to dismiss.

Configure both in the **Reminders** section of the device web UI. The interval range is 5–120 minutes.

---

## 10. Class schedule

The Schedule page shows the timetable for the current weekday (Monday to Friday, up to 6 classes a day). A slot with an empty time counts as a free period, and weekends show "今天没有课程安排". A matching **Schedule** home widget shows how many classes there are today.

Configure it in the **Class Schedule** panel of the device web UI:

- Fill in `HH:MM`, class name and room for each slot, then press **Save**.
- **Bulk import**: paste a whole timetable into the 批量导入 box — one class per line, formatted `day time class room` (spaces or tabs), for example `周一 08:00 高等数学 教学楼A301`. Days can also be grouped: put the day name on its own line and follow it with `time class room` lines. Press 解析填入 to parse it into the form, then **Save**.
- 复制当前课表 copies the current timetable back to the clipboard.

---

## 11. Optional: Arduino IDE (single-file version)

If you prefer Arduino IDE over PlatformIO, use `desk_buddy_github.cpp`. Rename it to a `.ino` file and install the required libraries. The `User_Setup.h` in the repo root is already set up for TFT_eSPI on the CYD (ST7789 panel, BGR colour order, 40 MHz SPI) — keep it next to the `.ino` so the IDE picks it up instead of the library default.

Required libraries (via Library Manager):
- `TFT_eSPI`
- `ArduinoJson`
- `XPT2046_Touchscreen`

> The Arduino IDE version is a standalone snapshot and does not include all features from the main PlatformIO build (no OTA, no context clock, no reminders).

---

## Troubleshooting

### Display is black or white after flashing

This should not happen with the PlatformIO build. `LGFX_config.hpp` is preconfigured for the ESP32-2432S028. If you modified the display config, double-check the panel and bus settings.

### USB upload fails

- Make sure the board is connected over USB (not a charge-only cable)
- Try holding the **BOOT** button on the board while the upload starts
- Check that the correct port is selected: `pio device list`

### OTA flash fails

- Check the device IP in the configurator settings matches the actual device IP (shown on the Status page)
- Make sure the device is on the same network as your computer
- Make sure you have compiled before trying to Flash OTA — the button sends the last built `firmware.bin`

### WiFi does not connect

- Check your SSID and password in `secrets.h`
- Make sure the network is 2.4 GHz (the ESP32 does not support 5 GHz)
- Rebuild and reflash after editing `secrets.h`

### Time or weather is wrong

- The timezone is set via the device web UI, not hardcoded
- Weather uses the latitude/longitude from the device web UI — update them if needed
- Make sure the device has internet access

### Touch position is offset

- Do not change `ts.setRotation(2)` in the source. This value is calibrated for the ESP32-2432S028 and must stay at 2.

### I changed `secrets.h` but the device still uses the old values

`secrets.h` is compiled into the firmware. Any change requires a rebuild and reflash (USB or OTA). Runtime settings changed via the web UI are saved to NVS immediately without reflashing.
