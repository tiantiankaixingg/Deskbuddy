// Deskbuddy V.8
// Nav: Home / Weather / Notes / Status
// Full version
// - KP dots replaced with Low / Medium / High / Extreme text
// - KP level text uses same small font as wind direction and stays inside the box
// - Wind + direction added to Weather page
// - Wind direction uses Accent color
// - Weather sun event field automatically shows Sunrise or Sunset, whichever is next
// - Uptime added to Status page

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include "LGFX_config.hpp"
#include "secrets.h"
#include <time.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <Preferences.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <math.h>

// =========================================================
// DISPLAY / TOUCH
// =========================================================
LGFX tft;

const int ROT = 0;  // offset_rotation=2 in LGFX_config handles the physical flip
const bool INV = false;

#define TOUCH_CS  33
#define TOUCH_IRQ 36

static const int T_SCK  = 25;
static const int T_MISO = 39;
static const int T_MOSI = 32;

SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS);

static const int TOUCH_X_MIN = 562;
static const int TOUCH_X_MAX = 3604;
static const int TOUCH_Y_MIN = 544;
static const int TOUCH_Y_MAX = 3720;

static const bool TOUCH_SWAP_XY = false;
static const bool TOUCH_FLIP_X  = false;
static const bool TOUCH_FLIP_Y  = false;

// =========================================================
// WEB / STORAGE
// =========================================================
WebServer server(80);
Preferences prefs;

// =========================================================
// SPRITES
// =========================================================
LGFX_Sprite sprClock(&tft);
LGFX_Sprite sprSmall(&tft);

// =========================================================
// LOCATION
// =========================================================
float LAT = DEFAULT_LAT;
float LNG = DEFAULT_LNG;
String locationName = DEFAULT_LOCATION;

// =========================================================
// THEME
// =========================================================
uint16_t COL_BG        = 0xE75E;
uint16_t COL_PANEL     = 0xCEFD;
uint16_t COL_PANEL_ALT = 0xC73C;
uint16_t COL_STROKE    = 0x9E19;
uint16_t COL_TEXT      = 0x1082;
uint16_t COL_DIM       = 0x6B4D;
uint16_t COL_ACCENT    = 0x2914;

const uint16_t COL_GREEN  = 0xF81F;
const uint16_t COL_YELLOW = 0xF800;
const uint16_t COL_RED    = 0xFFE0;
const uint16_t COL_BLUE   = 0x03FF;

String textColorKey = "standard";
String unitKey = "metric"; // metric = C/mm, imperial = F/in
String regionFormatKey = "europe"; // europe = 24h + dd.mm.yyyy, us = 12h + mm/dd/yyyy
String timezoneKey = "europe_central";

// =========================================================
// LAYOUT
// =========================================================
const int SCREEN_W = 240;
const int SCREEN_H = 320;
const int TOPBAR_H = 34;
const int NAV_H    = 44;

const int HOME_GRID_Y1 = 120;
const int HOME_GRID_Y2 = 198;
const int HOME_WIDGET_H = 70;

const int HOME_TIMER_X = 124;
const int HOME_TIMER_Y = HOME_GRID_Y1;
const int HOME_TIMER_W = 108;
const int HOME_TIMER_H = HOME_WIDGET_H;
const int TIMER_MENU_X = 20;
const int TIMER_MENU_Y = 68;
const int TIMER_MENU_W = 200;
const int TIMER_MENU_H = 194;
const int TIMER_DONE_X = 26;
const int TIMER_DONE_Y = 92;
const int TIMER_DONE_W = 188;
const int TIMER_DONE_H = 108;
const int PAGE_ROW1_Y = 42;
const int PAGE_ROW2_Y = 120;
const int PAGE_ROW3_Y = 198;
const int PAGE_WIDGET_H = HOME_WIDGET_H;

const uint32_t NTP_MIN_EPOCH = 1700000000UL;

// =========================================================
// DAILY QUOTE (replaces notes)
// =========================================================
const int DAILY_QUOTE_COUNT = 25;
const char* DAILY_QUOTES[DAILY_QUOTE_COUNT] = {
  "学如逆水行舟，不进则退。",
  "业精于勤，荒于嬉；行成于思，毁于随。",
  "千里之行，始于足下。",
  "不积跬步，无以至千里。",
  "书山有路勤为径，学海无涯苦作舟。",
  "天行健，君子以自强不息。",
  "路漫漫其修远兮，吾将上下而求索。",
  "宝剑锋从磨砺出，梅花香自苦寒来。",
  "少壮不努力，老大徒伤悲。",
  "一分耕耘，一分收获。",
  "志当存高远。",
  "今日事，今日毕。",
  "静以修身，俭以养德。",
  "非淡泊无以明志，非宁静无以致远。",
  "知之者不如好之者，好之者不如乐之者。",
  "三人行，必有我师焉。",
  "温故而知新，可以为师矣。",
  "锲而不舍，金石可镂。",
  "博观而约取，厚积而薄发。",
  "长风破浪会有时，直挂云帆济沧海。",
  "此刻打盹，你将做梦；此刻学习，你将圆梦。",
  "每天进步一点点，坚持带来大改变。",
  "自律给我自由。",
  "成功永远属于有准备的人。",
  "越努力，越幸运。"
};

String notesText = "No notes yet.";   // 自定义句子，留空(No notes yet.)则显示每日一句
bool notesDirty = true;
String quoteOnline = "";              // 联网获取的今日语录
String quoteOnlineDate = "";          // 该语录对应的日期 YYYYMMDD
String buddyNickname = "";

enum HomeWidgetType {
  HOME_WIDGET_WEEK = 0,
  HOME_WIDGET_TIMER,
  HOME_WIDGET_RAIN,
  HOME_WIDGET_OUTDOOR,
  HOME_WIDGET_KP,
  HOME_WIDGET_UV,
  HOME_WIDGET_WIND,
  HOME_WIDGET_SUN,
  HOME_WIDGET_HYDRATION,
  HOME_WIDGET_SCHEDULE,
  HOME_WIDGET_HA1,
  HOME_WIDGET_HA2,
  HOME_WIDGET_HA3,
  HOME_WIDGET_HA4,
  HOME_WIDGET_COUNTDOWN
};

const int HOME_SLOT_COUNT = 4;
HomeWidgetType homeWidgetSlots[HOME_SLOT_COUNT] = {
  HOME_WIDGET_WEEK,
  HOME_WIDGET_TIMER,
  HOME_WIDGET_RAIN,
  HOME_WIDGET_OUTDOOR
};

String cacheHomeSlots[HOME_SLOT_COUNT];

// =========================================================
// STATE
// =========================================================
enum Page {
  PAGE_HOME = 0,
  PAGE_WEATHER = 1,
  PAGE_NOTES = 2,
  PAGE_STATUS = 3,
  PAGE_SCHEDULE = 4,
  PAGE_HA = 5
};

// Nav order and labels (5 primary tabs — Status accessible via web UI)
const int NAV_COUNT = 5;
const Page NAV_PAGES[NAV_COUNT] = {PAGE_HOME, PAGE_SCHEDULE, PAGE_HA, PAGE_WEATHER, PAGE_NOTES};
const char* NAV_NAMES[NAV_COUNT] = {"Home", "Sched", "HA", "Wthr", "Quote"};

Page currentPage = PAGE_HOME;
Page lastDrawnPage = (Page)-1;

unsigned long lastClockTick = 0;
unsigned long lastDataTick  = 0;

const unsigned long CLOCK_TICK_MS = 1000;
const unsigned long DATA_TICK_MS  = 30UL * 1000UL;

bool pageDirty = true;
bool dataDirty = true;

// cache
String cacheClock = "";
String cacheTemp = "";
String cacheRain = "";
String cacheFocusTimer = "";
String cacheTimerMenu = "";
String cacheTimerDone = "";
String cacheTimerDoneCountdown = "";
String cacheTimerDoneFlash = "";

String lastWifiText = "";
String lastSignalText = "";
String lastIpText = "";
String lastUptimeText = "";
String lastTempText = "";
String lastRainText = "";
String lastUvText = "";
String lastUvLevelText = "";
String lastKpText = "";
String lastKpLevelText = "";
String lastWindText = "";
String lastWindDirText = "";
String lastNextSunLabel = "";
String lastNextSunTime = "";
String lastNotesText = "";
String lastNetworkToggleText = "";

const char* homeWidgetKey(HomeWidgetType type) {
  switch (type) {
    case HOME_WIDGET_WEEK:       return "week";
    case HOME_WIDGET_TIMER:      return "timer";
    case HOME_WIDGET_RAIN:       return "rain";
    case HOME_WIDGET_OUTDOOR:    return "outdoor";
    case HOME_WIDGET_KP:         return "kp";
    case HOME_WIDGET_UV:         return "uv";
    case HOME_WIDGET_WIND:       return "wind";
    case HOME_WIDGET_SUN:        return "sun";
    case HOME_WIDGET_HYDRATION:  return "hydration";
    case HOME_WIDGET_SCHEDULE:   return "schedule";
    case HOME_WIDGET_HA1:        return "ha1";
    case HOME_WIDGET_HA2:        return "ha2";
    case HOME_WIDGET_HA3:        return "ha3";
    case HOME_WIDGET_HA4:        return "ha4";
    case HOME_WIDGET_COUNTDOWN:  return "countdown";
    default:                     return "week";
  }
}

const char* homeWidgetLabel(HomeWidgetType type) {
  switch (type) {
    case HOME_WIDGET_WEEK:       return "Week";
    case HOME_WIDGET_TIMER:      return "Timer";
    case HOME_WIDGET_RAIN:       return "Rain";
    case HOME_WIDGET_OUTDOOR:    return "Outdoor";
    case HOME_WIDGET_KP:         return "KP index";
    case HOME_WIDGET_UV:         return "UV index";
    case HOME_WIDGET_WIND:       return "Wind";
    case HOME_WIDGET_SUN:        return "Sun event";
    case HOME_WIDGET_HYDRATION:  return "Hydration";
    case HOME_WIDGET_SCHEDULE:   return "Schedule";
    case HOME_WIDGET_HA1:        return "HA Sensor 1";
    case HOME_WIDGET_HA2:        return "HA Sensor 2";
    case HOME_WIDGET_HA3:        return "HA Sensor 3";
    case HOME_WIDGET_HA4:        return "HA Sensor 4";
    case HOME_WIDGET_COUNTDOWN:  return "Countdown";
    default:                     return "Week";
  }
}

HomeWidgetType homeWidgetFromKey(const String& key) {
  if (key == "week")       return HOME_WIDGET_WEEK;
  if (key == "timer")      return HOME_WIDGET_TIMER;
  if (key == "rain")       return HOME_WIDGET_RAIN;
  if (key == "outdoor")    return HOME_WIDGET_OUTDOOR;
  if (key == "kp")         return HOME_WIDGET_KP;
  if (key == "uv")         return HOME_WIDGET_UV;
  if (key == "wind")       return HOME_WIDGET_WIND;
  if (key == "sun")        return HOME_WIDGET_SUN;
  if (key == "hydration")  return HOME_WIDGET_HYDRATION;
  if (key == "schedule")   return HOME_WIDGET_SCHEDULE;
  if (key == "ha1")        return HOME_WIDGET_HA1;
  if (key == "ha2")        return HOME_WIDGET_HA2;
  if (key == "ha3")        return HOME_WIDGET_HA3;
  if (key == "ha4")        return HOME_WIDGET_HA4;
  if (key == "countdown")  return HOME_WIDGET_COUNTDOWN;
  return HOME_WIDGET_WEEK;
}

const char* homeSlotLabel(int slot) {
  switch (slot) {
    case 0: return "Top left";
    case 1: return "Top right";
    case 2: return "Bottom left";
    case 3: return "Bottom right";
    default: return "Slot";
  }
}

const char* timezonePosixByKey(const String& key) {
  if (key == "utc") return "UTC0";
  if (key == "atlantic_azores") return "AZOT1AZOST,M3.5.0/0,M10.5.0/1";
  if (key == "europe_west") return "WET0WEST,M3.5.0/1,M10.5.0";
  if (key == "uk") return "GMT0BST,M3.5.0/1,M10.5.0";
  if (key == "europe_central") return "CET-1CEST,M3.5.0/2,M10.5.0/3";
  if (key == "europe_east") return "EET-2EEST,M3.5.0/3,M10.5.0/4";
  if (key == "africa_south") return "SAST-2";
  if (key == "middle_east_gulf") return "GST-4";
  if (key == "india") return "IST-5:30";
  if (key == "thailand") return "ICT-7";
  if (key == "china") return "CST-8";
  if (key == "us_eastern") return "EST5EDT,M3.2.0/2,M11.1.0/2";
  if (key == "us_central") return "CST6CDT,M3.2.0/2,M11.1.0/2";
  if (key == "us_mountain") return "MST7MDT,M3.2.0/2,M11.1.0/2";
  if (key == "us_arizona") return "MST7";
  if (key == "us_pacific") return "PST8PDT,M3.2.0/2,M11.1.0/2";
  if (key == "alaska") return "AKST9AKDT,M3.2.0/2,M11.1.0/2";
  if (key == "hawaii") return "HST10";
  if (key == "canada_atlantic") return "AST4ADT,M3.2.0/2,M11.1.0/2";
  if (key == "brazil_east") return "BRT3";
  if (key == "argentina") return "ART3";
  if (key == "asia_tokyo") return "JST-9";
  if (key == "korea") return "KST-9";
  if (key == "australia_perth") return "AWST-8";
  if (key == "australia_darwin") return "ACST-9:30";
  if (key == "australia_sydney") return "AEST-10AEDT,M10.1.0,M4.1.0/3";
  if (key == "new_zealand") return "NZST-12NZDT,M9.5.0/2,M4.1.0/3";
  return "CET-1CEST,M3.5.0/2,M10.5.0/3";
}

const char* timezoneLabelByKey(const String& key) {
  if (key == "utc") return "UTC";
  if (key == "atlantic_azores") return "Azores";
  if (key == "europe_west") return "Western Europe";
  if (key == "uk") return "United Kingdom";
  if (key == "europe_central") return "Central Europe";
  if (key == "europe_east") return "Eastern Europe";
  if (key == "africa_south") return "South Africa";
  if (key == "middle_east_gulf") return "Gulf / UAE";
  if (key == "india") return "India";
  if (key == "thailand") return "Thailand / ICT";
  if (key == "china") return "China";
  if (key == "us_eastern") return "US Eastern";
  if (key == "us_central") return "US Central";
  if (key == "us_mountain") return "US Mountain";
  if (key == "us_arizona") return "US Arizona";
  if (key == "us_pacific") return "US Pacific";
  if (key == "alaska") return "Alaska";
  if (key == "hawaii") return "Hawaii";
  if (key == "canada_atlantic") return "Canada Atlantic";
  if (key == "brazil_east") return "Brazil East";
  if (key == "argentina") return "Argentina";
  if (key == "asia_tokyo") return "Japan";
  if (key == "korea") return "South Korea";
  if (key == "australia_perth") return "Australia West";
  if (key == "australia_darwin") return "Australia Central";
  if (key == "australia_sydney") return "Australia East";
  if (key == "new_zealand") return "New Zealand";
  return "Central Europe";
}

String sanitizeTimezoneKey(const String& key) {
  const char* supported[] = {
    "utc", "atlantic_azores", "europe_west", "uk", "europe_central", "europe_east",
    "africa_south", "middle_east_gulf", "india", "thailand", "china",
    "us_eastern", "us_central", "us_mountain", "us_arizona", "us_pacific",
    "alaska", "hawaii", "canada_atlantic", "brazil_east", "argentina",
    "asia_tokyo", "korea", "australia_perth", "australia_darwin",
    "australia_sydney", "new_zealand"
  };
  for (const char* supportedKey : supported) {
    if (key == supportedKey) return key;
  }
  return "europe_central";
}

void applyDeviceTimezoneByKey(const String& key) {
  timezoneKey = sanitizeTimezoneKey(key);
  setenv("TZ", timezonePosixByKey(timezoneKey), 1);
  tzset();
}

void appendTimezoneOptions(String& page, const String& selectedKey) {
  struct TimezoneGroup {
    const char* label;
    const char* keys[8];
    int count;
  };

  const TimezoneGroup groups[] = {
    {"Global", {"utc"}, 1},
    {"Europe", {"atlantic_azores", "europe_west", "uk", "europe_central", "europe_east"}, 5},
    {"Africa & Middle East", {"africa_south", "middle_east_gulf"}, 2},
    {"Asia", {"india", "thailand", "china", "asia_tokyo", "korea"}, 5},
    {"North America", {"us_eastern", "us_central", "us_mountain", "us_arizona", "us_pacific", "alaska", "hawaii", "canada_atlantic"}, 8},
    {"South America", {"brazil_east", "argentina"}, 2},
    {"Australia & Oceania", {"australia_perth", "australia_darwin", "australia_sydney", "new_zealand"}, 4}
  };

  for (const TimezoneGroup& group : groups) {
    page += "<optgroup label='";
    page += group.label;
    page += "'>";
    for (int i = 0; i < group.count; i++) {
      const char* key = group.keys[i];
      page += "<option value='";
      page += key;
      page += "'";
      if (selectedKey == key) page += " selected";
      page += ">";
      page += timezoneLabelByKey(key);
      page += "</option>";
    }
    page += "</optgroup>";
  }
}

void getHomeSlotRect(int slot, int& x, int& y, int& w, int& h) {
  const int xs[HOME_SLOT_COUNT] = {8, 124, 8, 124};
  const int ys[HOME_SLOT_COUNT] = {HOME_GRID_Y1, HOME_GRID_Y1, HOME_GRID_Y2, HOME_GRID_Y2};
  x = xs[slot];
  y = ys[slot];
  w = 108;
  h = HOME_WIDGET_H;
}

void appendHomeWidgetOptions(String& page, const String& selectedKey) {
  const HomeWidgetType types[] = {
    HOME_WIDGET_WEEK,
    HOME_WIDGET_TIMER,
    HOME_WIDGET_RAIN,
    HOME_WIDGET_OUTDOOR,
    HOME_WIDGET_KP,
    HOME_WIDGET_UV,
    HOME_WIDGET_WIND,
    HOME_WIDGET_SUN
  };

  for (HomeWidgetType type : types) {
    const char* key = homeWidgetKey(type);
    page += "<option value='";
    page += key;
    page += "'";
    if (selectedKey == key) page += " selected";
    page += ">";
    page += homeWidgetLabel(type);
    page += "</option>";
  }
}

void clearHomeSlotCaches() {
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    cacheHomeSlots[i] = "";
  }
}

// Focus timer
bool focusMenuOpen = false;
bool focusTimerRunning = false;
bool focusTimerFinished = false;
unsigned long focusEndMs = 0;
unsigned long focusDurationSec = 0;
unsigned long focusRemainingSec = 0;
bool timerDoneDialogOpen = false;
unsigned long timerDoneDialogStartedMs = 0;
const unsigned long TIMER_DONE_DIALOG_MS = 60UL * 1000UL;
bool flashModeEnabled = false;
int timerPresetMin[6] = {1, 5, 10, 15, 25, 30};

// Pomodoro
bool pomodoroEnabled   = false;
int  pomWorkMin        = 25;
int  pomBreakMin       = 5;
int  pomLongBreakMin   = 15;
bool pomInBreak        = false;
int  pomSessionCount   = 0;  // work sessions completed (0-3)
bool wifiEnabled = true;
bool wifiConnectInProgress = false;
unsigned long wifiConnectStartedMs = 0;
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000UL;

// Weather
static float tempC = NAN;
static float tempMinC = NAN;
static float tempMaxC = NAN;
static float precipMm = NAN;
static float windSpeedMs = NAN;
static float windDirectionDeg = NAN;
static float uvIndex = NAN;
static time_t lastWeatherFetch = 0;
static const uint32_t WEATHER_INTERVAL_SEC = 10 * 60;

// KP-index
static float kpIndex = NAN;
static time_t lastKpFetch = 0;
static const uint32_t KP_INTERVAL_SEC = 10 * 60;

// Sunrise / Sunset
static int sunriseMin = -1;
static int sunsetMin  = -1;
static int lastSunYmd = -1;
static time_t lastSyncTime = 0;

// =========================================================
// CLASS SCHEDULE (replaces habit tracker)
// =========================================================
const int SCHEDULE_DAYS  = 5;   // Monday..Friday
const int SCHEDULE_SLOTS = 6;   // max classes per day
String schedTime[SCHEDULE_DAYS][SCHEDULE_SLOTS];  // "HH:MM" or "" (empty = no class)
String schedName[SCHEDULE_DAYS][SCHEDULE_SLOTS];  // class name
String schedRoom[SCHEDULE_DAYS][SCHEDULE_SLOTS];  // classroom
String cacheSchedulePage = "";

const char* SCHEDULE_DAY_NAMES[SCHEDULE_DAYS] = {"周一","周二","周三","周四","周五"};

static int scheduleClassCount(int day) {
  if (day < 0 || day >= SCHEDULE_DAYS) return 0;
  int c = 0;
  for (int i = 0; i < SCHEDULE_SLOTS; i++) if (schedTime[day][i].length() > 0) c++;
  return c;
}

// =========================================================
// HYDRATION TRACKER
// =========================================================
int    hydrationCount   = 0;
int    hydrationGoal    = 8;
String hydrationDate    = "";  // "YYYYMMDD"
String cacheHydration   = "";

// =========================================================
// COUNTDOWN WIDGET
// =========================================================
String countdownLabel = "Event";
String countdownDate  = "";  // "YYYYMMDD"

// =========================================================
// STICKY NOTES API
// =========================================================
const int STICKY_COUNT = 3;
String stickyNote[STICKY_COUNT] = {"","",""};
bool   stickyDirty = false;

// =========================================================
// HOME ASSISTANT
// =========================================================
String haUrl   = "";
String haToken = "";

const int HA_SENSOR_COUNT = 4;
String haSensorEntity[HA_SENSOR_COUNT] = {"","","",""};
String haSensorLabel[HA_SENSOR_COUNT]  = {"Sensor 1","Sensor 2","Sensor 3","Sensor 4"};
String haSensorValue[HA_SENSOR_COUNT]  = {"--","--","--","--"};
String haSensorUnit[HA_SENSOR_COUNT]   = {"","","",""};

const int HA_CTRL_COUNT = 4;
String haCtrlEntity[HA_CTRL_COUNT] = {"","","",""};
String haCtrlLabel[HA_CTRL_COUNT]  = {"Control 1","Control 2","Control 3","Control 4"};
String haCtrlType[HA_CTRL_COUNT]   = {"switch","switch","scene","scene"};
bool   haCtrlState[HA_CTRL_COUNT]  = {};

int    haPageTab    = 0;  // 0=sensors 1=controls
time_t lastHaFetch  = 0;
const uint32_t HA_INTERVAL_SEC = 30;
String cacheHaPage  = "";

// =========================================================
// CONTEXT CLOCK
// =========================================================
String tz2Key   = "";   // posix key for secondary timezone, empty = disabled
String tz2Label = "";   // short display label, e.g. "NYC"

// =========================================================
// REMINDERS
// =========================================================
bool eyeBreakEnabled    = false;
int  eyeBreakIntervalMin = 20;
bool moveEnabled        = false;
int  moveIntervalMin    = 45;

unsigned long lastEyeBreakMs  = 0;
bool          eyeBreakActive  = false;
unsigned long eyeBreakStartMs = 0;
const unsigned long EYE_BREAK_DURATION_MS = 20000UL;

unsigned long lastMoveMs  = 0;
bool          moveActive  = false;

String cacheEyeBreak = "";
String cacheMove     = "";

// =========================================================
// SLEEP / BACKLIGHT
// =========================================================
const int BACKLIGHT_PIN = 21;

bool sleepDimmed = false;
bool sleepOff = false;
bool manualDimMode = false;

unsigned long lastInteractionMs = 0;

int sleepIntervalMin = 10;
int sleepOffDelaySec = 60;

const int BL_FULL = 255;
const int BL_DIM  = 18;
const int BL_OFF  = 0;
const int FLASH_BL_LOW = 20;
const int FLASH_BL_HIGH = 255;

void wakeDisplay(bool clearManualMode = true);
void setBacklight(int value);
void setWifiEnabled(bool enabled);
int sanitizeTimerMinutes(int value);
void drawHydrationWidget(int x, int y, int w, int h, String& cache, bool force);
void drawScheduleWidget(int x, int y, int w, int h, String& cache, bool force);
void drawCountdownWidget(int x, int y, int w, int h, String& cache, bool force);
void handleApiNote();
void handleApiNoteClear();
void handleApiHydrationReset();

// =========================================================
// HELPERS
// =========================================================
static int ymdFromLocal(time_t t) {
  struct tm tmLocal;
  localtime_r(&t, &tmLocal);
  return (tmLocal.tm_year + 1900) * 10000 + (tmLocal.tm_mon + 1) * 100 + tmLocal.tm_mday;
}

static int minutesFromLocalEpoch(time_t t) {
  struct tm tmLocal;
  localtime_r(&t, &tmLocal);
  return tmLocal.tm_hour * 60 + tmLocal.tm_min;
}

static int minutesNowLocal() {
  time_t now = time(nullptr);
  struct tm tmNow;
  localtime_r(&now, &tmNow);
  return tmNow.tm_hour * 60 + tmNow.tm_min;
}

static String wifiStatusText() {
  if (!wifiEnabled) return "Disabled";
  return WiFi.status() == WL_CONNECTED ? "Online" : "Offline";
}

static String signalText() {
  if (!wifiEnabled || WiFi.status() != WL_CONNECTED) return "-- dBm";
  return String(WiFi.RSSI()) + " dBm";
}

static String ipText() {
  if (!wifiEnabled || WiFi.status() != WL_CONNECTED) return "-";
  return WiFi.localIP().toString();
}

static bool useUsRegionFormat() {
  return regionFormatKey == "us";
}

static String formatClockParts(const struct tm& tmValue, bool withSeconds) {
  char buf[20];
  const char* pattern = useUsRegionFormat()
    ? (withSeconds ? "%I:%M:%S %p" : "%I:%M %p")
    : (withSeconds ? "%H:%M:%S" : "%H:%M");
  strftime(buf, sizeof(buf), pattern, &tmValue);
  return String(buf);
}

static String formatDateParts(const struct tm& tmValue) {
  char buf[32];
  strftime(buf, sizeof(buf), useUsRegionFormat() ? "%m/%d/%Y" : "%d.%m.%Y", &tmValue);
  return String(buf);
}

static String formatMinuteOfDay(int minOfDay) {
  if (minOfDay < 0) return "--:--";
  if (useUsRegionFormat()) {
    int hour24 = minOfDay / 60;
    int minute = minOfDay % 60;
    int hour12 = hour24 % 12;
    if (hour12 == 0) hour12 = 12;
    char buf[12];
    snprintf(buf, sizeof(buf), "%d:%02d %s", hour12, minute, hour24 >= 12 ? "PM" : "AM");
    return String(buf);
  }
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", minOfDay / 60, minOfDay % 60);
  return String(buf);
}

static String tempText() {
  if (isnan(tempC)) return unitKey == "imperial" ? "--.-F" : "--.-C";

  if (unitKey == "imperial") {
    float f = tempC * 9.0f / 5.0f + 32.0f;
    return String(f, 1) + "F";
  }

  return String(tempC, 1) + "C";
}

static String formatDisplayTemp(float value) {
  if (isnan(value)) return "--";

  if (unitKey == "imperial") {
    float f = value * 9.0f / 5.0f + 32.0f;
    return String((int)roundf(f)) + "F";
  }

  return String((int)roundf(value)) + "C";
}

static String tempRangeText() {
  return "H:" + formatDisplayTemp(tempMaxC) + "  L:" + formatDisplayTemp(tempMinC);
}

static String rainText() {
  if (isnan(precipMm)) return unitKey == "imperial" ? "--.--in" : "--.-mm";

  if (unitKey == "imperial") {
    float inches = precipMm / 25.4f;
    return String(inches, 2) + "in";
  }

  return String(precipMm, 1) + "mm";
}

static String windText() {
  if (isnan(windSpeedMs)) return unitKey == "imperial" ? "--.-mph" : "--.-m/s";

  if (unitKey == "imperial") {
    float mph = windSpeedMs * 2.236936f;
    return String(mph, 1) + "mph";
  }

  return String(windSpeedMs, 1) + "m/s";
}

static String windDirectionText() {
  if (isnan(windDirectionDeg)) return "--";

  const char* dirs[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  int idx = (int)roundf(windDirectionDeg / 45.0f) % 8;
  return String(dirs[idx]) + " " + String((int)roundf(windDirectionDeg)) + "deg";
}

static String kpText() {
  return isnan(kpIndex) ? "Kp --" : "Kp " + String(kpIndex, 1);
}

static String kpLevelText() {
  if (isnan(kpIndex)) return "--";
  if (kpIndex < 3.0f) return "Low";
  if (kpIndex < 5.0f) return "Medium";
  if (kpIndex < 7.0f) return "High";
  return "Extreme";
}

static String uvText() {
  return isnan(uvIndex) ? "UV --" : "UV " + String(uvIndex, 1);
}

static String uvLevelText() {
  if (isnan(uvIndex)) return "--";
  if (uvIndex < 3.0f) return "Low";
  if (uvIndex < 6.0f) return "Moderate";
  if (uvIndex < 8.0f) return "High";
  if (uvIndex < 11.0f) return "Very High";
  return "Extreme";
}

static uint16_t statusColor() {
  if (textColorKey != "standard") return COL_TEXT;
  if (!wifiEnabled) return COL_YELLOW;
  return WiFi.status() == WL_CONNECTED ? COL_GREEN : COL_RED;
}

static String uptimeText() {
  unsigned long seconds = millis() / 1000UL;
  unsigned long days = seconds / 86400UL;
  seconds %= 86400UL;
  unsigned long hours = seconds / 3600UL;
  seconds %= 3600UL;
  unsigned long minutes = seconds / 60UL;

  if (days > 0) return String(days) + "d " + String(hours) + "h";
  if (hours > 0) return String(hours) + "h " + String(minutes) + "m";
  return String(minutes) + "m";
}

static String nextSunLabel() {
  int nowMin = minutesNowLocal();
  if (sunriseMin < 0 || sunsetMin < 0) return "Sun";
  if (nowMin < sunriseMin) return "Sunrise";
  if (nowMin < sunsetMin) return "Sunset";
  return "Sunrise";
}

static String nextSunTimeText() {
  int nowMin = minutesNowLocal();
  if (sunriseMin < 0 || sunsetMin < 0) return "--:--";
  if (nowMin < sunriseMin) return formatMinuteOfDay(sunriseMin);
  if (nowMin < sunsetMin) return formatMinuteOfDay(sunsetMin);
  return formatMinuteOfDay(sunriseMin);
}

static String htmlEscape(const String& s) {
  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') out += "&amp;";
    else if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;";
    else if (c == '"') out += "&quot;";
    else out += c;
  }
  return out;
}

static String cssColorFrom565(uint16_t color) {
  uint8_t r = ((color >> 11) & 0x1F) * 255 / 31;
  uint8_t g = ((color >> 5) & 0x3F) * 255 / 63;
  uint8_t b = (color & 0x1F) * 255 / 31;
  char buf[8];
  snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
  return String(buf);
}

static String accentPreviewCss(const String& key) {
  if (key == "standard") return cssColorFrom565(0xEF7D);
  if (key == "ice")      return cssColorFrom565(0xEFFF);
  if (key == "white")    return cssColorFrom565(TFT_WHITE);
  if (key == "cyan")     return cssColorFrom565(0x5EFA);
  if (key == "mint")     return cssColorFrom565(0x07F0);
  if (key == "green")    return cssColorFrom565(TFT_GREEN);
  if (key == "blue")     return cssColorFrom565(0x3D9F);
  if (key == "purple")   return cssColorFrom565(0xA2F5);
  if (key == "pink")     return cssColorFrom565(0xF97F);
  if (key == "orange")   return cssColorFrom565(0xFD20);
  if (key == "amber")    return cssColorFrom565(0xFEA0);
  if (key == "red")      return cssColorFrom565(TFT_RED);
  return cssColorFrom565(0xEF7D);
}

static String themePreviewCss(const String& key) {
  if (key == "slate")    return cssColorFrom565(0x08A3);
  if (key == "deep")     return cssColorFrom565(0x0000);
  if (key == "nordic")   return cssColorFrom565(0x0864);
  if (key == "forest")   return cssColorFrom565(0x0208);
  if (key == "coffee")   return cssColorFrom565(0x18A3);
  if (key == "soft")     return cssColorFrom565(0x10A2);
  if (key == "midnight") return cssColorFrom565(0x0008);
  if (key == "graphite") return cssColorFrom565(0x1082);
  if (key == "garnet")   return cssColorFrom565(0x1004);
  if (key == "ochre")    return cssColorFrom565(0x20E1);
  return cssColorFrom565(0x08A3);
}

static String formatTimerClock(unsigned long totalSec) {
  unsigned long minutes = totalSec / 60UL;
  unsigned long seconds = totalSec % 60UL;

  char buf[10];
  snprintf(buf, sizeof(buf), "%02lu:%02lu", minutes, seconds);
  return String(buf);
}

static String focusHintText() {
  if (focusTimerFinished) return "Tap to reset";
  if (focusTimerRunning) return String((focusDurationSec / 60UL)) + " min session";
  return "Tap to start";
}

static String formatElapsedText(unsigned long totalSec) {
  unsigned long minutes = totalSec / 60UL;
  if (minutes == 0) return "< 1 minute elapsed";
  if (minutes == 1) return "1 minute elapsed";
  return String(minutes) + " minutes elapsed";
}

static String lastSyncText() {
  if (lastSyncTime <= 0) return "Sync --:--";

  struct tm tmSync;
  localtime_r(&lastSyncTime, &tmSync);
  return "Sync " + formatClockParts(tmSync, false);
}

static String weekNumberText() {
  time_t now = time(nullptr);
  struct tm tmNow;
  localtime_r(&now, &tmNow);
  char buf[4];
  strftime(buf, sizeof(buf), "%V", &tmNow);
  return String(buf);
}

static String timerDoneCountdownText() {
  if (!timerDoneDialogOpen) return "";

  unsigned long elapsedMs = millis() - timerDoneDialogStartedMs;
  unsigned long remainingMs = (elapsedMs >= TIMER_DONE_DIALOG_MS) ? 0 : (TIMER_DONE_DIALOG_MS - elapsedMs);
  unsigned long remainingSec = (remainingMs + 999UL) / 1000UL;
  return String("Auto close in ") + String(remainingSec) + "s";
}

static String homeTitleText() {
  return buddyNickname.length() > 0 ? buddyNickname : "Deskbuddy";
}

int sanitizeTimerMinutes(int value) {
  return constrain(value, 1, 180);
}

void resetFocusTimer() {
  focusTimerRunning = false;
  focusTimerFinished = false;
  focusMenuOpen = false;
  timerDoneDialogOpen = false;
  focusEndMs = 0;
  focusDurationSec = 0;
  focusRemainingSec = 0;
  cacheFocusTimer = "";
  clearHomeSlotCaches();
  cacheTimerMenu = "";
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  cacheTimerDoneFlash = "";
}

void startFocusTimer(unsigned long minutes) {
  focusDurationSec = minutes * 60UL;
  focusRemainingSec = focusDurationSec;
  focusEndMs = millis() + (focusDurationSec * 1000UL);
  focusTimerRunning = true;
  focusTimerFinished = false;
  focusMenuOpen = false;
  timerDoneDialogOpen = false;
  cacheFocusTimer = "";
  clearHomeSlotCaches();
  cacheTimerMenu = "";
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  cacheTimerDoneFlash = "";
}

void dismissTimerDoneDialog() {
  if (!timerDoneDialogOpen) return;
  timerDoneDialogOpen = false;
  timerDoneDialogStartedMs = 0;
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  cacheTimerDoneFlash = "";
  if (!sleepDimmed && !sleepOff) setBacklight(BL_FULL);
  pageDirty = true;
}

void openTimerDoneDialog() {
  timerDoneDialogOpen = true;
  timerDoneDialogStartedMs = millis();
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  wakeDisplay();
}

void updateFocusTimerState() {
  if (!focusTimerRunning) return;

  unsigned long now = millis();
  if ((long)(focusEndMs - now) <= 0) {
    focusRemainingSec = 0;
    focusTimerRunning = false;
    focusMenuOpen = false;
    cacheFocusTimer = "";
    clearHomeSlotCaches();
    cacheTimerMenu = "";

    if (pomodoroEnabled) {
      if (pomInBreak) {
        pomInBreak = false;
        startFocusTimer(pomWorkMin);
      } else {
        pomSessionCount++;
        pomInBreak = true;
        if (pomSessionCount >= 4) {
          pomSessionCount = 0;
          startFocusTimer(pomLongBreakMin);
        } else {
          startFocusTimer(pomBreakMin);
        }
      }
      return;
    }

    focusTimerFinished = true;
    openTimerDoneDialog();
    return;
  }

  unsigned long remainingMs = focusEndMs - now;
  unsigned long nextRemainingSec = (remainingMs + 999UL) / 1000UL;
  if (nextRemainingSec != focusRemainingSec) {
    focusRemainingSec = nextRemainingSec;
    cacheFocusTimer = "";
    clearHomeSlotCaches();
  }
}

void updateTimerDoneDialogState() {
  if (!timerDoneDialogOpen) return;
  if (millis() - timerDoneDialogStartedMs >= TIMER_DONE_DIALOG_MS) {
    dismissTimerDoneDialog();
  }
}

void setBacklight(int value) {
  value = constrain(value, 0, 255);
  tft.setBrightness(value);
}

void wakeDisplay(bool clearManualMode) {
  sleepDimmed = false;
  sleepOff = false;
  if (clearManualMode) manualDimMode = false;
  lastInteractionMs = millis();
  setBacklight(BL_FULL);
  pageDirty = true;
}

void enterSleepDim() {
  if (sleepDimmed || sleepOff || manualDimMode) return;
  sleepDimmed = true;
  setBacklight(BL_DIM);
}

void enterSleepOff() {
  if (sleepOff) return;
  sleepOff = true;
  sleepDimmed = true;
  setBacklight(BL_OFF);
  pageDirty = true;
}

void toggleSleepMode() {
  if (manualDimMode) {
    wakeDisplay();
    return;
  }

  manualDimMode = true;
  sleepDimmed = true;
  sleepOff = false;
  setBacklight(BL_DIM);
  pageDirty = true;
}

void handleAutoSleep() {
  if (focusMenuOpen || timerDoneDialogOpen) return;
  if (sleepIntervalMin <= 0) return;

  unsigned long now = millis();
  unsigned long dimAfterMs = (unsigned long)sleepIntervalMin * 60UL * 1000UL;
  unsigned long offAfterMs = dimAfterMs + ((unsigned long)sleepOffDelaySec * 1000UL);

  if (!sleepDimmed && !sleepOff && now - lastInteractionMs > dimAfterMs) {
    enterSleepDim();
  }

  if (sleepDimmed && !sleepOff && now - lastInteractionMs > offAfterMs) {
    enterSleepOff();
  }
}

// =========================================================
// THEME / SETTINGS
// =========================================================
void applyThemeByKey(const String& accentKey, const String& bgKey) {
  if (accentKey == "standard")    COL_ACCENT = 0x1082;
  else if (accentKey == "cyan")   COL_ACCENT = 0x2914;
  else if (accentKey == "ice")    COL_ACCENT = 0x0002;
  else if (accentKey == "white")  COL_ACCENT = TFT_BLACK;
  else if (accentKey == "mint")   COL_ACCENT = 0x781F;
  else if (accentKey == "green")  COL_ACCENT = 0xF81F;
  else if (accentKey == "blue")   COL_ACCENT = 0x0278;
  else if (accentKey == "purple") COL_ACCENT = 0x550B;
  else if (accentKey == "pink")   COL_ACCENT = 0x0680;
  else if (accentKey == "orange") COL_ACCENT = 0xFAC0;
  else if (accentKey == "amber")  COL_ACCENT = 0xF940;
  else if (accentKey == "red")    COL_ACCENT = 0xFFE0;
  else                            COL_ACCENT = 0x2914;

  if (bgKey == "slate") {
    COL_BG = 0xE75E; COL_PANEL = 0xCEFD; COL_PANEL_ALT = 0xC73C; COL_STROKE = 0x9E19;
  } else if (bgKey == "deep") {
    COL_BG = 0xFFFF; COL_PANEL = 0xF7BE; COL_PANEL_ALT = 0xEF7D; COL_STROKE = 0xD6BA;
  } else if (bgKey == "nordic") {
    COL_BG = 0xDF9E; COL_PANEL = 0xCF3D; COL_PANEL_ALT = 0xBEFC; COL_STROKE = 0x95D8;
  } else if (bgKey == "forest") {
    COL_BG = 0xBDFF; COL_PANEL = 0xA53E; COL_PANEL_ALT = 0x94BD; COL_STROKE = 0x6A9A;
  } else if (bgKey == "coffee") {
    COL_BG = 0xE75C; COL_PANEL = 0xD6BA; COL_PANEL_ALT = 0xC638; COL_STROKE = 0xB574;
  } else if (bgKey == "soft") {
    COL_BG = 0xEF5D; COL_PANEL = 0xDEDC; COL_PANEL_ALT = 0xD6BB; COL_STROKE = 0xB5B8;
  } else if (bgKey == "midnight") {
    COL_BG = 0xBFFF; COL_PANEL = 0x77FF; COL_PANEL_ALT = 0x3FFF; COL_STROKE = 0x0598;
  } else if (bgKey == "graphite") {
    COL_BG = 0xEF7D; COL_PANEL = 0xE73C; COL_PANEL_ALT = 0xDEFB; COL_STROKE = 0xBDF7;
  } else if (bgKey == "garnet") {
    COL_BG = 0xDFFD; COL_PANEL = 0xCF7C; COL_PANEL_ALT = 0xBF1B; COL_STROKE = 0x9E57;
  } else if (bgKey == "ochre") {
    COL_BG = 0xF71B; COL_PANEL = 0xDE79; COL_PANEL_ALT = 0xCDD7; COL_STROKE = 0xA4D3;
  } else {
    COL_BG = 0xE75E; COL_PANEL = 0xCEFD; COL_PANEL_ALT = 0xC73C; COL_STROKE = 0x9E19;
  }
}

void applyTextColorByKey(const String& key) {
  textColorKey = key;

  if (key == "standard") {
    COL_TEXT = 0x1082; COL_DIM  = 0x6B4D;
  } else if (key == "white") {
    COL_TEXT = TFT_BLACK; COL_DIM = 0x4208;
  } else if (key == "ice") {
    COL_TEXT = 0x0002; COL_DIM = 0x028C;
  } else if (key == "mint") {
    COL_TEXT = 0x781F; COL_DIM = 0x9A1F;
  } else if (key == "orange") {
    COL_TEXT = 0xFAC0; COL_DIM = 0xCDC8;
  } else if (key == "amber") {
    COL_TEXT = 0xF940; COL_DIM = 0xFB08;
  } else if (key == "green") {
    COL_TEXT = 0xF81F; COL_DIM = 0xB90F;
  } else if (key == "cyan") {
    COL_TEXT = 0x2914; COL_DIM = 0x4A78;
  } else if (key == "blue") {
    COL_TEXT = 0x0278; COL_DIM = 0x755B;
  } else if (key == "purple") {
    COL_TEXT = 0x550B; COL_DIM = 0x9610;
  } else if (key == "red") {
    COL_TEXT = 0xFFE0; COL_DIM = 0xC608;
  } else if (key == "pink") {
    COL_TEXT = 0x0680; COL_DIM = 0x7507;
  } else {
    COL_TEXT = 0x1082;
    COL_DIM  = 0x6B4D;
    textColorKey = "standard";
  }
}

void loadStoredSettings() {
  prefs.begin("deskbuddy", false);

  String accent = prefs.getString("accent", "cyan");
  String bg     = prefs.getString("bg", "slate");
  String txt    = prefs.getString("text", "standard");

  notesText        = prefs.getString("notes", "No notes yet.");
  quoteOnline      = prefs.getString("quoteOnline", "");
  quoteOnlineDate  = prefs.getString("quoteOnlineDate", "");
  buddyNickname    = prefs.getString("nickname", "");
  locationName     = prefs.getString("locname", DEFAULT_LOCATION);
  LAT              = prefs.getFloat("lat", DEFAULT_LAT);
  LNG              = prefs.getFloat("lng", DEFAULT_LNG);
  sleepIntervalMin = prefs.getInt("sleepMin", 10);
  unitKey          = prefs.getString("units", "metric");
  regionFormatKey  = prefs.getString("region", "europe");
  timezoneKey      = sanitizeTimezoneKey(prefs.getString("tz", "europe_central"));
  flashModeEnabled = prefs.getBool("flashMode", false);
  wifiEnabled      = prefs.getBool("wifiEnabled", true);

  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    String key = String("homeSlot") + String(i);
    homeWidgetSlots[i] = homeWidgetFromKey(prefs.getString(key.c_str(), homeWidgetKey(homeWidgetSlots[i])));
  }

  for (int i = 0; i < 6; i++) {
    String key = String("timer") + String(i);
    timerPresetMin[i] = sanitizeTimerMinutes(prefs.getInt(key.c_str(), timerPresetMin[i]));
  }

  if (unitKey != "metric" && unitKey != "imperial") unitKey = "metric";
  if (regionFormatKey != "europe" && regionFormatKey != "us") regionFormatKey = "europe";
  buddyNickname.trim();
  applyThemeByKey(accent, bg);
  applyTextColorByKey(txt);
  applyDeviceTimezoneByKey(timezoneKey);

  // Class schedule
  for (int d = 0; d < SCHEDULE_DAYS; d++) {
    for (int i = 0; i < SCHEDULE_SLOTS; i++) {
      String base = "sch" + String(d) + String(i);
      schedTime[d][i] = prefs.getString((base + "t").c_str(), "");
      schedName[d][i] = prefs.getString((base + "n").c_str(), "");
      schedRoom[d][i] = prefs.getString((base + "r").c_str(), "");
    }
  }
  cacheSchedulePage = "";

  // Context clock
  tz2Key   = prefs.getString("tz2", "");
  tz2Label = prefs.getString("tz2label", "");
  tz2Label.trim();

  // Reminders
  eyeBreakEnabled     = prefs.getBool("eyeBreak", false);
  eyeBreakIntervalMin = constrain(prefs.getInt("eyeBreakMin", 20), 5, 120);
  moveEnabled         = prefs.getBool("moveRemind", false);
  moveIntervalMin     = constrain(prefs.getInt("moveMin", 45), 5, 120);
  lastEyeBreakMs      = millis();
  lastMoveMs          = millis();

  // Hydration
  hydrationCount = prefs.getInt("hydCount", 0);
  hydrationGoal  = constrain(prefs.getInt("hydGoal", 8), 1, 20);
  hydrationDate  = prefs.getString("hydDate", "");

  // Countdown widget
  countdownLabel = prefs.getString("cdLabel", "Event");
  countdownDate  = prefs.getString("cdDate", "");

  // Pomodoro
  pomodoroEnabled  = prefs.getBool("pomEn", false);
  pomWorkMin       = constrain(prefs.getInt("pomWork", 25), 1, 90);
  pomBreakMin      = constrain(prefs.getInt("pomBreak", 5), 1, 30);
  pomLongBreakMin  = constrain(prefs.getInt("pomLong", 15), 5, 60);

  // Sticky notes
  for (int i = 0; i < STICKY_COUNT; i++) {
    stickyNote[i] = prefs.getString(("sticky" + String(i)).c_str(), "");
  }

  // Home Assistant
  haUrl   = prefs.getString("haUrl", "");
  haToken = prefs.getString("haToken", "");
  for (int i = 0; i < HA_SENSOR_COUNT; i++) {
    haSensorEntity[i] = prefs.getString(("haSe" + String(i)).c_str(), "");
    haSensorLabel[i]  = prefs.getString(("haSl" + String(i)).c_str(), "Sensor " + String(i+1));
    haSensorUnit[i]   = prefs.getString(("haSu" + String(i)).c_str(), "");
  }
  for (int i = 0; i < HA_CTRL_COUNT; i++) {
    haCtrlEntity[i] = prefs.getString(("haCe" + String(i)).c_str(), "");
    haCtrlLabel[i]  = prefs.getString(("haCl" + String(i)).c_str(), "Control " + String(i+1));
    haCtrlType[i]   = prefs.getString(("haCt" + String(i)).c_str(), "switch");
  }
}

void resetDataCaches() {
  tempC = NAN;
  precipMm = NAN;
  windSpeedMs = NAN;
  windDirectionDeg = NAN;
  kpIndex = NAN;
  sunriseMin = -1;
  sunsetMin = -1;
  lastSunYmd = -1;
  lastWeatherFetch = 0;
  lastKpFetch = 0;
  dataDirty = true;
  pageDirty = true;
}

// =========================================================
// TOUCH
// =========================================================
bool readTouchXY(int& sx, int& sy) {
  if (!ts.touched()) return false;

  TS_Point p = ts.getPoint();
  if (p.z < 80 || p.z > 4000) return false;

  int x = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_W);
  int y = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_H);

  x = constrain(x, 0, SCREEN_W - 1);
  y = constrain(y, 0, SCREEN_H - 1);

  if (TOUCH_SWAP_XY) { int tmp = x; x = y; y = tmp; }
  if (TOUCH_FLIP_X)  x = (SCREEN_W - 1) - x;
  if (TOUCH_FLIP_Y)  y = (SCREEN_H - 1) - y;

  sx = x;
  sy = y;
  return true;
}

bool touchNewPress(int& tx, int& ty) {
  static bool wasDown = false;
  static unsigned long lastPressMs = 0;

  bool down = false;
  int x = 0, y = 0;

  if (readTouchXY(x, y)) down = true;

  bool fire = false;
  unsigned long now = millis();

  if (down && !wasDown && (now - lastPressMs > 220)) {
    fire = true;
    lastPressMs = now;
    tx = x;
    ty = y;
  }

  wasDown = down;
  return fire;
}

// =========================================================
// API
// =========================================================
bool fetchSunriseSunset() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();

  String url = String("https://api.sunrise-sunset.org/json?lat=") + String(LAT, 4) +
               "&lng=" + String(LNG, 4) + "&formatted=0";

  HTTPClient http;
  if (!http.begin(client, url)) return false;

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, body)) return false;

  const char* sunriseStr = doc["results"]["sunrise"];
  const char* sunsetStr  = doc["results"]["sunset"];
  if (!sunriseStr || !sunsetStr) return false;

  auto parseIsoToEpochUTC = [](const char* iso) -> time_t {
    int Y, M, D, h, m, s;
    if (sscanf(iso, "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &s) != 6) return (time_t)-1;

    struct tm t{};
    t.tm_year = Y - 1900;
    t.tm_mon  = M - 1;
    t.tm_mday = D;
    t.tm_hour = h;
    t.tm_min  = m;
    t.tm_sec  = s;

    char* oldTz = getenv("TZ");
    String old = oldTz ? String(oldTz) : String("");

    setenv("TZ", "UTC0", 1);
    tzset();
    time_t epoch = mktime(&t);

    if (old.length()) setenv("TZ", old.c_str(), 1);
    else unsetenv("TZ");
    tzset();

    return epoch;
  };

  time_t srEpoch = parseIsoToEpochUTC(sunriseStr);
  time_t ssEpoch = parseIsoToEpochUTC(sunsetStr);
  if (srEpoch < 0 || ssEpoch < 0) return false;

  sunriseMin = minutesFromLocalEpoch(srEpoch);
  sunsetMin  = minutesFromLocalEpoch(ssEpoch);
  lastSunYmd = ymdFromLocal(time(nullptr));
  lastSyncTime = time(nullptr);
  return true;
}

void ensureSunTimesForToday() {
  time_t nowT = time(nullptr);
  int ymd = ymdFromLocal(nowT);

  if ((sunriseMin < 0 || sunsetMin < 0 || ymd != lastSunYmd) &&
      WiFi.status() == WL_CONNECTED) {
    if (fetchSunriseSunset()) dataDirty = true;
  }
}

bool fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();

  String url = String("https://api.open-meteo.com/v1/forecast?latitude=") + String(LAT, 4) +
               "&longitude=" + String(LNG, 4) +
               "&current=temperature_2m,wind_speed_10m,wind_direction_10m,uv_index" +
               "&hourly=precipitation" +
               "&daily=temperature_2m_max,temperature_2m_min" +
               "&forecast_days=1&timezone=auto&wind_speed_unit=ms";

  HTTPClient http;
  if (!http.begin(client, url)) return false;

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, body)) return false;

  tempC = doc["current"]["temperature_2m"] | NAN;
  windSpeedMs = doc["current"]["wind_speed_10m"] | NAN;
  windDirectionDeg = doc["current"]["wind_direction_10m"] | NAN;
  uvIndex = doc["current"]["uv_index"] | NAN;
  tempMaxC = NAN;
  tempMinC = NAN;

  JsonArray maxTemps = doc["daily"]["temperature_2m_max"];
  JsonArray minTemps = doc["daily"]["temperature_2m_min"];
  if (maxTemps && !maxTemps.isNull() && maxTemps.size() > 0) tempMaxC = maxTemps[0] | NAN;
  if (minTemps && !minTemps.isNull() && minTemps.size() > 0) tempMinC = minTemps[0] | NAN;

  JsonArray times = doc["hourly"]["time"];
  JsonArray precs = doc["hourly"]["precipitation"];

  if (times && precs) {
    time_t nowT = time(nullptr);
    struct tm tmNow;
    localtime_r(&nowT, &tmNow);

    char key[20];
    strftime(key, sizeof(key), "%Y-%m-%dT%H:00", &tmNow);

    int idx = -1;
    for (int i = 0; i < (int)times.size(); i++) {
      const char* t = times[i];
      if (t && String(t).startsWith(key)) {
        idx = i;
        break;
      }
    }
    if (idx < 0) idx = 0;
    precipMm = precs[idx] | NAN;
  }

  lastWeatherFetch = time(nullptr);
  lastSyncTime = lastWeatherFetch;
  return true;
}

void ensureWeather() {
  time_t nowT = time(nullptr);
  if ((isnan(tempC) || isnan(tempMinC) || isnan(tempMaxC) || isnan(precipMm) || isnan(windSpeedMs) || isnan(windDirectionDeg) || isnan(uvIndex) ||
       (nowT - lastWeatherFetch) > WEATHER_INTERVAL_SEC) &&
      WiFi.status() == WL_CONNECTED) {
    if (fetchWeather()) dataDirty = true;
  }
}

bool fetchKpIndex() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, "https://services.swpc.noaa.gov/products/noaa-planetary-k-index.json")) {
    return false;
  }

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  int lastRow = body.lastIndexOf('[');
  if (lastRow < 0) return false;

  int firstComma = body.indexOf(',', lastRow);
  if (firstComma < 0) return false;

  int q1 = body.indexOf('"', firstComma);
  if (q1 < 0) return false;
  int q2 = body.indexOf('"', q1 + 1);
  if (q2 < 0) return false;

  String kpStrLocal = body.substring(q1 + 1, q2);
  kpIndex = kpStrLocal.toFloat();

  lastKpFetch = time(nullptr);
  lastSyncTime = lastKpFetch;
  return true;
}

void ensureKpIndex() {
  time_t nowT = time(nullptr);
  if ((isnan(kpIndex) || (nowT - lastKpFetch) > KP_INTERVAL_SEC) &&
      WiFi.status() == WL_CONNECTED) {
    if (fetchKpIndex()) dataDirty = true;
  }
}

// =========================================================
// DRAW HELPERS
// =========================================================
void drawCard(int x, int y, int w, int h, bool accent = false) {
  tft.fillRoundRect(x, y, w, h, 10, COL_PANEL);
  tft.drawRoundRect(x, y, w, h, 10, accent ? COL_ACCENT : COL_STROKE);
}

void drawTopBar(const String& title) {
  tft.fillRect(0, 0, SCREEN_W, TOPBAR_H, COL_PANEL_ALT);
  tft.drawFastHLine(0, TOPBAR_H - 1, SCREEN_W, COL_STROKE);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_TEXT, COL_PANEL_ALT);
  tft.drawString(title, 10, 9, 2);

  const int bs = 25;
  const int by = 4;

  // Power button (rightmost)
  const int bx = SCREEN_W - bs - 6;
  uint16_t pwrBg = (sleepDimmed || sleepOff) ? COL_ACCENT : COL_PANEL;
  uint16_t pwrFg = (sleepDimmed || sleepOff) ? TFT_BLACK : COL_TEXT;
  tft.fillRoundRect(bx, by, bs, bs, 7, pwrBg);
  tft.drawRoundRect(bx, by, bs, bs, 7, COL_ACCENT);
  const int cx = bx + 12;
  const int cy = by + 12;
  tft.drawCircle(cx, cy + 1, 6, pwrFg);
  tft.drawFastHLine(cx - 4, cy - 5, 9, pwrBg);
  tft.drawFastVLine(cx, cy - 7, 6, pwrFg);

  // Info / Status button (left of power button)
  const int ix = bx - bs - 4;
  bool onStatus = (currentPage == PAGE_STATUS);
  uint16_t infoBg = onStatus ? COL_ACCENT : COL_PANEL;
  uint16_t infoFg = onStatus ? TFT_BLACK : COL_TEXT;
  tft.fillRoundRect(ix, by, bs, bs, 7, infoBg);
  tft.drawRoundRect(ix, by, bs, bs, 7, COL_ACCENT);
  const int icx = ix + 12;
  const int icy = by + 12;
  tft.fillCircle(icx, icy - 5, 2, infoFg);
  tft.fillRect(icx - 1, icy - 1, 3, 9, infoFg);
}

void drawNavBar() {
  const int y = SCREEN_H - NAV_H;
  tft.fillRect(0, y, SCREEN_W, NAV_H, COL_PANEL_ALT);
  tft.drawFastHLine(0, y, SCREEN_W, COL_STROKE);

  const int btnW = SCREEN_W / NAV_COUNT;

  for (int i = 0; i < NAV_COUNT; i++) {
    int bx = i * btnW;
    bool active = (currentPage == NAV_PAGES[i]);

    uint16_t bg = active ? COL_ACCENT : COL_PANEL;
    uint16_t fg = active ? TFT_BLACK : COL_TEXT;

    tft.fillRoundRect(bx + 2, y + 5, btnW - 4, NAV_H - 10, 7, bg);
    tft.drawRoundRect(bx + 2, y + 5, btnW - 4, NAV_H - 10, 7, active ? COL_ACCENT : COL_STROKE);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(fg, bg);
    tft.drawString(NAV_NAMES[i], bx + btnW / 2, y + NAV_H / 2, 1);
  }

  tft.setTextDatum(TL_DATUM);
}

void makeSpriteCard(TFT_eSprite& spr, int w, int h, bool accent = false) {
  spr.setColorDepth(16);
  spr.createSprite(w, h);
  spr.fillSprite(COL_BG);
  spr.fillRoundRect(0, 0, w, h, 10, COL_PANEL);
  spr.drawRoundRect(0, 0, w, h, 10, accent ? COL_ACCENT : COL_STROKE);
}

void pushSpriteAndDelete(TFT_eSprite& spr, int x, int y) {
  spr.pushSprite(x, y, COL_BG);
  spr.deleteSprite();
}

void drawCleanSunIcon(TFT_eSprite& spr, int cx, int cy, uint16_t c) {
  spr.fillCircle(cx, cy, 4, c);
  spr.drawLine(cx, cy - 9, cx, cy - 7, c);
  spr.drawLine(cx, cy + 7, cx, cy + 9, c);
  spr.drawLine(cx - 9, cy, cx - 7, cy, c);
  spr.drawLine(cx + 7, cy, cx + 9, cy, c);
  spr.drawLine(cx - 6, cy - 6, cx - 5, cy - 5, c);
  spr.drawLine(cx + 5, cy - 5, cx + 6, cy - 6, c);
  spr.drawLine(cx - 6, cy + 6, cx - 5, cy + 5, c);
  spr.drawLine(cx + 5, cy + 5, cx + 6, cy + 6, c);
}

void drawMoonIcon(TFT_eSprite& spr, int cx, int cy, uint16_t c) {
  spr.fillCircle(cx, cy, 6, c);
  spr.fillCircle(cx + 4, cy - 2, 6, COL_PANEL);
}

int drawWrappedTextLimited(int x, int y, int maxW, const String& text, int font, uint16_t fg, uint16_t bg, int maxLines) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(fg, bg);

  const int lineH = tft.fontHeight((uint8_t)font) + 2;
  String line = "";
  String word = "";
  int linesDrawn = 0;

  auto flushLine = [&]() {
    if (linesDrawn >= maxLines) return;
    if (line.length() > 0) tft.drawString(line, x, y, font);
    y += lineH;
    line = "";
    linesDrawn++;
  };

  auto placeWordOnEmptyLine = [&]() {
    if (word.length() == 0 || linesDrawn >= maxLines) return;

    while (tft.textWidth(word.c_str(), lgfxFont(font)) > maxW && word.length() > 1) {
      int cut = word.length();
      while (cut > 1 && tft.textWidth(word.substring(0, cut).c_str(), lgfxFont(font)) > maxW) cut--;
      if (linesDrawn >= maxLines) return;
      tft.drawString(word.substring(0, cut), x, y, font);
      y += lineH;
      linesDrawn++;
      word = word.substring(cut);
    }

    if (linesDrawn < maxLines) {
      line = word;
      word = "";
    }
  };

  auto flushWord = [&]() {
    if (word.length() == 0 || linesDrawn >= maxLines) return;

    if (line.length() == 0) {
      placeWordOnEmptyLine();
      return;
    }

    String candidate = line + " " + word;
    if (tft.textWidth(candidate.c_str(), lgfxFont(font)) <= maxW) {
      line = candidate;
      word = "";
      return;
    }

    flushLine();
    placeWordOnEmptyLine();
  };

  for (int i = 0; i < (int)text.length(); i++) {
    if (linesDrawn >= maxLines) break;
    char c = text[i];

    if (c == '\n') {
      flushWord();
      flushLine();
      continue;
    }

    if (c == ' ') {
      flushWord();
      continue;
    }

    word += c;
  }

  if (linesDrawn < maxLines) {
    flushWord();
    if (line.length() > 0) flushLine();
  }

  return y;
}

// =========================================================
// HOME SPRITES
// =========================================================
static String weatherAdvice() {
  String parts[7];
  int n = 0;

  // 降水（大雨优先）
  if (!isnan(precipMm) && precipMm >= 10.0f) parts[n++] = "大雨带伞";
  else if (!isnan(precipMm) && precipMm > 0.5f) parts[n++] = "有雨带伞";

  // 温度极端
  if (!isnan(tempMaxC) && tempMaxC >= 35.0f) parts[n++] = "高温防暑";
  else if (!isnan(tempMinC) && tempMinC <= 0.0f) parts[n++] = "低温保暖";

  // 温差（优先级高于大风）
  if (!isnan(tempMinC) && !isnan(tempMaxC) && (tempMaxC - tempMinC) >= 8.0f) parts[n++] = "温差大";

  // 风
  if (!isnan(windSpeedMs) && windSpeedMs >= 10.8f) parts[n++] = "大风";

  // 紫外线
  if (!isnan(uvIndex) && uvIndex >= 6.0f) parts[n++] = "防晒";

  if (n == 0) return "";
  if (n > 2) n = 2;  // 最多显示 2 条，避免超出宽度

  String adv = parts[0];
  for (int i = 1; i < n; i++) { adv += " · "; adv += parts[i]; }
  return adv;
}

void drawClockCardSprite(bool force = false) {
  const int x = 8, y = PAGE_ROW1_Y, w = 224, h = HOME_WIDGET_H;

  time_t now = time(nullptr);
  struct tm tmNow;
  localtime_r(&now, &tmNow);

  String timeBuf = formatClockParts(tmNow, true);
  String dateBuf = formatDateParts(tmNow);

  String sr = formatMinuteOfDay(sunriseMin);
  String ss = formatMinuteOfDay(sunsetMin);

  String weekBuf = "W" + weekNumberText();
  String combined = timeBuf + "|" + dateBuf + "|" + weekBuf + "|" + sr + "|" + ss + "|" +
                    tz2Key + "|" + tz2Label + "|" +
                    String(COL_ACCENT) + "|" + String(COL_TEXT);

  if (!force && combined == cacheClock) return;
  cacheClock = combined;

  makeSpriteCard(sprClock, w, h, true);

  sprClock.setTextDatum(TL_DATUM);

  sprClock.setTextColor(COL_TEXT, COL_PANEL);
  if (useUsRegionFormat()) {
    int splitAt = timeBuf.lastIndexOf(' ');
    String clockMain = splitAt > 0 ? timeBuf.substring(0, splitAt) : timeBuf;
    String clockSuffix = splitAt > 0 ? timeBuf.substring(splitAt + 1) : "";
    sprClock.drawString(clockMain, 10, 11, 4);
    if (clockSuffix.length() > 0) {
      int suffixX = 10 + sprClock.textWidth(clockMain.c_str(), lgfxFont(4)) + 4;
      sprClock.drawString(clockSuffix, suffixX, 18, 2);
    }
  } else {
    sprClock.drawString(timeBuf, 10, 11, 4);
  }

  sprClock.setTextColor(COL_DIM, COL_PANEL);
  sprClock.drawString(dateBuf, 10, 36, 2);
  int dateTextW = sprClock.textWidth(dateBuf.c_str(), lgfxFont(2));
  sprClock.drawString(weekBuf, 10 + dateTextW + 7, 36, 2);

  if (tz2Key.length() > 0 && tz2Label.length() > 0) {
    const char* posix2 = timezonePosixByKey(tz2Key);
    setenv("TZ", posix2, 1); tzset();
    struct tm tm2; localtime_r(&now, &tm2);
    setenv("TZ", timezonePosixByKey(timezoneKey), 1); tzset();
    String t2Buf = formatClockParts(tm2, false);
    sprClock.setTextColor(COL_ACCENT, COL_PANEL);
    sprClock.drawString(tz2Label + " " + t2Buf, 10, 52, 1);
  } else {
    String advice = weatherAdvice();
    if (advice.length() > 0) {
      sprClock.setFont(&lgfx::fonts::efontCN_12);
      sprClock.setTextColor(COL_ACCENT, COL_PANEL);
      sprClock.drawString(advice, 10, 54);
    }
  }

  drawCleanSunIcon(sprClock, 151, 22, COL_ACCENT);
  drawMoonIcon(sprClock, 151, 50, COL_ACCENT);

  sprClock.setTextColor(COL_ACCENT, COL_PANEL);
  sprClock.drawString(sr, 165, 15, 2);
  sprClock.drawString(ss, 165, 43, 2);

  pushSpriteAndDelete(sprClock, x, y);
}

void drawWeatherStyleMetricSprite(int x, int y, int w, int h, const char* label, const String& value, String& cache, bool force = false, const String& detail = "") {
  String combined = String(label) + "|" + value + "|" + detail + "|" + String(COL_PANEL) + "|" +
                    String(COL_STROKE) + "|" + String(COL_TEXT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString(label, 10, 8, 2);

  sprSmall.setTextColor(COL_TEXT, COL_PANEL);
  sprSmall.drawString(value, 10, 28, 4);

  if (detail.length() > 0) {
    sprSmall.setTextColor(COL_ACCENT, COL_PANEL);
    sprSmall.drawString(detail, 10, 52, 1);
  }

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawSunEventWidget(int x, int y, int w, int h, String& cache, bool force = false) {
  String label = nextSunLabel();
  String value = nextSunTimeText();
  String combined = label + "|" + value + "|" + String(COL_PANEL) + "|" +
                    String(COL_STROKE) + "|" + String(COL_TEXT) + "|" + String(COL_ACCENT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString(label, 10, 8, 2);

  sprSmall.setTextColor(COL_TEXT, COL_PANEL);
  if (useUsRegionFormat()) {
    int splitAt = value.lastIndexOf(' ');
    String mainValue = splitAt > 0 ? value.substring(0, splitAt) : value;
    String suffix = splitAt > 0 ? value.substring(splitAt + 1) : "";
    sprSmall.drawString(mainValue, 10, 30, 4);
    if (suffix.length() > 0) {
      int suffixX = 10 + sprSmall.textWidth(mainValue.c_str(), lgfxFont(4)) + 3;
      sprSmall.drawString(suffix, suffixX, 35, 2);
    }
  } else {
    sprSmall.drawString(value, 10, 30, 4);
  }

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawFocusTimerWidget(int x, int y, int w, int h, String& cache, bool force = false) {
  String value = formatTimerClock(focusRemainingSec);
  String hint = focusHintText();
  String combined = value + "|" + hint + "|" + String(focusMenuOpen ? 1 : 0) +
                    "|" + String(COL_PANEL) + "|" + String(COL_ACCENT) + "|" + String(COL_TEXT) +
                    "|" + String(pomodoroEnabled ? 1 : 0) + "|" + String(pomInBreak ? 1 : 0) + "|" + String(pomSessionCount);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);
  sprSmall.setTextDatum(TL_DATUM);

  if (pomodoroEnabled) {
    sprSmall.setTextColor(pomInBreak ? COL_GREEN : COL_ACCENT, COL_PANEL);
    sprSmall.drawString(pomInBreak ? "BREAK" : "WORK", 10, 8, 2);
  } else {
    sprSmall.setTextColor(COL_DIM, COL_PANEL);
    sprSmall.drawString("Timer", 10, 8, 2);
  }

  if (focusMenuOpen && !pomodoroEnabled) {
    sprSmall.setTextColor(COL_TEXT, COL_PANEL);
    sprSmall.drawString("Select", 10, 22, 4);
    sprSmall.setTextColor(COL_DIM, COL_PANEL);
    sprSmall.drawString("duration", 10, 44, 2);
  } else if (pomodoroEnabled && !focusTimerRunning && !focusTimerFinished) {
    sprSmall.setTextColor(COL_DIM, COL_PANEL);
    sprSmall.drawString("Tap to", 10, 26, 2);
    sprSmall.drawString("start", 10, 42, 2);
  } else {
    sprSmall.setTextColor(focusTimerFinished ? COL_GREEN : COL_TEXT, COL_PANEL);
    sprSmall.drawString(value, 10, 24, 4);
    if (pomodoroEnabled) {
      for (int i = 0; i < 4; i++) {
        uint16_t dc = (i < pomSessionCount % 4) ? COL_ACCENT : COL_STROKE;
        sprSmall.fillCircle(10 + i * 12, 60, 4, dc);
      }
    } else {
      sprSmall.setTextColor(COL_DIM, COL_PANEL);
      sprSmall.drawString(hint, 10, 49, 1);
    }
  }

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawHomeSlotWidget(int slot, bool force = false) {
  int x, y, w, h;
  getHomeSlotRect(slot, x, y, w, h);

  switch (homeWidgetSlots[slot]) {
    case HOME_WIDGET_WEEK:
      drawWeatherStyleMetricSprite(x, y, w, h, "Week", weekNumberText(), cacheHomeSlots[slot], force);
      break;
    case HOME_WIDGET_TIMER:
      drawFocusTimerWidget(x, y, w, h, cacheHomeSlots[slot], force);
      break;
    case HOME_WIDGET_RAIN:
      drawWeatherStyleMetricSprite(x, y, w, h, "Rain", rainText(), cacheHomeSlots[slot], force);
      break;
    case HOME_WIDGET_OUTDOOR:
      drawWeatherStyleMetricSprite(x, y, w, h, "Outdoor", tempText(), cacheHomeSlots[slot], force, tempRangeText());
      break;
    case HOME_WIDGET_KP:
      drawWeatherStyleMetricSprite(x, y, w, h, "KP index", kpText(), cacheHomeSlots[slot], force, kpLevelText());
      break;
    case HOME_WIDGET_UV:
      drawWeatherStyleMetricSprite(x, y, w, h, "UV index", uvText(), cacheHomeSlots[slot], force, uvLevelText());
      break;
    case HOME_WIDGET_WIND:
      drawWeatherStyleMetricSprite(x, y, w, h, "Wind", windText(), cacheHomeSlots[slot], force, windDirectionText());
      break;
    case HOME_WIDGET_SUN:
      drawSunEventWidget(x, y, w, h, cacheHomeSlots[slot], force);
      break;
    case HOME_WIDGET_HYDRATION:
      drawHydrationWidget(x, y, w, h, cacheHomeSlots[slot], force);
      break;
    case HOME_WIDGET_SCHEDULE:
      drawScheduleWidget(x, y, w, h, cacheHomeSlots[slot], force);
      break;
    case HOME_WIDGET_HA1:
    case HOME_WIDGET_HA2:
    case HOME_WIDGET_HA3:
    case HOME_WIDGET_HA4: {
      int hi = homeWidgetSlots[slot] - HOME_WIDGET_HA1;
      String lbl = haSensorLabel[hi].length() > 0 ? haSensorLabel[hi] : (String("HA ") + String(hi + 1));
      String val = haSensorValue[hi].length() > 0 ? haSensorValue[hi] : "--";
      drawWeatherStyleMetricSprite(x, y, w, h, lbl.c_str(), val, cacheHomeSlots[slot], force, haSensorUnit[hi]);
      break;
    }
    case HOME_WIDGET_COUNTDOWN:
      drawCountdownWidget(x, y, w, h, cacheHomeSlots[slot], force);
      break;
  }
}

void drawFocusMenuOverlay(bool force = false) {
  String combined = String(focusTimerRunning ? 1 : 0) + "|" + String(focusTimerFinished ? 1 : 0) +
                    "|" + String(COL_PANEL_ALT) + "|" + String(COL_PANEL) + "|" + String(COL_ACCENT);
  if (!force && combined == cacheTimerMenu) return;
  cacheTimerMenu = combined;

  tft.fillRect(0, 0, SCREEN_W, SCREEN_H, COL_BG);
  tft.fillRoundRect(TIMER_MENU_X, TIMER_MENU_Y, TIMER_MENU_W, TIMER_MENU_H, 12, COL_PANEL_ALT);
  tft.drawRoundRect(TIMER_MENU_X, TIMER_MENU_Y, TIMER_MENU_W, TIMER_MENU_H, 12, COL_ACCENT);

  tft.setTextColor(COL_TEXT, COL_PANEL_ALT);
  tft.drawString("Start timer", TIMER_MENU_X + 14, TIMER_MENU_Y + 12, 2);
  tft.setTextColor(COL_DIM, COL_PANEL_ALT);
  tft.drawString("Choose a session length", TIMER_MENU_X + 14, TIMER_MENU_Y + 34, 1);

  const int btnW = 74;
  const int btnH = 28;
  const int col1X = TIMER_MENU_X + 14;
  const int col2X = TIMER_MENU_X + 112;
  const int row1Y = TIMER_MENU_Y + 54;
  const int row2Y = TIMER_MENU_Y + 88;
  const int row3Y = TIMER_MENU_Y + 122;

  String labels[6];
  const int xs[] = {col1X, col2X, col1X, col2X, col1X, col2X};
  const int ys[] = {row1Y, row1Y, row2Y, row2Y, row3Y, row3Y};
  for (int i = 0; i < 6; i++) {
    labels[i] = String(timerPresetMin[i]) + " min";
  }

  for (int i = 0; i < 6; i++) {
    tft.fillRoundRect(xs[i], ys[i], btnW, btnH, 8, COL_PANEL);
    tft.drawRoundRect(xs[i], ys[i], btnW, btnH, 8, COL_STROKE);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawCentreString(labels[i].c_str(), xs[i] + btnW / 2, ys[i] + 7, 2);
  }

  const int actionY = TIMER_MENU_Y + 160;
  const int actionW = 152;
  const int actionX = TIMER_MENU_X + 24;
  const char* actionLabel = focusTimerRunning ? "Stop" : (focusTimerFinished ? "Reset" : nullptr);
  uint16_t actionColor = focusTimerRunning ? COL_RED : COL_ACCENT;

  if (actionLabel) {
    tft.fillRoundRect(actionX, actionY - 4, actionW, 26, 8, COL_PANEL);
    tft.drawRoundRect(actionX, actionY - 4, actionW, 26, 8, actionColor);
    tft.setTextColor(actionColor, COL_PANEL);
    tft.drawCentreString(actionLabel, actionX + actionW / 2, actionY + 4, 2);
  } else {
    tft.setTextColor(COL_DIM, COL_PANEL_ALT);
    tft.drawCentreString("Tap outside to close", TIMER_MENU_X + TIMER_MENU_W / 2, TIMER_MENU_Y + TIMER_MENU_H - 13, 1);
  }
}

void drawTimerDoneOverlay(bool force = false) {
  if (!timerDoneDialogOpen) return;

  String elapsed = formatElapsedText(focusDurationSec);
  String countdown = timerDoneCountdownText();
  bool flashOn = flashModeEnabled && ((millis() / 300UL) % 2UL == 0);
  setBacklight(flashModeEnabled ? (flashOn ? FLASH_BL_HIGH : FLASH_BL_LOW) : BL_FULL);
  String combined = elapsed + "|" + String(COL_PANEL_ALT) + "|" + String(COL_ACCENT) + "|" + String(COL_TEXT);
  String flashKey = String(flashOn ? 1 : 0);
  if (force || combined != cacheTimerDone || flashKey != cacheTimerDoneFlash) {
    cacheTimerDone = combined;
    cacheTimerDoneFlash = flashKey;
    cacheTimerDoneCountdown = "";

    uint16_t backdrop = flashOn ? COL_ACCENT : COL_BG;
    uint16_t panelBorder = flashOn ? TFT_WHITE : COL_ACCENT;
    tft.fillRect(0, 0, SCREEN_W, SCREEN_H, backdrop);
    tft.fillRoundRect(TIMER_DONE_X, TIMER_DONE_Y, TIMER_DONE_W, TIMER_DONE_H, 12, COL_PANEL_ALT);
    tft.drawRoundRect(TIMER_DONE_X, TIMER_DONE_Y, TIMER_DONE_W, TIMER_DONE_H, 12, panelBorder);

    tft.setTextColor(COL_TEXT, COL_PANEL_ALT);
    tft.drawCentreString("Timer complete", TIMER_DONE_X + TIMER_DONE_W / 2, TIMER_DONE_Y + 14, 2);
    tft.setTextColor(COL_ACCENT, COL_PANEL_ALT);
    tft.drawCentreString(elapsed, TIMER_DONE_X + TIMER_DONE_W / 2, TIMER_DONE_Y + 42, 2);
    tft.setTextColor(COL_DIM, COL_PANEL_ALT);
    tft.drawCentreString("Tap anywhere to acknowledge", TIMER_DONE_X + TIMER_DONE_W / 2, TIMER_DONE_Y + 68, 1);
  }

  if (force || countdown != cacheTimerDoneCountdown) {
    cacheTimerDoneCountdown = countdown;
    tft.fillRect(TIMER_DONE_X + 28, TIMER_DONE_Y + 82, TIMER_DONE_W - 56, 12, COL_PANEL_ALT);
    tft.setTextColor(COL_DIM, COL_PANEL_ALT);
    tft.drawCentreString(countdown.c_str(), TIMER_DONE_X + TIMER_DONE_W / 2, TIMER_DONE_Y + 82, 1);
  }
}

// =========================================================
// REMINDER OVERLAYS
// =========================================================
void drawEyeBreakOverlay() {
  unsigned long elapsed = millis() - eyeBreakStartMs;
  int remaining = max(0, (int)((EYE_BREAK_DURATION_MS - elapsed) / 1000) + 1);
  String key = String(remaining);
  if (key == cacheEyeBreak) return;
  cacheEyeBreak = key;

  tft.fillScreen(COL_BG);
  tft.fillRoundRect(20, 80, 200, 160, 14, COL_PANEL_ALT);
  tft.drawRoundRect(20, 80, 200, 160, 14, COL_ACCENT);

  tft.setTextColor(COL_ACCENT, COL_PANEL_ALT);
  tft.drawCentreString("Eye break", 120, 98, 4);
  tft.setTextColor(COL_TEXT, COL_PANEL_ALT);
  tft.drawCentreString("Look 6m away", 120, 148, 2);
  tft.setTextColor(COL_DIM, COL_PANEL_ALT);
  tft.drawCentreString(String(remaining) + "s", 120, 172, 2);
  tft.drawCentreString("or tap to dismiss", 120, 210, 1);
}

void drawMoveReminderOverlay() {
  if (cacheMove == "1") return;
  cacheMove = "1";

  tft.fillScreen(COL_BG);
  tft.fillRoundRect(20, 80, 200, 160, 14, COL_PANEL_ALT);
  tft.drawRoundRect(20, 80, 200, 160, 14, COL_ACCENT);

  tft.setTextColor(COL_ACCENT, COL_PANEL_ALT);
  tft.drawCentreString("Move!", 120, 98, 4);
  tft.setTextColor(COL_TEXT, COL_PANEL_ALT);
  tft.drawCentreString("Time to stretch", 120, 148, 2);
  tft.setTextColor(COL_DIM, COL_PANEL_ALT);
  tft.drawCentreString("Tap anywhere to dismiss", 120, 210, 1);
}

// =========================================================
// PAGES
// =========================================================
void drawHomePageFull() {
  tft.fillScreen(COL_BG);
  drawTopBar(homeTitleText());
  drawNavBar();

  cacheClock = "";
  cacheFocusTimer = "";
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    cacheHomeSlots[i] = "";
  }

  pageDirty = false;
  lastDrawnPage = PAGE_HOME;

  drawClockCardSprite(true);
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    drawHomeSlotWidget(i, true);
  }
  if (focusMenuOpen) drawFocusMenuOverlay(true);
  if (timerDoneDialogOpen) drawTimerDoneOverlay(true);
}

void updateHomeDynamic() {
  if (timerDoneDialogOpen) {
    drawTimerDoneOverlay(false);
    return;
  }

  if (focusMenuOpen) {
    drawFocusMenuOverlay(false);
    return;
  }

  drawClockCardSprite(false);
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    drawHomeSlotWidget(i, false);
  }
}

void drawWeatherPageFull() {
  tft.fillScreen(COL_BG);
  drawTopBar("Weather");
  drawNavBar();

  drawCard(8, PAGE_ROW1_Y, 108, PAGE_WIDGET_H, true);
  drawCard(124, PAGE_ROW1_Y, 108, PAGE_WIDGET_H, true);
  drawCard(8, PAGE_ROW2_Y, 108, PAGE_WIDGET_H, true);
  drawCard(124, PAGE_ROW2_Y, 108, PAGE_WIDGET_H, true);
  drawCard(8, PAGE_ROW3_Y, 108, PAGE_WIDGET_H, true);
  drawCard(124, PAGE_ROW3_Y, 108, PAGE_WIDGET_H, true);

  pageDirty = false;
  lastDrawnPage = PAGE_WEATHER;

  lastTempText = "";
  lastRainText = "";
  lastUvText = "";
  lastUvLevelText = "";
  lastKpText = "";
  lastKpLevelText = "";
  lastWindText = "";
  lastWindDirText = "";
  lastNextSunLabel = "";
  lastNextSunTime = "";
}

void updateWeatherDynamic() {
  String t = tempText();
  String tr = tempRangeText();
  String tempCombined = t + "|" + tr;
  if (tempCombined != lastTempText) {
    tft.fillRect(18, PAGE_ROW1_Y + 30, 88, 30, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("Outdoor", 18, PAGE_ROW1_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(t, 18, PAGE_ROW1_Y + 30, 4);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    tft.drawString(tr, 18, PAGE_ROW1_Y + 54, 1);
    lastTempText = tempCombined;
  }

  String r = rainText();
  if (r != lastRainText) {
    tft.fillRect(134, PAGE_ROW1_Y + 30, 88, 24, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("Rain", 134, PAGE_ROW1_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(r, 134, PAGE_ROW1_Y + 30, 4);
    lastRainText = r;
  }

  String u = uvText();
  String ul = uvLevelText();
  if (u != lastUvText || ul != lastUvLevelText || dataDirty) {
    tft.fillRect(18, PAGE_ROW2_Y + 30, 88, 30, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("UV index", 18, PAGE_ROW2_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(u, 18, PAGE_ROW2_Y + 28, 4);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    tft.drawString(ul, 18, PAGE_ROW2_Y + 52, 1);
    lastUvText = u;
    lastUvLevelText = ul;
  }

  String w = windText();
  String wd = windDirectionText();
  if (w != lastWindText || wd != lastWindDirText) {
    tft.fillRect(134, PAGE_ROW2_Y + 30, 88, 30, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("Wind", 134, PAGE_ROW2_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(w, 134, PAGE_ROW2_Y + 28, 4);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    tft.drawString(wd, 134, PAGE_ROW2_Y + 52, 1);
    lastWindText = w;
    lastWindDirText = wd;
  }

  String nl = nextSunLabel();
  String nt = nextSunTimeText();
  if (nl != lastNextSunLabel || nt != lastNextSunTime) {
    tft.fillRect(18, PAGE_ROW3_Y + 24, 88, 30, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString(nl, 18, PAGE_ROW3_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    if (useUsRegionFormat()) {
      int splitAt = nt.lastIndexOf(' ');
      String sunMain = splitAt > 0 ? nt.substring(0, splitAt) : nt;
      String sunSuffix = splitAt > 0 ? nt.substring(splitAt + 1) : "";
      tft.drawString(sunMain, 18, PAGE_ROW3_Y + 26, 4);
      if (sunSuffix.length() > 0) {
        int suffixX = 18 + tft.textWidth(sunMain.c_str(), lgfxFont(4)) + 3;
        tft.drawString(sunSuffix, suffixX, PAGE_ROW3_Y + 31, 2);
      }
    } else {
      tft.drawString(nt, 18, PAGE_ROW3_Y + 26, 4);
    }
    lastNextSunLabel = nl;
    lastNextSunTime = nt;
  }

  String k = kpText();
  String kl = kpLevelText();
  if (k != lastKpText || kl != lastKpLevelText || dataDirty) {
    tft.fillRect(134, PAGE_ROW3_Y + 30, 88, 30, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("KP index", 134, PAGE_ROW3_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(k, 134, PAGE_ROW3_Y + 28, 4);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    tft.drawString(kl, 134, PAGE_ROW3_Y + 52, 1);
    lastKpText = k;
    lastKpLevelText = kl;
  }
}

static int dailyQuoteIndex() {
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  return t.tm_yday % DAILY_QUOTE_COUNT;
}

static void fetchDailyQuote() {
  if (WiFi.status() != WL_CONNECTED) return;
  time_t now = time(nullptr);
  if (now < NTP_MIN_EPOCH) return;
  struct tm t;
  localtime_r(&now, &t);
  char today[9];
  strftime(today, sizeof(today), "%Y%m%d", &t);
  if (quoteOnlineDate == String(today)) return;  // 今天已获取

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, "https://v1.hitokoto.cn/")) return;
  int code = http.GET();
  if (code != 200) { http.end(); return; }
  String body = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, body)) return;
  String q = doc["hitokoto"].as<String>();
  if (q.length() == 0) return;

  quoteOnline = q;
  quoteOnlineDate = String(today);
  prefs.putString("quoteOnline", quoteOnline);
  prefs.putString("quoteOnlineDate", quoteOnlineDate);
  notesDirty = true;
}

static void drawCnWrapped(int x, int y, int maxW, const String& text, uint16_t fg, uint16_t bg, int lineH, int maxLines) {
  tft.setFont(&lgfx::fonts::efontCN_16);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(fg, bg);
  int cx = x, cy = y, lines = 0;
  String cur = "";
  int i = 0, len = text.length();
  while (i < len) {
    int clen = 1;
    unsigned char c = (unsigned char)text[i];
    if (c >= 0xF0) clen = 4; else if (c >= 0xE0) clen = 3; else if (c >= 0xC0) clen = 2;
    if (i + clen > len) break;
    String ch = text.substring(i, i + clen);
    String test = cur + ch;
    if (cur.length() > 0 && tft.textWidth(test) > maxW) {
      if (lines >= maxLines - 1) break;
      tft.drawString(cur, cx, cy);
      cy += lineH;
      cur = ch;
      lines++;
    } else {
      cur = test;
    }
    i += clen;
  }
  if (cur.length() > 0 && lines < maxLines) tft.drawString(cur, cx, cy);
}

void drawNotesPageFull() {
  tft.fillScreen(COL_BG);
  drawTopBar("Quote");
  drawNavBar();

  drawCard(8, 42, 224, 226, true);

  pageDirty = false;
  lastDrawnPage = PAGE_NOTES;
  lastNotesText = "";
}

void updateNotesDynamic() {
  int idx = dailyQuoteIndex();
  String quote;
  if (notesText.length() > 0 && notesText != "No notes yet.") {
    quote = notesText;             // 用户自定义优先
  } else if (quoteOnline.length() > 0) {
    quote = quoteOnline;           // 联网获取的今日语录
  } else {
    quote = DAILY_QUOTES[idx];     // 内置语录兜底
  }

  String combined = quote;
  if (combined == lastNotesText && !notesDirty) return;
  lastNotesText = combined;
  notesDirty = false;

  tft.fillRect(18, 54, 204, 196, COL_PANEL);

  // 标题
  tft.setFont(&lgfx::fonts::efontCN_16);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_ACCENT, COL_PANEL);
  tft.drawString("每日一句", 24, 58);

  // 日期
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  char dateBuf[24];
  strftime(dateBuf, sizeof(dateBuf), "%m月%d日", &t);
  tft.setFont(&lgfx::fonts::Font2);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString(dateBuf, 24, 80, 2);

  // 分隔线
  tft.drawFastHLine(24, 100, 180, COL_STROKE);

  // 语录（中文自动换行）
  drawCnWrapped(24, 110, 176, quote, COL_TEXT, COL_PANEL, 26, 4);
}

void drawStatusPageFull() {
  tft.fillScreen(COL_BG);
  drawTopBar("Status");
  drawNavBar();

  drawCard(8, PAGE_ROW1_Y, 108, PAGE_WIDGET_H, true);
  drawCard(124, PAGE_ROW1_Y, 108, PAGE_WIDGET_H, true);
  drawCard(8, PAGE_ROW2_Y, 224, PAGE_WIDGET_H, true);
  drawCard(8, PAGE_ROW3_Y, 108, PAGE_WIDGET_H, true);
  drawCard(124, PAGE_ROW3_Y, 108, PAGE_WIDGET_H, true);

  pageDirty = false;
  lastDrawnPage = PAGE_STATUS;

  lastWifiText = "";
  lastSignalText = "";
  lastIpText = "";
  lastUptimeText = "";
  lastNetworkToggleText = "";
}

void updateStatusDynamic() {
  String w = wifiStatusText();
  if (w != lastWifiText) {
    tft.fillRect(18, PAGE_ROW1_Y + 24, 88, 30, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("WiFi", 18, PAGE_ROW1_Y + 8, 2);
    tft.setTextColor(statusColor(), COL_PANEL);
    tft.drawString(w, 18, PAGE_ROW1_Y + 32, 2);
    lastWifiText = w;
  }

  String s = signalText();
  if (s != lastSignalText) {
    tft.fillRect(134, PAGE_ROW1_Y + 30, 88, 24, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("Signal", 134, PAGE_ROW1_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(s, 134, PAGE_ROW1_Y + 30, 4);
    lastSignalText = s;
  }

  String ip = ipText();
  if (ip != lastIpText) {
    tft.fillRect(18, PAGE_ROW2_Y + 30, 200, 18, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("IP address", 18, PAGE_ROW2_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(ip, 18, PAGE_ROW2_Y + 30, 2);
    lastIpText = ip;
  }

  String up = uptimeText();
  String upCombined = up + "|" + lastSyncText();
  if (upCombined != lastUptimeText) {
    tft.fillRect(18, PAGE_ROW3_Y + 26, 88, 26, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("Uptime", 18, PAGE_ROW3_Y + 10, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(up, 18, PAGE_ROW3_Y + 26, 2);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString(lastSyncText(), 18, PAGE_ROW3_Y + 42, 1);
    lastUptimeText = upCombined;
  }

  String networkLabel = wifiEnabled ? "Enabled" : "Disabled";
  if (networkLabel != lastNetworkToggleText) {
    uint16_t btnBg = wifiEnabled ? COL_ACCENT : COL_PANEL_ALT;
    uint16_t btnFg = wifiEnabled ? TFT_BLACK : COL_TEXT;
    uint16_t btnStroke = wifiEnabled ? COL_ACCENT : COL_STROKE;

    tft.fillRect(132, PAGE_ROW3_Y + 8, 92, 42, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("Network", 134, PAGE_ROW3_Y + 10, 2);
    tft.fillRoundRect(134, PAGE_ROW3_Y + 30, 88, 22, 8, btnBg);
    tft.drawRoundRect(134, PAGE_ROW3_Y + 30, 88, 22, 8, btnStroke);
    tft.setTextColor(btnFg, btnBg);
    tft.drawCentreString(networkLabel.c_str(), 178, PAGE_ROW3_Y + 32, 2);
    lastNetworkToggleText = networkLabel;
  }
}

// =========================================================
// SCHEDULE PAGE
// =========================================================
static int scheduleTodayIndex() {
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  // tm_wday: 0=Sun .. 6=Sat → Mon=1 .. Fri=5 → index 0..4
  return t.tm_wday - 1;
}

static String utf8FitWidth(const String& s, int maxPx) {
  int w = tft.textWidth(s);
  if (w <= maxPx) return s;
  String r = "";
  int i = 0, len = s.length();
  while (i < len) {
    int clen = 1;
    unsigned char c = (unsigned char)s[i];
    if (c >= 0xF0) clen = 4;
    else if (c >= 0xE0) clen = 3;
    else if (c >= 0xC0) clen = 2;
    if (i + clen > len) break;
    String sub = r + s.substring(i, i + clen);
    if (tft.textWidth(sub) > maxPx) break;
    r = sub;
    i += clen;
  }
  return r;
}

void drawSchedulePageFull() {
  tft.fillScreen(COL_BG);
  drawTopBar("Schedule");
  drawNavBar();
  cacheSchedulePage = "";
  pageDirty = false;
  lastDrawnPage = PAGE_SCHEDULE;
}

void updateScheduleDynamic() {
  int day = scheduleTodayIndex();

  String key = String(day) + "|" + String(COL_ACCENT) + "|";
  for (int i = 0; i < SCHEDULE_SLOTS; i++) {
    key += schedTime[day][i]; key += "|";
    key += schedName[day][i]; key += "|";
    key += schedRoom[day][i]; key += "|";
  }
  if (key == cacheSchedulePage) return;
  cacheSchedulePage = key;

  tft.setTextDatum(TL_DATUM);
  tft.setFont(&lgfx::fonts::efontCN_16);

  if (day < 0 || day >= SCHEDULE_DAYS) {
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("周末", 8, 44);
    tft.setTextColor(COL_DIM, COL_BG);
    tft.drawString("今天没有课程安排", 8, 74);
    return;
  }

  // Header: weekday + date
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.drawString(SCHEDULE_DAY_NAMES[day], 8, 44);
  int dayNameW = tft.textWidth(SCHEDULE_DAY_NAMES[day]);

  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  char dateBuf[12];
  strftime(dateBuf, sizeof(dateBuf), "%m-%d", &t);
  tft.setFont(&lgfx::fonts::Font2);
  tft.setTextColor(COL_DIM, COL_BG);
  tft.drawString(dateBuf, 8 + dayNameW + 10, 46, 2);

  // Class rows
  const int startY = 68;
  const int rowH   = 34;
  const int padX   = 8;
  const int w      = SCREEN_W - padX * 2;

  int row = 0;
  for (int i = 0; i < SCHEDULE_SLOTS; i++) {
    if (schedTime[day][i].length() == 0) continue;
    int y = startY + row * rowH;

    tft.fillRoundRect(padX, y, w, 30, 8, COL_PANEL);
    tft.drawRoundRect(padX, y, w, 30, 8, COL_STROKE);

    // Time
    tft.setFont(&lgfx::fonts::efontCN_16);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    tft.drawString(schedTime[day][i], 16, y + 7);

    // Name (fitted)
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(utf8FitWidth(schedName[day][i], 116), 64, y + 7);

    // Room (right-aligned)
    if (schedRoom[day][i].length() > 0) {
      tft.setTextDatum(TR_DATUM);
      tft.setTextColor(COL_DIM, COL_PANEL);
      tft.drawString(schedRoom[day][i], 228, y + 7);
      tft.setTextDatum(TL_DATUM);
    }
    row++;
  }

  if (row == 0) {
    tft.setFont(&lgfx::fonts::efontCN_16);
    tft.setTextColor(COL_DIM, COL_BG);
    tft.drawString("今天没有课程安排", 8, 100);
  }
}

void checkMidnightReset() {
  time_t now = time(nullptr);
  if (now < NTP_MIN_EPOCH) return;
  struct tm t;
  localtime_r(&now, &t);
  char today[9];
  strftime(today, sizeof(today), "%Y%m%d", &t);

  // Reset hydration if new day
  if (hydrationDate != String(today)) {
    hydrationCount = 0;
    hydrationDate  = String(today);
    prefs.putInt("hydCount", 0);
    prefs.putString("hydDate", hydrationDate);
    cacheHydration = "";
    clearHomeSlotCaches();
    pageDirty = true;
  }
}

// =========================================================
// HA PAGE
// =========================================================
bool fetchHaSensor(int idx) {
  if (haUrl.length() == 0 || haToken.length() == 0) return false;
  if (haSensorEntity[idx].length() == 0) return false;
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClient client;
  HTTPClient http;
  String url = haUrl + "/api/states/" + haSensorEntity[idx];
  http.begin(client, url);
  http.addHeader("Authorization", "Bearer " + haToken);
  http.addHeader("Content-Type", "application/json");
  int code = http.GET();
  if (code != 200) { http.end(); return false; }

  String body = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, body)) return false;
  haSensorValue[idx] = doc["state"].as<String>();
  // try to get unit from attributes
  if (doc["attributes"]["unit_of_measurement"].is<const char*>()) {
    haSensorUnit[idx] = doc["attributes"]["unit_of_measurement"].as<String>();
  }
  return true;
}

bool fetchHaCtrlState(int idx) {
  if (haUrl.length() == 0 || haToken.length() == 0) return false;
  if (haCtrlEntity[idx].length() == 0) return false;
  if (WiFi.status() != WL_CONNECTED) return false;
  if (haCtrlType[idx] == "scene" || haCtrlType[idx] == "script") return false;

  WiFiClient client;
  HTTPClient http;
  String url = haUrl + "/api/states/" + haCtrlEntity[idx];
  http.begin(client, url);
  http.addHeader("Authorization", "Bearer " + haToken);
  int code = http.GET();
  if (code != 200) { http.end(); return false; }
  String body = http.getString();
  http.end();
  JsonDocument doc;
  if (deserializeJson(doc, body)) return false;
  haCtrlState[idx] = (String(doc["state"].as<String>()) == "on");
  return true;
}

void callHaService(int idx) {
  if (haUrl.length() == 0 || haToken.length() == 0) return;
  if (haCtrlEntity[idx].length() == 0) return;
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClient client;
  HTTPClient http;
  String domain = haCtrlType[idx];
  String service;
  String body;

  if (haCtrlType[idx] == "scene") {
    domain = "scene";
    service = "turn_on";
    body = "{\"entity_id\":\"" + haCtrlEntity[idx] + "\"}";
  } else if (haCtrlType[idx] == "script") {
    domain = "script";
    service = "turn_on";
    body = "{\"entity_id\":\"" + haCtrlEntity[idx] + "\"}";
  } else {
    // switch or light — toggle
    service = haCtrlState[idx] ? "turn_off" : "turn_on";
    haCtrlState[idx] = !haCtrlState[idx];
    body = "{\"entity_id\":\"" + haCtrlEntity[idx] + "\"}";
  }

  String url = haUrl + "/api/services/" + domain + "/" + service;
  http.begin(client, url);
  http.addHeader("Authorization", "Bearer " + haToken);
  http.addHeader("Content-Type", "application/json");
  http.POST(body);
  http.end();
  cacheHaPage = "";
  pageDirty = true;
}

void ensureHaData() {
  if (haUrl.length() == 0 || haToken.length() == 0) return;
  time_t now = time(nullptr);
  if (now - lastHaFetch < HA_INTERVAL_SEC) return;
  lastHaFetch = now;
  for (int i = 0; i < HA_SENSOR_COUNT; i++) fetchHaSensor(i);
  for (int i = 0; i < HA_CTRL_COUNT; i++) fetchHaCtrlState(i);
  cacheHaPage = "";
  if (currentPage == PAGE_HA) pageDirty = true;
}

void drawHaPageFull() {
  tft.fillScreen(COL_BG);
  drawTopBar("Home Assistant");
  drawNavBar();

  // Tab row below top bar
  const int tabY = TOPBAR_H + 2;
  const int tabH = 26;
  const int tabW = SCREEN_W / 2;

  tft.fillRect(0, tabY, SCREEN_W, tabH, COL_PANEL_ALT);
  const char* tabs[2] = {"Sensors", "Controls"};
  for (int i = 0; i < 2; i++) {
    bool sel = (haPageTab == i);
    uint16_t bg = sel ? COL_ACCENT : COL_PANEL;
    uint16_t fg = sel ? TFT_BLACK : COL_DIM;
    tft.fillRoundRect(i * tabW + 2, tabY + 2, tabW - 4, tabH - 4, 6, bg);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(fg, bg);
    tft.drawString(tabs[i], i * tabW + tabW / 2, tabY + tabH / 2, 1);
    tft.setTextDatum(TL_DATUM);
  }

  cacheHaPage = "";
  pageDirty = false;
  lastDrawnPage = PAGE_HA;
}

void updateHaDynamic() {
  String key = String(haPageTab);
  if (haPageTab == 0) {
    for (int i = 0; i < HA_SENSOR_COUNT; i++) key += haSensorValue[i] + haSensorLabel[i];
  } else {
    for (int i = 0; i < HA_CTRL_COUNT; i++) key += String(haCtrlState[i]) + haCtrlLabel[i];
  }
  key += String(COL_ACCENT);
  if (key == cacheHaPage) return;
  cacheHaPage = key;

  const int startY = TOPBAR_H + 30;
  const int rowH   = 58;
  const int padX   = 8;
  const int w      = SCREEN_W - padX * 2;

  if (haPageTab == 0) {
    // Sensors
    for (int i = 0; i < HA_SENSOR_COUNT; i++) {
      int y = startY + i * rowH;
      if (y + rowH > SCREEN_H - NAV_H) break;
      tft.fillRoundRect(padX, y, w, rowH - 4, 8, COL_PANEL);
      tft.drawRoundRect(padX, y, w, rowH - 4, 8, COL_STROKE);
      tft.setTextColor(COL_DIM, COL_PANEL);
      String lbl = haSensorEntity[i].length() > 0 ? haSensorLabel[i] : "— not configured —";
      tft.drawString(lbl, padX + 8, y + 6, 1);
      tft.setTextColor(COL_TEXT, COL_PANEL);
      String val = haSensorValue[i] + (haSensorUnit[i].length() > 0 ? " " + haSensorUnit[i] : "");
      tft.drawString(val, padX + 8, y + 22, 4);
    }
  } else {
    // Controls
    for (int i = 0; i < HA_CTRL_COUNT; i++) {
      int y = startY + i * rowH;
      if (y + rowH > SCREEN_H - NAV_H) break;
      bool isToggle = (haCtrlType[i] == "switch" || haCtrlType[i] == "light");
      bool on = haCtrlState[i];
      uint16_t border = (isToggle && on) ? COL_ACCENT : COL_STROKE;
      uint16_t bg = (isToggle && on) ? COL_PANEL_ALT : COL_PANEL;
      tft.fillRoundRect(padX, y, w, rowH - 4, 8, bg);
      tft.drawRoundRect(padX, y, w, rowH - 4, 8, border);
      tft.setTextColor(COL_DIM, bg);
      String lbl = haCtrlEntity[i].length() > 0 ? haCtrlLabel[i] : "— not configured —";
      tft.drawString(lbl, padX + 8, y + 8, 2);
      tft.setTextColor((isToggle && on) ? COL_ACCENT : COL_DIM, bg);
      if (haCtrlType[i] == "scene") tft.drawString("Tap to activate", padX + 8, y + 30, 1);
      else if (haCtrlType[i] == "script") tft.drawString("Tap to run", padX + 8, y + 30, 1);
      else tft.drawString(on ? "ON" : "OFF", padX + 8, y + 30, 2);
    }
  }
}

bool handleHaTouch(int x, int y) {
  if (currentPage != PAGE_HA) return false;

  // Tab switch
  const int tabY = TOPBAR_H + 2;
  const int tabH = 26;
  if (y >= tabY && y < tabY + tabH) {
    int newTab = (x < SCREEN_W / 2) ? 0 : 1;
    if (newTab != haPageTab) {
      haPageTab = newTab;
      cacheHaPage = "";
      pageDirty = true;
    }
    return true;
  }

  if (haPageTab == 1) {
    const int startY = TOPBAR_H + 30;
    const int rowH   = 58;
    const int padX   = 8;
    const int w      = SCREEN_W - padX * 2;
    for (int i = 0; i < HA_CTRL_COUNT; i++) {
      int ry = startY + i * rowH;
      if (y >= ry && y < ry + rowH - 4 && x >= padX && x < padX + w) {
        if (haCtrlEntity[i].length() > 0) callHaService(i);
        return true;
      }
    }
  }
  return false;
}

// =========================================================
// HYDRATION HOME WIDGET
// =========================================================
void drawHydrationWidget(int x, int y, int w, int h, String& cache, bool force = false) {
  String today = "";
  time_t now = time(nullptr);
  if (now > NTP_MIN_EPOCH) {
    struct tm t; localtime_r(&now, &t);
    char buf[9]; strftime(buf, sizeof(buf), "%Y%m%d", &t);
    today = String(buf);
    if (today != hydrationDate) {
      hydrationCount = 0;
      hydrationDate  = today;
      prefs.putInt("hydCount", 0);
      prefs.putString("hydDate", hydrationDate);
    }
  }

  String combined = String(hydrationCount) + "|" + String(COL_PANEL) + String(COL_ACCENT);
  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, false);
  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString("Water", 10, 8, 2);

  String val = String(hydrationCount) + "/" + String(hydrationGoal);
  uint16_t col = hydrationCount >= hydrationGoal ? COL_GREEN : COL_TEXT;
  sprSmall.setTextColor(col, COL_PANEL);
  sprSmall.drawString(val, 10, 28, 4);

  // Progress dots (cap at 12, smaller radius if > 8)
  int dotCount = min(hydrationGoal, 12);
  int dotR = hydrationGoal > 8 ? 3 : 4;
  int dotSpacing = hydrationGoal > 8 ? 9 : 12;
  for (int i = 0; i < dotCount; i++) {
    uint16_t dc = (i < hydrationCount) ? COL_ACCENT : COL_STROKE;
    sprSmall.fillCircle(10 + i * dotSpacing, 60, dotR, dc);
  }
  pushSpriteAndDelete(sprSmall, x, y);
}

void drawScheduleWidget(int x, int y, int w, int h, String& cache, bool force = false) {
  int day = scheduleTodayIndex();
  int count = scheduleClassCount(day);
  String combined = String(day) + "/" + String(count) + "|" + String(COL_ACCENT);
  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, false);
  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString("Schedule", 10, 8, 2);
  sprSmall.setTextColor(count > 0 ? COL_TEXT : COL_DIM, COL_PANEL);
  sprSmall.drawString(String(count) + " class" + (count == 1 ? "" : "es"), 10, 28, 4);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString(count > 0 ? "Today" : "No classes", 10, 55, 1);
  pushSpriteAndDelete(sprSmall, x, y);
}

void drawCountdownWidget(int x, int y, int w, int h, String& cache, bool force = false) {
  int days = 0;
  bool hasDate = countdownDate.length() == 8;
  if (hasDate) {
    int yr = countdownDate.substring(0, 4).toInt();
    int mo = countdownDate.substring(4, 6).toInt();
    int dy = countdownDate.substring(6, 8).toInt();
    time_t now = time(nullptr);
    struct tm tgt = {};
    tgt.tm_year = yr - 1900; tgt.tm_mon = mo - 1; tgt.tm_mday = dy; tgt.tm_hour = 12;
    time_t tgtTime = mktime(&tgt);
    struct tm today_tm; localtime_r(&now, &today_tm);
    today_tm.tm_hour = 12; today_tm.tm_min = 0; today_tm.tm_sec = 0;
    time_t todayNoon = mktime(&today_tm);
    days = (int)(difftime(tgtTime, todayNoon) / 86400.0 + 0.5);
  }

  String combined = String(days) + "|" + countdownLabel + "|" + String(COL_PANEL) + String(COL_ACCENT);
  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, false);
  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  String lbl = countdownLabel.length() > 0 ? countdownLabel : "Event";
  sprSmall.drawString(lbl.c_str(), 10, 8, 2);

  if (!hasDate) {
    sprSmall.setTextColor(COL_TEXT, COL_PANEL);
    sprSmall.drawString("Set date", 10, 24, 4);
    sprSmall.setTextColor(COL_ACCENT, COL_PANEL);
    sprSmall.drawString("in web UI", 10, 52, 1);
  } else if (days == 0) {
    sprSmall.setTextColor(COL_GREEN, COL_PANEL);
    sprSmall.drawString("TODAY!", 10, 26, 4);
  } else {
    sprSmall.setTextColor(COL_TEXT, COL_PANEL);
    sprSmall.drawString(String(abs(days)), 10, 24, 4);
    sprSmall.setTextColor(COL_ACCENT, COL_PANEL);
    sprSmall.drawString(days > 0 ? "days away" : "days ago", 10, 52, 1);
  }
  pushSpriteAndDelete(sprSmall, x, y);
}

void drawCurrentPageFull() {
  switch (currentPage) {
    case PAGE_HOME:    drawHomePageFull(); break;
    case PAGE_WEATHER: drawWeatherPageFull(); break;
    case PAGE_NOTES:   drawNotesPageFull(); break;
    case PAGE_STATUS:  drawStatusPageFull(); break;
    case PAGE_SCHEDULE: drawSchedulePageFull(); break;
    case PAGE_HA:      drawHaPageFull(); break;
  }

  if (focusMenuOpen && currentPage == PAGE_HOME) drawFocusMenuOverlay(true);
  if (timerDoneDialogOpen) drawTimerDoneOverlay(true);
}

void updateCurrentPageDynamic() {
  if (timerDoneDialogOpen) {
    drawTimerDoneOverlay(false);
    return;
  }

  if (focusMenuOpen && currentPage == PAGE_HOME) {
    drawFocusMenuOverlay(false);
    return;
  }

  switch (currentPage) {
    case PAGE_HOME:    updateHomeDynamic(); break;
    case PAGE_WEATHER: updateWeatherDynamic(); break;
    case PAGE_NOTES:   updateNotesDynamic(); break;
    case PAGE_STATUS:  updateStatusDynamic(); break;
    case PAGE_SCHEDULE: updateScheduleDynamic(); break;
    case PAGE_HA:      updateHaDynamic(); break;
  }
}

bool handleFocusMenuTouch(int x, int y) {
  if (!focusMenuOpen) return false;

  if (x < TIMER_MENU_X || x >= TIMER_MENU_X + TIMER_MENU_W || y < TIMER_MENU_Y || y >= TIMER_MENU_Y + TIMER_MENU_H) {
    focusMenuOpen = false;
    cacheTimerMenu = "";
    pageDirty = true;
    return true;
  }

  struct ButtonHit {
    int x;
    int y;
    int w;
    int h;
    int minutes;
  };

  const ButtonHit buttons[] = {
    {34, 124, 74, 28, timerPresetMin[0]},
    {132, 124, 74, 28, timerPresetMin[1]},
    {34, 158, 74, 28, timerPresetMin[2]},
    {132, 158, 74, 28, timerPresetMin[3]},
    {34, 192, 74, 28, timerPresetMin[4]},
    {132, 192, 74, 28, timerPresetMin[5]}
  };

  for (const ButtonHit& btn : buttons) {
    if (x >= btn.x && x < btn.x + btn.w && y >= btn.y && y < btn.y + btn.h) {
      startFocusTimer(btn.minutes);
      pageDirty = true;
      return true;
    }
  }

  if ((focusTimerRunning || focusTimerFinished) && x >= 44 && x < 196 && y >= 224 && y < 250) {
    if (focusTimerRunning || focusTimerFinished) {
      resetFocusTimer();
    } else {
      focusMenuOpen = false;
      cacheTimerMenu = "";
    }
    pageDirty = true;
    return true;
  }

  return true;
}

bool handleHomeTouch(int x, int y) {
  if (currentPage != PAGE_HOME) return false;

  if (focusMenuOpen) return handleFocusMenuTouch(x, y);

  for (int slot = 0; slot < HOME_SLOT_COUNT; slot++) {
    int slotX, slotY, slotW, slotH;
    getHomeSlotRect(slot, slotX, slotY, slotW, slotH);
    if (x < slotX || x >= slotX + slotW || y < slotY || y >= slotY + slotH) continue;

    if (homeWidgetSlots[slot] == HOME_WIDGET_TIMER) {
      if (pomodoroEnabled) {
        if (!focusTimerRunning && !focusTimerFinished) {
          pomInBreak = false;
          pomSessionCount = 0;
          startFocusTimer(pomWorkMin);
        } else {
          pomInBreak = false;
          pomSessionCount = 0;
          resetFocusTimer();
        }
        cacheFocusTimer = "";
        clearHomeSlotCaches();
        cacheTimerMenu = "";
      } else if (focusTimerFinished) {
        resetFocusTimer();
      } else {
        focusMenuOpen = true;
        cacheFocusTimer = "";
        clearHomeSlotCaches();
        cacheTimerMenu = "";
      }
      pageDirty = true;
      return true;
    }

    if (homeWidgetSlots[slot] == HOME_WIDGET_HYDRATION) {
      if (hydrationCount < hydrationGoal) {
        hydrationCount++;
        prefs.putInt("hydCount", hydrationCount);
        cacheHomeSlots[slot] = "";
        pageDirty = true;
      }
      return true;
    }

    if (homeWidgetSlots[slot] == HOME_WIDGET_SCHEDULE) {
      currentPage = PAGE_SCHEDULE;
      pageDirty = true;
      return true;
    }
  }

  return false;
}

bool handleTimerDoneDialogTouch(int x, int y) {
  (void)x;
  (void)y;
  if (!timerDoneDialogOpen) return false;
  dismissTimerDoneDialog();
  return true;
}

bool handleStatusTouch(int x, int y) {
  if (currentPage != PAGE_STATUS) return false;

  if (x >= 124 && x < 232 && y >= 198 && y < 268) {
    setWifiEnabled(!wifiEnabled);
    return true;
  }

  return false;
}

// =========================================================
// NAVIGATION
// =========================================================
void handleNavTouch(int x, int y) {
  if (y < SCREEN_H - NAV_H) return;

  int btnW = SCREEN_W / NAV_COUNT;
  int idx = x / btnW;
  if (idx < 0 || idx >= NAV_COUNT) return;

  Page newPage = NAV_PAGES[idx];
  if (newPage != currentPage) {
    currentPage = newPage;
    pageDirty = true;
    cacheSchedulePage = "";
    cacheHaPage = "";
  }
}

// =========================================================
// WEB SERVER
// =========================================================
void handleRoot() {
  String accent = prefs.getString("accent", "cyan");
  String bg     = prefs.getString("bg", "slate");
  String txt    = prefs.getString("text", "standard");
  String units  = prefs.getString("units", "metric");
  String region = prefs.getString("region", "europe");
  String tz     = sanitizeTimezoneKey(prefs.getString("tz", "europe_central"));
  String nickname = prefs.getString("nickname", "");
  bool flashMode = prefs.getBool("flashMode", false);
  String homeSlotKeys[HOME_SLOT_COUNT];
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    homeSlotKeys[i] = prefs.getString((String("homeSlot") + String(i)).c_str(), homeWidgetKey(homeWidgetSlots[i]));
  }

  String page;
  page.reserve(32000);

  page += "<!doctype html><html><head>";
  page += "<meta charset='utf-8'>";
  page += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  page += "<title>Deskbuddy</title>";
  page += "<style>";
  page += ":root{color-scheme:dark;}";
  page += "body{margin:0;background:linear-gradient(180deg,#0b1018 0%,#111827 100%);color:#edf2f7;font-family:system-ui,sans-serif;}";
  page += ".wrap{max-width:780px;margin:0 auto;padding:28px 16px 100px;}";
  page += ".hero{margin-bottom:18px;padding:18px 20px;border:1px solid #243244;border-radius:20px;background:linear-gradient(135deg,#111927 0%,#172235 100%);box-shadow:0 10px 30px rgba(0,0,0,.22);}";
  page += ".hero h1{font-size:30px;margin:0 0 8px 0;}";
  page += ".hero p{margin:0;color:#a9b7c9;font-size:14px;}";
  page += ".ip{display:inline-block;margin-top:14px;padding:8px 12px;border-radius:999px;background:#0b1220;border:1px solid #334155;color:#dbe7f5;font-size:13px;}";
  page += ".stack{display:grid;gap:14px;}";
  page += ".settings-grid{display:grid;grid-template-columns:1fr 1fr;gap:14px;}";
  page += ".panel{background:#171b22;border:1px solid #2d3748;border-radius:18px;padding:18px;margin:0;}";
  page += ".panel-toggle{width:100%;display:flex;align-items:center;justify-content:space-between;gap:12px;background:none;border:none;color:#edf2f7;padding:0;margin:0;cursor:pointer;text-align:left;}";
  page += ".panel-toggle:hover{color:#ffffff;}";
  page += ".panel-toggle h2{flex:1;}";
  page += ".panel-chevron{font-size:18px;color:#8ea3ba;transition:transform .18s ease;}";
  page += ".panel.collapsed .panel-chevron{transform:rotate(-90deg);}";
  page += ".panel-body{margin-top:12px;}";
  page += ".panel.collapsed .panel-body{display:none;}";
  page += ".panel h2{margin:0 0 6px 0;font-size:18px;}";
  page += ".panel p{margin:0 0 14px 0;color:#94a3b8;font-size:13px;line-height:1.45;}";
  page += ".grid{display:grid;grid-template-columns:1fr 1fr;gap:14px;}";
  page += ".grid-3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:14px;}";
  page += ".label{display:block;font-size:13px;margin:0 0 8px 0;color:#a0aec0;font-weight:600;}";
  page += "textarea,input,select{width:100%;border-radius:12px;border:1px solid #334155;background:#0b1220;color:#edf2f7;padding:12px;box-sizing:border-box;font:inherit;}";
  page += "textarea{min-height:170px;resize:vertical;}";
  page += "button{margin-top:18px;background:#38bdf8;border:none;color:#001018;padding:13px 18px;border-radius:12px;font-weight:800;cursor:pointer;font:inherit;}";
  page += ".save-bar{position:fixed;bottom:0;left:0;right:0;background:rgba(10,15,24,.97);backdrop-filter:blur(12px);border-top:1px solid #2d3748;padding:14px 16px;z-index:100;}";
  page += ".save-bar button{margin:0;width:100%;max-width:780px;display:block;margin-left:auto;margin-right:auto;padding:14px;font-size:15px;border-radius:14px;}";
  page += ".muted{font-size:13px;color:#94a3b8;line-height:1.45;}";
  page += ".footer-note{margin-top:10px;font-size:12px;color:#7f92a8;}";
  page += ".settings-block{margin-top:18px;padding-top:16px;border-top:1px solid #2b3545;}";
  page += ".settings-block:first-of-type{margin-top:0;padding-top:0;border-top:none;}";
  page += ".settings-title{display:block;margin:0 0 6px 0;font-size:14px;font-weight:700;color:#edf2f7;letter-spacing:.02em;}";
  page += ".settings-desc{margin:0 0 12px 0;font-size:12px;color:#8ea3ba;line-height:1.45;}";
  page += ".color-stack{display:grid;gap:12px;}";
  page += ".color-row{display:grid;grid-template-columns:120px 1fr;gap:12px;align-items:center;}";
  page += ".color-meta{display:flex;align-items:center;justify-content:space-between;gap:10px;}";
  page += ".color-meta .label{margin:0;color:#dbe7f5;}";
  page += ".color-value{font-size:12px;color:#8ea3ba;white-space:nowrap;}";
  page += ".swatch-row{display:flex;flex-wrap:wrap;gap:8px;}";
  page += ".swatch{width:22px;height:22px;border-radius:999px;border:1px solid rgba(255,255,255,.18);cursor:pointer;position:relative;box-sizing:border-box;}";
  page += ".swatch input{display:none;}";
  page += ".swatch.active{box-shadow:0 0 0 2px #67e8f9, 0 0 0 5px rgba(103,232,249,.18);}";
  page += ".swatch.active::after{content:'';position:absolute;inset:5px;border-radius:999px;border:1px solid rgba(0,16,24,.45);}";
  page += ".timer-slot-grid{display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;margin-top:14px;}";
  page += ".timer-slot{border:1px solid #334155;border-radius:12px;background:#0b1220;padding:10px 10px 12px 10px;}";
  page += ".timer-slot-head{font-size:12px;color:#8ea3ba;margin-bottom:8px;font-weight:600;}";
  page += ".timer-slot-input{display:flex;align-items:center;gap:8px;}";
  page += ".timer-slot input{padding:10px 12px;text-align:center;font-weight:700;}";
  page += ".timer-unit{font-size:12px;color:#8ea3ba;white-space:nowrap;}";
  page += "@media(max-width:600px){.grid,.grid-3,.timer-slot-grid,.settings-grid{grid-template-columns:1fr;}.color-row{grid-template-columns:1fr;}}";
  page += ".wdg-wrap{display:flex;gap:24px;align-items:flex-start;flex-wrap:wrap;}";
  page += ".wdg-screen{flex-shrink:0;}";
  page += ".wdg-display{width:180px;background:var(--p-bg,#0b1220);border:2px solid var(--p-stroke,#334155);border-radius:10px;overflow:hidden;display:flex;flex-direction:column;box-shadow:0 8px 24px rgba(0,0,0,.35);transition:background .3s,border-color .3s;}";
  page += ".wdg-clock{height:86px;background:var(--p-panel,#172235);border-bottom:1px solid var(--p-stroke,#334155);display:flex;flex-direction:column;align-items:center;justify-content:center;gap:4px;color:var(--p-acc,#38bdf8);text-align:center;transition:background .3s,color .3s;}";
  page += ".wdg-clock-time{font-size:22px;font-weight:700;letter-spacing:.04em;}";
  page += ".wdg-clock-sub{font-size:9px;color:var(--p-dim,#64748b);letter-spacing:.03em;transition:color .3s;}";
  page += ".wdg-grid{display:grid;grid-template-columns:1fr 1fr;gap:6px;padding:6px;}";
  page += ".wdg-slot{min-height:52px;background:var(--p-bg,#0f1c2e);border:2px dashed var(--p-stroke,#2d3f55);border-radius:6px;display:flex;align-items:center;justify-content:center;font-size:11px;color:#475569;text-align:center;padding:6px;line-height:1.3;cursor:grab;user-select:none;transition:border-color .15s,background .3s,color .15s;}";
  page += ".wdg-slot.filled{border-style:solid;border-color:var(--p-acc-dim,#38bdf840);color:var(--p-txt,#cbd5e1);background:var(--p-panel,#172235);}";
  page += ".wdg-slot.wdg-over{border-color:var(--p-acc,#38bdf8)!important;background:#0e2540!important;color:var(--p-acc,#38bdf8)!important;}";
  page += ".wdg-nav{height:28px;background:var(--p-panel,#172235);border-top:1px solid var(--p-stroke,#334155);display:flex;align-items:center;justify-content:center;font-size:8px;color:#475569;gap:6px;transition:background .3s;}";
  page += ".wdg-nav-dot{width:5px;height:5px;border-radius:50%;background:var(--p-stroke,#334155);}";
  page += ".wdg-nav-dot.active{background:var(--p-acc,#38bdf8);transition:background .3s;}";
  page += ".wdg-palette{flex:1;min-width:200px;}";
  page += ".wdg-palette-label{font-size:12px;color:#64748b;margin-bottom:10px;}";
  page += ".wdg-chips{display:flex;flex-wrap:wrap;gap:8px;}";
  page += ".wdg-chip{padding:8px 14px;background:#0b1220;border:1px solid #334155;border-radius:10px;font-size:13px;color:#edf2f7;cursor:grab;user-select:none;transition:border-color .15s,box-shadow .15s;}";
  page += ".wdg-chip:hover{border-color:#38bdf8;box-shadow:0 0 0 1px #38bdf840;}";
  page += ".wdg-chip.wdg-dragging{opacity:.35;}";
  page += "</style></head><body><div class='wrap'>";
  page += "<div class='hero'>";
  page += "<h1>Deskbuddy</h1>";
  page += "<p>Shape Deskbuddy into your own desk companion with widgets, notes, colors, and smart daily tools.</p>";
  page += "<div style='display:flex;align-items:center;gap:12px;margin-top:14px;flex-wrap:wrap;'>";
  page += "<div class='ip'>&#128246; " + WiFi.localIP().toString() + "</div>";
  page += "<a href='/update' title='Upload a .bin firmware file manually (backup flashing method)' style='font-size:12px;color:#5a7fa8;text-decoration:none;border:1px solid #2d3f58;border-radius:999px;padding:6px 12px;'>Update firmware &#8599;</a>";
  page += "</div></div>";

  page += "<form method='POST' action='/save'>";
  page += "<div class='stack'>";

  // ── Widget Customization (first — most interactive) ───────────────────────
  page += "<div class='panel' data-panel='widgets'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>Widget Customization</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<div class='wdg-wrap'>";
  String previewBg  = themePreviewCss(bg);
  String previewAcc = accentPreviewCss(accent);
  String previewTxt = accentPreviewCss(txt);
  page += "<div class='wdg-screen'>";
  page += "<div class='wdg-display' id='device-preview' style='--p-bg:" + previewBg + ";--p-acc:" + previewAcc + ";--p-txt:" + previewTxt + ";--p-panel:color-mix(in srgb," + previewBg + " 55%,#334155 45%);--p-stroke:color-mix(in srgb," + previewBg + " 30%,#334155 70%);--p-dim:color-mix(in srgb," + previewAcc + " 30%,#475569 70%);--p-acc-dim:color-mix(in srgb," + previewAcc + " 25%,transparent 75%);'>";
  page += "<div class='wdg-clock'><div class='wdg-clock-time' id='preview-clock'>00:00</div><div class='wdg-clock-sub' id='preview-date'>&mdash;</div></div>";
  page += "<div class='wdg-grid'>";
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    HomeWidgetType slotType = homeWidgetFromKey(homeSlotKeys[i]);
    String slotLabel = String(homeWidgetLabel(slotType));
    if (slotType >= HOME_WIDGET_HA1 && slotType <= HOME_WIDGET_HA4) {
      int hi = slotType - HOME_WIDGET_HA1;
      if (haSensorLabel[hi].length() > 0) slotLabel = haSensorLabel[hi];
    }
    page += "<div class='wdg-slot filled' draggable='true' id='ws" + String(i) + "' data-slot='" + String(i) + "' data-key='" + homeSlotKeys[i] + "'>";
    page += htmlEscape(slotLabel);
    page += "</div>";
  }
  page += "</div>";
  page += "<div class='wdg-nav'><div class='wdg-nav-dot active'></div><div class='wdg-nav-dot'></div><div class='wdg-nav-dot'></div><div class='wdg-nav-dot'></div><div class='wdg-nav-dot'></div></div>";
  page += "</div></div>";
  page += "<div class='wdg-palette'>";
  page += "<p class='wdg-palette-label'>Drag onto a slot &mdash; or drag slots to swap</p>";
  page += "<div class='wdg-chips'>";
  const HomeWidgetType wdgAllTypes[] = {
    HOME_WIDGET_WEEK, HOME_WIDGET_TIMER, HOME_WIDGET_RAIN,
    HOME_WIDGET_OUTDOOR, HOME_WIDGET_KP, HOME_WIDGET_UV,
    HOME_WIDGET_WIND, HOME_WIDGET_SUN, HOME_WIDGET_HYDRATION, HOME_WIDGET_SCHEDULE,
    HOME_WIDGET_HA1, HOME_WIDGET_HA2, HOME_WIDGET_HA3, HOME_WIDGET_HA4,
    HOME_WIDGET_COUNTDOWN
  };
  for (HomeWidgetType wt : wdgAllTypes) {
    String chipLabel = String(homeWidgetLabel(wt));
    if (wt >= HOME_WIDGET_HA1 && wt <= HOME_WIDGET_HA4) {
      int hi = wt - HOME_WIDGET_HA1;
      if (haSensorLabel[hi].length() > 0) chipLabel = haSensorLabel[hi];
    }
    page += "<div class='wdg-chip' draggable='true' data-key='" + String(homeWidgetKey(wt)) + "'>" + htmlEscape(chipLabel) + "</div>";
  }
  page += "</div></div>";
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    page += "<input type='hidden' name='homeSlot" + String(i) + "' id='hs" + String(i) + "' value='" + homeSlotKeys[i] + "'>";
  }
  page += "</div></div></div>";

  page += "<div class='panel' data-panel='theme'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>Appearance</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<div class='color-stack'>";

  page += "<div class='color-row'><div class='color-meta'><label class='label'>Accent</label><span class='color-value' id='accent-value'>";
  page += accent;
  page += "</span></div><div class='swatch-row'>";
  page += "<label class='swatch" + String(accent=="standard"?" active":"") + "' style='background:" + accentPreviewCss("standard") + ";'><input type='radio' name='accent' value='standard'" + String(accent=="standard"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="ice"?" active":"") + "' style='background:" + accentPreviewCss("ice") + ";'><input type='radio' name='accent' value='ice'" + String(accent=="ice"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="white"?" active":"") + "' style='background:" + accentPreviewCss("white") + ";'><input type='radio' name='accent' value='white'" + String(accent=="white"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="cyan"?" active":"") + "' style='background:" + accentPreviewCss("cyan") + ";'><input type='radio' name='accent' value='cyan'" + String(accent=="cyan"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="mint"?" active":"") + "' style='background:" + accentPreviewCss("mint") + ";'><input type='radio' name='accent' value='mint'" + String(accent=="mint"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="green"?" active":"") + "' style='background:" + accentPreviewCss("green") + ";'><input type='radio' name='accent' value='green'" + String(accent=="green"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="blue"?" active":"") + "' style='background:" + accentPreviewCss("blue") + ";'><input type='radio' name='accent' value='blue'" + String(accent=="blue"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="purple"?" active":"") + "' style='background:" + accentPreviewCss("purple") + ";'><input type='radio' name='accent' value='purple'" + String(accent=="purple"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="pink"?" active":"") + "' style='background:" + accentPreviewCss("pink") + ";'><input type='radio' name='accent' value='pink'" + String(accent=="pink"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="orange"?" active":"") + "' style='background:" + accentPreviewCss("orange") + ";'><input type='radio' name='accent' value='orange'" + String(accent=="orange"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="amber"?" active":"") + "' style='background:" + accentPreviewCss("amber") + ";'><input type='radio' name='accent' value='amber'" + String(accent=="amber"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="red"?" active":"") + "' style='background:" + accentPreviewCss("red") + ";'><input type='radio' name='accent' value='red'" + String(accent=="red"?" checked":"") + "></label>";
  page += "</div></div>";

  page += "<div class='color-row'><div class='color-meta'><label class='label'>Text</label><span class='color-value' id='text-value'>";
  page += txt;
  page += "</span></div><div class='swatch-row'>";
  page += "<label class='swatch" + String(txt=="standard"?" active":"") + "' style='background:" + accentPreviewCss("standard") + ";'><input type='radio' name='text' value='standard'" + String(txt=="standard"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="ice"?" active":"") + "' style='background:" + accentPreviewCss("ice") + ";'><input type='radio' name='text' value='ice'" + String(txt=="ice"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="white"?" active":"") + "' style='background:" + accentPreviewCss("white") + ";'><input type='radio' name='text' value='white'" + String(txt=="white"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="cyan"?" active":"") + "' style='background:" + accentPreviewCss("cyan") + ";'><input type='radio' name='text' value='cyan'" + String(txt=="cyan"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="mint"?" active":"") + "' style='background:" + accentPreviewCss("mint") + ";'><input type='radio' name='text' value='mint'" + String(txt=="mint"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="green"?" active":"") + "' style='background:" + accentPreviewCss("green") + ";'><input type='radio' name='text' value='green'" + String(txt=="green"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="blue"?" active":"") + "' style='background:" + accentPreviewCss("blue") + ";'><input type='radio' name='text' value='blue'" + String(txt=="blue"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="purple"?" active":"") + "' style='background:" + accentPreviewCss("purple") + ";'><input type='radio' name='text' value='purple'" + String(txt=="purple"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="pink"?" active":"") + "' style='background:" + accentPreviewCss("pink") + ";'><input type='radio' name='text' value='pink'" + String(txt=="pink"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="orange"?" active":"") + "' style='background:" + accentPreviewCss("orange") + ";'><input type='radio' name='text' value='orange'" + String(txt=="orange"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="amber"?" active":"") + "' style='background:" + accentPreviewCss("amber") + ";'><input type='radio' name='text' value='amber'" + String(txt=="amber"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="red"?" active":"") + "' style='background:" + accentPreviewCss("red") + ";'><input type='radio' name='text' value='red'" + String(txt=="red"?" checked":"") + "></label>";
  page += "</div></div>";

  page += "<div class='color-row'><div class='color-meta'><label class='label'>Theme</label><span class='color-value' id='bg-value'>";
  page += bg;
  page += "</span></div><div class='swatch-row'>";
  page += "<label class='swatch" + String(bg=="slate"?" active":"") + "' style='background:" + themePreviewCss("slate") + ";'><input type='radio' name='bg' value='slate'" + String(bg=="slate"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="deep"?" active":"") + "' style='background:" + themePreviewCss("deep") + ";'><input type='radio' name='bg' value='deep'" + String(bg=="deep"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="nordic"?" active":"") + "' style='background:" + themePreviewCss("nordic") + ";'><input type='radio' name='bg' value='nordic'" + String(bg=="nordic"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="forest"?" active":"") + "' style='background:" + themePreviewCss("forest") + ";'><input type='radio' name='bg' value='forest'" + String(bg=="forest"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="coffee"?" active":"") + "' style='background:" + themePreviewCss("coffee") + ";'><input type='radio' name='bg' value='coffee'" + String(bg=="coffee"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="soft"?" active":"") + "' style='background:" + themePreviewCss("soft") + ";'><input type='radio' name='bg' value='soft'" + String(bg=="soft"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="midnight"?" active":"") + "' style='background:" + themePreviewCss("midnight") + ";'><input type='radio' name='bg' value='midnight'" + String(bg=="midnight"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="graphite"?" active":"") + "' style='background:" + themePreviewCss("graphite") + ";'><input type='radio' name='bg' value='graphite'" + String(bg=="graphite"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="garnet"?" active":"") + "' style='background:" + themePreviewCss("garnet") + ";'><input type='radio' name='bg' value='garnet'" + String(bg=="garnet"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="ochre"?" active":"") + "' style='background:" + themePreviewCss("ochre") + ";'><input type='radio' name='bg' value='ochre'" + String(bg=="ochre"?" checked":"") + "></label>";
  page += "</div></div>";

  page += "</div></div></div>";

  page += "<div class='panel' data-panel='settings'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>Settings</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>Core behavior and timer setup.</p>";
  page += "<div class='settings-block'>";
  page += "<span class='settings-title'>General</span>";
  page += "<div class='grid'>";
  page += "<div><label class='label'>Buddy nickname</label><input name='nickname' maxlength='24' value='" + htmlEscape(nickname) + "'></div>";
  page += "<div><label class='label'>Auto sleep interval</label><select name='sleepMin'>";
  page += "<option value='0'"  + String(sleepIntervalMin==0?" selected":"")  + ">Never</option>";
  page += "<option value='1'"  + String(sleepIntervalMin==1?" selected":"")  + ">1 minute</option>";
  page += "<option value='5'"  + String(sleepIntervalMin==5?" selected":"")  + ">5 minutes</option>";
  page += "<option value='10'" + String(sleepIntervalMin==10?" selected":"") + ">10 minutes</option>";
  page += "<option value='30'" + String(sleepIntervalMin==30?" selected":"") + ">30 minutes</option>";
  page += "<option value='60'" + String(sleepIntervalMin==60?" selected":"") + ">1 hour</option>";
  page += "</select><div class='muted' style='margin-top:8px;'>Sleep dims the screen first, then turns it fully off after 60 seconds.</div></div>";
  page += "<div><label class='label'>Measurement system</label><select name='units'>";
  page += "<option value='metric'"   + String(units=="metric"?" selected":"")   + ">Celsius / mm</option>";
  page += "<option value='imperial'" + String(units=="imperial"?" selected":"") + ">Fahrenheit / inches</option>";
  page += "</select></div>";
  page += "<div><label class='label'>Date format</label><select name='region'>";
  page += "<option value='europe'" + String(region=="europe"?" selected":"") + ">European: dd.mm.yyyy</option>";
  page += "<option value='us'" + String(region=="us"?" selected":"") + ">US: mm/dd/yyyy</option>";
  page += "</select></div>";
  page += "<div><label class='label'>Time zone</label><select name='tz'>";
  appendTimezoneOptions(page, tz);
  page += "</select></div>";
  page += "</div>";
  page += "</div>";
  page += "<div class='settings-block'><span class='settings-title'>Timer</span><div class='settings-desc'>Choose the six quick timers shown in the popup menu.</div><div class='timer-slot-grid'>";
  for (int i = 0; i < 6; i++) {
    page += "<div class='timer-slot'><div class='timer-slot-head'>Slot " + String(i + 1) + "</div><div class='timer-slot-input'><input type='number' min='1' max='180' name='timer" + String(i) + "' value='" + String(timerPresetMin[i]) + "'><span class='timer-unit'>min</span></div></div>";
  }
  page += "</div>";
  page += "<div style='margin-top:14px;'><span class='settings-title'>Alert behavior</span><label style='display:flex;align-items:center;gap:10px;color:#edf2f7;'><input type='checkbox' name='flashMode' value='1'" + String(flashMode ? " checked" : "") + " style='width:auto;'>Flash screen when timer ends</label></div></div>";
  page += "<div class='settings-block'><span class='settings-title'>Location</span><div class='settings-desc'>Used for weather data and sun times.</div><div class='grid-3'>";
  page += "<div><label class='label'>Location name</label><input name='locname' value='" + htmlEscape(locationName) + "'></div>";
  page += "<div><label class='label'>Latitude</label><input name='lat' value='" + String(LAT, 6) + "'></div>";
  page += "<div><label class='label'>Longitude</label><input name='lng' value='" + String(LNG, 6) + "'></div>";
  page += "</div><div class='footer-note'>Enter your city's latitude and longitude (e.g. from maps.google.com).</div></div>";

  // Context Clock — merged as settings-block
  page += "<div class='settings-block'><span class='settings-title'>Context Clock</span>";
  page += "<div class='settings-desc'>Show a second timezone on the clock card, below the date. Leave label empty to disable.</div>";
  page += "<div class='settings-grid'>";
  page += "<div><label class='label'>Label (e.g. NYC, Tokyo)</label><input name='tz2label' maxlength='8' value='" + htmlEscape(tz2Label) + "'></div>";
  page += "<div><label class='label'>Timezone</label><select name='tz2'>";
  page += "<option value=''>-- Disabled --</option>";
  auto tzOpt2 = [&](const String& key, const String& label) {
    page += "<option value='" + key + "'" + (tz2Key == key ? " selected" : "") + ">" + label + "</option>";
  };
  tzOpt2("utc","UTC");
  tzOpt2("europe_west","Western Europe (WET)");
  tzOpt2("uk","UK / Ireland");
  tzOpt2("europe_central","Central Europe (CET)");
  tzOpt2("europe_east","Eastern Europe (EET)");
  tzOpt2("us_eastern","US Eastern (ET)");
  tzOpt2("us_central","US Central (CT)");
  tzOpt2("us_mountain","US Mountain (MT)");
  tzOpt2("us_pacific","US Pacific (PT)");
  tzOpt2("canada_atlantic","Canada Atlantic");
  tzOpt2("brazil_east","Brazil (BRT)");
  tzOpt2("argentina","Argentina (ART)");
  tzOpt2("africa_south","South Africa (SAST)");
  tzOpt2("middle_east_gulf","Gulf (GST)");
  tzOpt2("india","India (IST)");
  tzOpt2("thailand","Thailand (ICT)");
  tzOpt2("china","China (CST)");
  tzOpt2("asia_tokyo","Japan (JST)");
  tzOpt2("korea","Korea (KST)");
  tzOpt2("australia_sydney","Australia East (AEST)");
  tzOpt2("new_zealand","New Zealand (NZST)");
  page += "</select></div>";
  page += "</div></div>";

  // Reminders — merged as settings-block
  page += "<div class='settings-block'><span class='settings-title'>Reminders</span>";
  page += "<div class='settings-desc'>Periodic full-screen overlays. Eye break auto-dismisses after 20 s; movement reminder waits for a tap.</div>";
  page += "<div class='settings-grid'>";
  page += "<div><span class='settings-title' style='font-size:13px;'>Eye break (20-20-20)</span>"
          "<label style='display:flex;align-items:center;gap:10px;color:#edf2f7;margin:8px 0;'>"
          "<input type='checkbox' name='eyeBreak' value='1'" + String(eyeBreakEnabled ? " checked" : "") + " style='width:auto;'>Enabled</label>"
          "<label class='label'>Interval (minutes)</label>"
          "<input type='number' name='eyeBreakMin' min='5' max='120' value='" + String(eyeBreakIntervalMin) + "'></div>";
  page += "<div><span class='settings-title' style='font-size:13px;'>Movement reminder</span>"
          "<label style='display:flex;align-items:center;gap:10px;color:#edf2f7;margin:8px 0;'>"
          "<input type='checkbox' name='moveRemind' value='1'" + String(moveEnabled ? " checked" : "") + " style='width:auto;'>Enabled</label>"
          "<label class='label'>Interval (minutes)</label>"
          "<input type='number' name='moveMin' min='5' max='120' value='" + String(moveIntervalMin) + "'></div>";
  page += "</div></div>";

  page += "<div class='settings-block'><span class='settings-title'>Hydration</span>";
  page += "<div class='settings-desc'>Daily water intake goal. Tap the widget on the home screen to log a glass.</div>";
  page += "<div class='settings-grid'>";
  page += "<div><label class='label'>Daily goal (glasses)</label><input type='number' name='hydGoal' min='1' max='20' value='" + String(hydrationGoal) + "'></div>";
  page += "<div style='display:flex;align-items:flex-end;'><button type='button' onclick=\"fetch('/api/hydration/reset',{method:'POST'}).then(function(){location.reload();})\" style='margin-bottom:0;width:100%;'>Reset today (" + String(hydrationCount) + "/" + String(hydrationGoal) + ")</button></div>";
  page += "</div></div>";

  page += "<div class='settings-block'><span class='settings-title'>Countdown Widget</span>";
  page += "<div class='settings-desc'>Home screen widget counting days to a target date.</div>";
  page += "<div class='settings-grid'>";
  page += "<div><label class='label'>Label (e.g. Vacation)</label><input name='cdLabel' maxlength='12' value='" + htmlEscape(countdownLabel) + "'></div>";
  {
    String cdDateHtml = "";
    if (countdownDate.length() == 8) {
      cdDateHtml = countdownDate.substring(0,4) + "-" + countdownDate.substring(4,6) + "-" + countdownDate.substring(6,8);
    }
    page += "<div><label class='label'>Target date</label><input type='date' name='cdDate' value='" + cdDateHtml + "'></div>";
  }
  page += "</div></div>";

  page += "<div class='settings-block'><span class='settings-title'>Pomodoro Mode</span>";
  page += "<div class='settings-desc'>Auto-cycling focus timer: work &rarr; break &rarr; work. Long break after 4 sessions.</div>";
  page += "<label style='display:flex;align-items:center;gap:10px;color:#edf2f7;margin-bottom:12px;'><input type='checkbox' name='pomEn' value='1'" + String(pomodoroEnabled ? " checked" : "") + " style='width:auto;'>Enable Pomodoro mode</label>";
  page += "<div class='settings-grid'>";
  page += "<div><label class='label'>Work (min)</label><input type='number' name='pomWork' min='1' max='90' value='" + String(pomWorkMin) + "'></div>";
  page += "<div><label class='label'>Break (min)</label><input type='number' name='pomBreak' min='1' max='30' value='" + String(pomBreakMin) + "'></div>";
  page += "<div><label class='label'>Long break (min)</label><input type='number' name='pomLong' min='5' max='60' value='" + String(pomLongBreakMin) + "'></div>";
  page += "</div></div>";

  page += "</div></div>";

  // ── Schedule panel ────────────────────────────────────────
  page += "<div class='panel' data-panel='schedule'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>Class Schedule</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>周一~周五，每天最多 6 节。时间留空表示该节无课（如 08:00）。</p>";
  page += "<div class='settings-block'><span class='settings-title'>批量导入（粘贴整张课表）</span>";
  page += "<div class='settings-desc'>每行一节课：<code>星期 时间 课程 教室</code>（空格或 Tab 分隔）。也支持按天分组：天名单独一行，下面每行 <code>时间 课程 教室</code>。</div>";
  page += "<textarea id='schedBulk' rows='7' placeholder='周一 08:00 高等数学 教学楼A301&#10;周二 09:55 大学英语 教学楼B204&#10;周三 08:00 线性代数 教学楼C102' style='min-height:120px;font-family:monospace;'></textarea>";
  page += "<div style='display:flex;gap:8px;margin-top:8px;flex-wrap:wrap;'>";
  page += "<button type='button' onclick='schedImport()' style='margin:0;'>解析填入</button>";
  page += "<button type='button' onclick='schedClear()' style='margin:0;'>清空全部</button>";
  page += "<button type='button' onclick='schedCopy()' style='margin:0;'>复制当前课表</button>";
  page += "</div></div>";
  const char* schedDayLabels[] = {"周一","周二","周三","周四","周五"};
  for (int d = 0; d < SCHEDULE_DAYS; d++) {
    page += "<div class='settings-block'><span class='settings-title'>" + String(schedDayLabels[d]) + "</span>";
    page += "<div style='display:grid;gap:6px;'>";
    for (int i = 0; i < SCHEDULE_SLOTS; i++) {
      String base = "sch" + String(d) + String(i);
      page += "<div style='display:grid;grid-template-columns:70px 1fr 90px;gap:6px;align-items:center;'>";
      page += "<input type='text' name='" + base + "t' maxlength='5' placeholder='HH:MM' value='" + htmlEscape(schedTime[d][i]) + "'>";
      page += "<input type='text' name='" + base + "n' maxlength='20' placeholder='课程名' value='" + htmlEscape(schedName[d][i]) + "'>";
      page += "<input type='text' name='" + base + "r' maxlength='12' placeholder='教室' value='" + htmlEscape(schedRoom[d][i]) + "'>";
      page += "</div>";
    }
    page += "</div></div>";
  }
  page += "</div></div>";

  // ── Home Assistant panel ───────────────────────────────────
  page += "<div class='panel' data-panel='ha'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>Home Assistant</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>Connect to your local Home Assistant. The token never leaves your network.</p>";
  page += "<div class='settings-block'><span class='settings-title'>Connection</span>";
  page += "<div class='grid'>";
  page += "<div><label class='label'>HA URL</label><input name='haUrl' value='" + htmlEscape(haUrl) + "' placeholder='http://homeassistant.local:8123'></div>";
  page += "<div><label class='label'>Long-lived access token</label><input type='password' name='haToken' value='" + htmlEscape(haToken) + "' placeholder='eyJ...'></div>";
  page += "</div></div>";
  page += "<div class='settings-block'><span class='settings-title'>Sensors (4 slots)</span><div class='settings-desc'>Entity IDs like sensor.office_temperature. Value + unit shown automatically.</div>";
  page += "<div class='grid'>";
  for (int i = 0; i < HA_SENSOR_COUNT; i++) {
    page += "<div><label class='label'>Sensor " + String(i+1) + " label</label><input name='haSl" + String(i) + "' value='" + htmlEscape(haSensorLabel[i]) + "'></div>";
    page += "<div><label class='label'>Entity ID</label><input name='haSe" + String(i) + "' value='" + htmlEscape(haSensorEntity[i]) + "' placeholder='sensor.xxx'></div>";
  }
  page += "</div></div>";
  page += "<div class='settings-block'><span class='settings-title'>Controls (4 buttons)</span><div class='settings-desc'>Switches, lights, scenes, or scripts. Type determines tap behaviour.</div>";
  page += "<div class='grid'>";
  for (int i = 0; i < HA_CTRL_COUNT; i++) {
    page += "<div><label class='label'>Control " + String(i+1) + " label</label><input name='haCl" + String(i) + "' value='" + htmlEscape(haCtrlLabel[i]) + "'></div>";
    page += "<div><label class='label'>Entity ID + type</label><div style='display:flex;gap:6px;'>";
    page += "<input name='haCe" + String(i) + "' value='" + htmlEscape(haCtrlEntity[i]) + "' placeholder='switch.xxx' style='flex:2;'>";
    page += "<select name='haCt" + String(i) + "' style='flex:1;'>";
    const char* types[] = {"switch","light","scene","script"};
    for (const char* t : types) {
      page += "<option value='" + String(t) + "'" + (haCtrlType[i] == t ? " selected" : "") + ">" + String(t) + "</option>";
    }
    page += "</select></div></div>";
  }
  page += "</div></div>";
  page += "</div></div>";

  // ── Daily quote panel ─────────────────────────────────────────────────────
  page += "<div class='panel collapsed' data-panel='notes'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='false'><h2>每日一句</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>设备每天自动显示一句励志语录（内置 25 条，按日期轮换）。</p>";
  page += "<label class='label'>自定义句子（可选）</label>";
  page += "<textarea name='notes' maxlength='700' placeholder='留空则显示每日一句'>";
  if (notesText != "No notes yet.") page += htmlEscape(notesText);
  page += "</textarea>";
  page += "<div class='muted'>填了自定义内容就显示它，清空则回到每日一句。</div>";
  page += "</div></div>";

  // ── Quick sticky note ─────────────────────────────────────────────────────
  page += "<div class='panel collapsed' data-panel='quicknote'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='false'><h2>Quick Note</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>Send a sticky note to the display. Also callable via: <code>curl http://" + WiFi.localIP().toString() + "/api/note -d '{\"msg\":\"text\"}'</code></p>";
  for (int i = 0; i < STICKY_COUNT; i++) {
    if (stickyNote[i].length() == 0) continue;
    page += "<div style='display:flex;align-items:center;gap:8px;margin-bottom:6px;padding:8px 10px;background:#0b1220;border:1px solid #38bdf830;border-radius:8px;'>";
    page += "<span style='flex:1;font-size:13px;color:#edf2f7;'>" + htmlEscape(stickyNote[i]) + "</span>";
    page += "<a href='/api/note/clear?id=" + String(i) + "' style='color:#f87171;font-size:11px;text-decoration:none;' onclick=\"fetch('/api/note/clear',{method:'POST',body:'{\\\"id\\\":"+String(i)+"}',headers:{'Content-Type':'application/json'}});this.closest('div').remove();return false;\">&#10005;</a>";
    page += "</div>";
  }
  page += "<div style='display:flex;gap:8px;margin-top:8px;'>";
  page += "<input id='quickNoteInput' type='text' maxlength='80' placeholder='Type a note...' style='flex:1;'>";
  page += "<button type='button' onclick=\"var v=document.getElementById('quickNoteInput').value;if(v){fetch('/api/note',{method:'POST',body:JSON.stringify({msg:v}),headers:{'Content-Type':'application/json'}}).then(()=>{location.reload();});}\">Send</button>";
  page += "</div></div></div>";

  page += "</div>"; // end .stack
  page += "<div class='save-bar'><button type='submit'>&#128190; Save to Deskbuddy</button></div>";
  page += "</form>";
  page += "<script>";
  page += "var colorNames={accent:{standard:'Standard',ice:'Ice',white:'White',cyan:'Cyan',mint:'Mint',green:'Green',blue:'Blue',purple:'Purple',pink:'Pink',orange:'Orange',amber:'Amber',red:'Red'},text:{standard:'Standard',ice:'Ice',white:'White',cyan:'Cyan',mint:'Mint',green:'Green',blue:'Blue',purple:'Purple',pink:'Pink',orange:'Orange',amber:'Amber',red:'Red'},bg:{slate:'Slate',deep:'Deep black',nordic:'Nordic blue',forest:'Forest',coffee:'Coffee',soft:'Soft dark',midnight:'Midnight',graphite:'Graphite',garnet:'Garnet',ochre:'Ochre'}};";
  page += "var panelStorageKey='deskbuddy-panel-state-v1';";
  page += "document.querySelectorAll('.swatch input').forEach(function(input){";
  page += "input.addEventListener('change',function(){";
  page += "document.querySelectorAll('.swatch input[name=\"'+input.name+'\"]').forEach(function(peer){";
  page += "peer.closest('.swatch').classList.toggle('active', peer.checked);";
  page += "});";
  page += "var valueEl=document.getElementById(input.name+'-value');";
  page += "if(valueEl&&colorNames[input.name]&&colorNames[input.name][input.value]){valueEl.textContent=colorNames[input.name][input.value];}";
  page += "});";
  page += "});";
  page += "function readPanelState(){try{return JSON.parse(localStorage.getItem(panelStorageKey)||'{}');}catch(e){return {};}}";
  page += "function writePanelState(state){localStorage.setItem(panelStorageKey,JSON.stringify(state));}";
  page += "function applyPanelState(panel,collapsed){panel.classList.toggle('collapsed',collapsed);var btn=panel.querySelector('.panel-toggle');if(btn){btn.setAttribute('aria-expanded',collapsed?'false':'true');}}";
  page += "var savedPanelState=readPanelState();";
  page += "document.querySelectorAll('.panel[data-panel]').forEach(function(panel){";
  page += "var panelId=panel.getAttribute('data-panel');";
  page += "if(Object.prototype.hasOwnProperty.call(savedPanelState,panelId)){applyPanelState(panel,!!savedPanelState[panelId]);}";
  page += "});";
  page += "document.querySelectorAll('.panel-toggle').forEach(function(btn){";
  page += "btn.addEventListener('click',function(){";
  page += "var panel=btn.closest('.panel');";
  page += "var collapsed=!panel.classList.contains('collapsed');";
  page += "applyPanelState(panel,collapsed);";
  page += "var state=readPanelState();";
  page += "var panelId=panel.getAttribute('data-panel');";
  page += "if(panelId){state[panelId]=collapsed;writePanelState(state);}";
  page += "});";
  page += "});";
  // Live preview color lookup tables (generated from C++ color functions)
  page += "var BG_COLORS={";
  {
    const char* bgKeys[] = {"slate","deep","nordic","forest","coffee","soft","midnight","graphite","garnet","ochre"};
    for (int i = 0; i < 10; i++) {
      if (i > 0) page += ",";
      page += String(bgKeys[i]) + ":'" + themePreviewCss(bgKeys[i]) + "'";
    }
  }
  page += "};";
  page += "var ACCENT_COLORS={";
  {
    const char* accKeys[] = {"standard","ice","white","cyan","mint","green","blue","purple","pink","orange","amber","red"};
    for (int i = 0; i < 12; i++) {
      if (i > 0) page += ",";
      page += String(accKeys[i]) + ":'" + accentPreviewCss(accKeys[i]) + "'";
    }
  }
  page += "};";
  page += "function updatePreview(){";
  page += "var bgKey=(document.querySelector('[name=bg]:checked')||{value:'slate'}).value;";
  page += "var accKey=(document.querySelector('[name=accent]:checked')||{value:'cyan'}).value;";
  page += "var txtKey=(document.querySelector('[name=text]:checked')||{value:'standard'}).value;";
  page += "var preview=document.getElementById('device-preview');if(!preview)return;";
  page += "var bg=BG_COLORS[bgKey]||BG_COLORS['slate'];";
  page += "var acc=ACCENT_COLORS[accKey]||ACCENT_COLORS['cyan'];";
  page += "var txt=ACCENT_COLORS[txtKey]||ACCENT_COLORS['standard'];";
  page += "preview.style.setProperty('--p-bg',bg);";
  page += "preview.style.setProperty('--p-acc',acc);";
  page += "preview.style.setProperty('--p-txt',txt);";
  page += "}";
  page += "document.querySelectorAll('.swatch input').forEach(function(r){r.addEventListener('change',updatePreview);});";
  page += "updatePreview();";
  // Live clock in preview
  page += "function updatePreviewClock(){";
  page += "var c=document.getElementById('preview-clock');var d=document.getElementById('preview-date');if(!c)return;";
  page += "var now=new Date();";
  page += "c.textContent=String(now.getHours()).padStart(2,'0')+':'+String(now.getMinutes()).padStart(2,'0');";
  page += "if(d){var dy=['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];var mo=['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];";
  page += "d.textContent=dy[now.getDay()]+' '+now.getDate()+' '+mo[now.getMonth()];}";
  page += "}";
  page += "updatePreviewClock();setInterval(updatePreviewClock,30000);";
  // Widget drag-and-drop
  page += "(function(){";
  // Build wdgLabels including current HA sensor names
  page += "var wdgLabels={week:'Week',timer:'Timer',rain:'Rain',outdoor:'Outdoor',kp:'KP Index',uv:'UV Index',wind:'Wind',sun:'Sun',hydration:'Hydration',schedule:'Schedule',countdown:'Countdown'";
  for (int i = 0; i < HA_SENSOR_COUNT; i++) {
    String lbl = haSensorLabel[i].length() > 0 ? haSensorLabel[i] : ("HA Sensor " + String(i + 1));
    page += ",ha" + String(i + 1) + ":'" + lbl + "'";
  }
  page += "};";
  page += "var dragKey=null,dragFrom=null;";
  page += "function setSlot(idx,key){";
  page += "var s=document.getElementById('ws'+idx);";
  page += "s.textContent=wdgLabels[key]||key;s.setAttribute('data-key',key);";
  page += "s.classList.toggle('filled',!!key);";
  page += "document.getElementById('hs'+idx).value=key;";
  page += "}";
  page += "document.querySelectorAll('.wdg-chip').forEach(function(c){";
  page += "c.addEventListener('dragstart',function(e){dragKey=c.getAttribute('data-key');dragFrom=null;c.classList.add('wdg-dragging');e.dataTransfer.effectAllowed='move';e.dataTransfer.setData('text/plain',dragKey);});";
  page += "c.addEventListener('dragend',function(){c.classList.remove('wdg-dragging');});";
  page += "});";
  page += "document.querySelectorAll('.wdg-slot').forEach(function(s){";
  page += "s.addEventListener('dragstart',function(e){var k=s.getAttribute('data-key');dragKey=k||null;dragFrom=+s.getAttribute('data-slot');e.dataTransfer.effectAllowed='move';e.dataTransfer.setData('text/plain',dragKey||'');});";
  page += "s.addEventListener('dragover',function(e){e.preventDefault();s.classList.add('wdg-over');e.dataTransfer.dropEffect='move';});";
  page += "s.addEventListener('dragleave',function(e){if(!s.contains(e.relatedTarget))s.classList.remove('wdg-over');});";
  page += "s.addEventListener('drop',function(e){e.preventDefault();s.classList.remove('wdg-over');";
  page += "if(dragKey===null)return;";
  page += "var to=+s.getAttribute('data-slot');";
  page += "if(dragFrom!==null&&dragFrom!==to){setSlot(dragFrom,s.getAttribute('data-key')||'');}";
  page += "setSlot(to,dragKey);dragKey=null;dragFrom=null;});";
  page += "});";
  page += "})();";
  page += "function schedDayIndex(s){s=s.trim();if(s=='周一'||s=='星期一'||s=='1'||s=='Mon'||s=='Monday')return 0;if(s=='周二'||s=='星期二'||s=='2'||s=='Tue'||s=='Tuesday')return 1;if(s=='周三'||s=='星期三'||s=='3'||s=='Wed'||s=='Wednesday')return 2;if(s=='周四'||s=='星期四'||s=='4'||s=='Thu'||s=='Thursday')return 3;if(s=='周五'||s=='星期五'||s=='5'||s=='Fri'||s=='Friday')return 4;return -1;}";
  page += "function schedSplit(line){line=line.trim();var p=line.split('\\t');if(p.length<2)p=line.split('  ');if(p.length<2)p=line.split(' ');return p;}";
  page += "function schedFill(day,time,name,room){for(var i=0;i<6;i++){var t=document.querySelector('[name=\"sch'+day+i+'t\"]');var n=document.querySelector('[name=\"sch'+day+i+'n\"]');var r=document.querySelector('[name=\"sch'+day+i+'r\"]');if(!t||!n||!r)return;if(!t.value){t.value=time;n.value=name;r.value=room;return;}}}";
  page += "function schedImport(){var el=document.getElementById('schedBulk');var text=el?el.value:'';var lines=text.split('\\n');var day=-1,count=0;for(var i=0;i<lines.length;i++){var line=lines[i].trim();if(!line)continue;var d=schedDayIndex(line);if(d>=0){day=d;continue;}var p=schedSplit(line);var lead=schedDayIndex(p[0]);if(lead>=0){day=lead;p=p.slice(1);}if(day<0)continue;if(p.length>=2){var time=p[0],name=p[1],room=p.length>=3?p.slice(2).join(' '):'';schedFill(day,time,name,room);count++;}}alert(count>0?('已导入 '+count+' 节课，检查后点「Save」保存。'):'没有解析到课程，请检查格式。');}";
  page += "function schedClear(){for(var d=0;d<5;d++)for(var i=0;i<6;i++){var t=document.querySelector('[name=\"sch'+d+i+'t\"]');var n=document.querySelector('[name=\"sch'+d+i+'n\"]');var r=document.querySelector('[name=\"sch'+d+i+'r\"]');if(t)t.value='';if(n)n.value='';if(r)r.value='';}}";
  page += "function schedCopy(){var out=[],dn=['周一','周二','周三','周四','周五'];for(var d=0;d<5;d++)for(var i=0;i<6;i++){var t=document.querySelector('[name=\"sch'+d+i+'t\"]');var n=document.querySelector('[name=\"sch'+d+i+'n\"]');var r=document.querySelector('[name=\"sch'+d+i+'r\"]');if(t&&t.value)out.push(dn[d]+' '+t.value+' '+n.value+(r.value?' '+r.value:''));}var txt=out.join('\\n')||'（空课表）';if(navigator.clipboard&&navigator.clipboard.writeText){navigator.clipboard.writeText(txt).then(function(){alert('已复制 '+out.length+' 节课到剪贴板');},function(){prompt('复制失败，请手动复制：',txt);});}else{prompt('复制到剪贴板：',txt);}}";
  page += "</script>";
  page += "</div></body></html>";

  server.send(200, "text/html; charset=utf-8", page);
}

void handleSave() {
  String newNotes  = server.hasArg("notes") ? server.arg("notes") : notesText;
  String newAccent = server.hasArg("accent") ? server.arg("accent") : "cyan";
  String newBg     = server.hasArg("bg") ? server.arg("bg") : "slate";
  String newText   = server.hasArg("text") ? server.arg("text") : "standard";
  String newUnits  = server.hasArg("units") ? server.arg("units") : "metric";
  String newRegion = server.hasArg("region") ? server.arg("region") : "europe";
  String newTz     = server.hasArg("tz") ? server.arg("tz") : timezoneKey;
  String newLoc    = server.hasArg("locname") ? server.arg("locname") : locationName;
  String newNickname = server.hasArg("nickname") ? server.arg("nickname") : buddyNickname;
  HomeWidgetType newHomeSlots[HOME_SLOT_COUNT];
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    String key = String("homeSlot") + String(i);
    String currentKey = homeWidgetKey(homeWidgetSlots[i]);
    newHomeSlots[i] = homeWidgetFromKey(server.hasArg(key) ? server.arg(key) : currentKey);
  }

  float newLat = server.hasArg("lat") ? server.arg("lat").toFloat() : LAT;
  float newLng = server.hasArg("lng") ? server.arg("lng").toFloat() : LNG;

  newNotes.trim();
  newLoc.trim();
  newNickname.trim();

  if (newNotes.length() == 0) newNotes = "No notes yet.";
  if (newNotes.length() > 700) newNotes = newNotes.substring(0, 700);
  if (newLoc.length() == 0) newLoc = "Unknown";
  if (newNickname.length() > 24) newNickname = newNickname.substring(0, 24);
  if (newUnits != "metric" && newUnits != "imperial") newUnits = "metric";
  if (newRegion != "europe" && newRegion != "us") newRegion = "europe";
  newTz = sanitizeTimezoneKey(newTz);

  int newSleepMin = server.hasArg("sleepMin") ? server.arg("sleepMin").toInt() : sleepIntervalMin;
  sleepIntervalMin = constrain(newSleepMin, 0, 120);
  bool newFlashMode = server.hasArg("flashMode");

  bool locationChanged =
    (fabsf(newLat - LAT) > 0.0001f) ||
    (fabsf(newLng - LNG) > 0.0001f) ||
    (newLoc != locationName);

  notesText = newNotes;
  buddyNickname = newNickname;
  locationName = newLoc;
  LAT = newLat;
  LNG = newLng;
  unitKey = newUnits;
  regionFormatKey = newRegion;
  timezoneKey = newTz;
  flashModeEnabled = newFlashMode;
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    homeWidgetSlots[i] = newHomeSlots[i];
  }

  for (int i = 0; i < 6; i++) {
    String key = String("timer") + String(i);
    int currentValue = timerPresetMin[i];
    int nextValue = server.hasArg(key) ? server.arg(key).toInt() : currentValue;
    timerPresetMin[i] = sanitizeTimerMinutes(nextValue);
  }

  prefs.putString("notes", notesText);
  prefs.putString("accent", newAccent);
  prefs.putString("bg", newBg);
  prefs.putString("text", newText);
  prefs.putString("units", unitKey);
  prefs.putString("region", regionFormatKey);
  prefs.putString("tz", timezoneKey);
  prefs.putString("nickname", buddyNickname);
  prefs.putString("locname", locationName);
  prefs.putFloat("lat", LAT);
  prefs.putFloat("lng", LNG);
  prefs.putInt("sleepMin", sleepIntervalMin);
  prefs.putBool("flashMode", flashModeEnabled);

  // Context clock
  tz2Key   = server.hasArg("tz2") ? server.arg("tz2") : "";
  tz2Label = server.hasArg("tz2label") ? server.arg("tz2label") : "";
  tz2Label.trim();
  if (tz2Label.length() > 8) tz2Label = tz2Label.substring(0, 8);
  if (tz2Key.length() == 0) tz2Label = "";
  prefs.putString("tz2", tz2Key);
  prefs.putString("tz2label", tz2Label);

  // Reminders
  eyeBreakEnabled     = server.hasArg("eyeBreak");
  eyeBreakIntervalMin = constrain(server.hasArg("eyeBreakMin") ? server.arg("eyeBreakMin").toInt() : 20, 5, 120);
  moveEnabled         = server.hasArg("moveRemind");
  moveIntervalMin     = constrain(server.hasArg("moveMin") ? server.arg("moveMin").toInt() : 45, 5, 120);
  prefs.putBool("eyeBreak", eyeBreakEnabled);
  prefs.putInt("eyeBreakMin", eyeBreakIntervalMin);
  prefs.putBool("moveRemind", moveEnabled);
  prefs.putInt("moveMin", moveIntervalMin);

  // Class schedule
  for (int d = 0; d < SCHEDULE_DAYS; d++) {
    for (int i = 0; i < SCHEDULE_SLOTS; i++) {
      String base = "sch" + String(d) + String(i);
      schedTime[d][i] = server.hasArg(base + "t") ? server.arg(base + "t") : "";
      schedName[d][i] = server.hasArg(base + "n") ? server.arg(base + "n") : "";
      schedRoom[d][i] = server.hasArg(base + "r") ? server.arg(base + "r") : "";
      schedTime[d][i].trim();
      schedName[d][i].trim();
      schedRoom[d][i].trim();
      prefs.putString((base + "t").c_str(), schedTime[d][i]);
      prefs.putString((base + "n").c_str(), schedName[d][i]);
      prefs.putString((base + "r").c_str(), schedRoom[d][i]);
    }
  }
  cacheSchedulePage = "";

  // Home Assistant
  if (server.hasArg("haUrl"))   { haUrl   = server.arg("haUrl");   haUrl.trim();   prefs.putString("haUrl", haUrl); }
  if (server.hasArg("haToken")) { haToken = server.arg("haToken"); haToken.trim(); prefs.putString("haToken", haToken); }
  for (int i = 0; i < HA_SENSOR_COUNT; i++) {
    if (server.hasArg("haSe"+String(i))) { haSensorEntity[i] = server.arg("haSe"+String(i)); haSensorEntity[i].trim(); prefs.putString(("haSe"+String(i)).c_str(), haSensorEntity[i]); }
    if (server.hasArg("haSl"+String(i))) { haSensorLabel[i]  = server.arg("haSl"+String(i)); prefs.putString(("haSl"+String(i)).c_str(), haSensorLabel[i]); }
  }
  for (int i = 0; i < HA_CTRL_COUNT; i++) {
    if (server.hasArg("haCe"+String(i))) { haCtrlEntity[i] = server.arg("haCe"+String(i)); haCtrlEntity[i].trim(); prefs.putString(("haCe"+String(i)).c_str(), haCtrlEntity[i]); }
    if (server.hasArg("haCl"+String(i))) { haCtrlLabel[i]  = server.arg("haCl"+String(i)); prefs.putString(("haCl"+String(i)).c_str(), haCtrlLabel[i]); }
    if (server.hasArg("haCt"+String(i))) { haCtrlType[i]   = server.arg("haCt"+String(i)); prefs.putString(("haCt"+String(i)).c_str(), haCtrlType[i]); }
  }
  lastHaFetch = 0;  // force HA refresh after config change
  cacheHaPage = "";

  // Hydration goal
  if (server.hasArg("hydGoal")) {
    hydrationGoal = constrain(server.arg("hydGoal").toInt(), 1, 20);
    prefs.putInt("hydGoal", hydrationGoal);
  }

  // Countdown widget
  if (server.hasArg("cdLabel")) { countdownLabel = server.arg("cdLabel"); countdownLabel.trim(); if (countdownLabel.length() == 0) countdownLabel = "Event"; prefs.putString("cdLabel", countdownLabel); }
  if (server.hasArg("cdDate")) {
    String d = server.arg("cdDate");
    d.replace("-","");
    countdownDate = (d.length() == 8) ? d : "";
    prefs.putString("cdDate", countdownDate);
  }

  // Pomodoro
  pomodoroEnabled = server.hasArg("pomEn");
  prefs.putBool("pomEn", pomodoroEnabled);
  if (server.hasArg("pomWork"))  { pomWorkMin       = constrain(server.arg("pomWork").toInt(),  1, 90); prefs.putInt("pomWork",  pomWorkMin); }
  if (server.hasArg("pomBreak")) { pomBreakMin      = constrain(server.arg("pomBreak").toInt(), 1, 30); prefs.putInt("pomBreak", pomBreakMin); }
  if (server.hasArg("pomLong"))  { pomLongBreakMin  = constrain(server.arg("pomLong").toInt(),  5, 60); prefs.putInt("pomLong",  pomLongBreakMin); }

  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    String key = String("homeSlot") + String(i);
    prefs.putString(key.c_str(), homeWidgetKey(homeWidgetSlots[i]));
  }
  for (int i = 0; i < 6; i++) {
    String key = String("timer") + String(i);
    prefs.putInt(key.c_str(), timerPresetMin[i]);
  }

  applyThemeByKey(newAccent, newBg);
  applyTextColorByKey(newText);
  applyDeviceTimezoneByKey(timezoneKey);
  if (!sleepDimmed && !sleepOff) setBacklight(BL_FULL);

  notesDirty = true;
  pageDirty = true;
  dataDirty = true;

  cacheClock = "";
  cacheFocusTimer = "";
  cacheTimerMenu = "";
  cacheTimerDone = "";
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    cacheHomeSlots[i] = "";
  }

  lastTempText = "";
  lastRainText = "";
  lastKpText = "";
  lastKpLevelText = "";
  lastWindText = "";
  lastWindDirText = "";
  lastNextSunLabel = "";
  lastNextSunTime = "";
  lastUptimeText = "";

  if (locationChanged) resetDataCaches();

  server.sendHeader("Location", "/");
  server.send(303);
}

// ── Sticky Notes API ─────────────────────────────────────────
void handleApiNote() {
  if (server.method() == HTTP_POST) {
    String body = server.arg("plain");
    // Accept JSON {"msg":"..."} or plain text
    String msg = body;
    int mStart = body.indexOf("\"msg\"");
    if (mStart >= 0) {
      int q1 = body.indexOf('"', mStart + 5);
      int q2 = body.indexOf('"', q1 + 1);
      if (q1 >= 0 && q2 > q1) msg = body.substring(q1 + 1, q2);
    }
    msg.trim();
    if (msg.length() > 0) {
      // Shift notes down, add new at top
      for (int i = STICKY_COUNT - 1; i > 0; i--) stickyNote[i] = stickyNote[i-1];
      stickyNote[0] = msg.substring(0, 80);
      for (int i = 0; i < STICKY_COUNT; i++) prefs.putString(("sticky"+String(i)).c_str(), stickyNote[i]);
      stickyDirty = true;
      server.send(200, "application/json", "{\"ok\":true}");
    } else {
      server.send(400, "application/json", "{\"error\":\"empty message\"}");
    }
  } else if (server.method() == HTTP_GET) {
    // GET ?msg=text also works (bookmarkable)
    if (server.hasArg("msg")) {
      String msg = server.arg("msg");
      msg.trim();
      if (msg.length() > 0) {
        for (int i = STICKY_COUNT - 1; i > 0; i--) stickyNote[i] = stickyNote[i-1];
        stickyNote[0] = msg.substring(0, 80);
        for (int i = 0; i < STICKY_COUNT; i++) prefs.putString(("sticky"+String(i)).c_str(), stickyNote[i]);
        stickyDirty = true;
        server.send(200, "text/plain", "Note saved: " + msg);
        return;
      }
    }
    // Return current notes as JSON
    String json = "[";
    for (int i = 0; i < STICKY_COUNT; i++) {
      if (i > 0) json += ",";
      json += "\"" + stickyNote[i] + "\"";
    }
    json += "]";
    server.send(200, "application/json", json);
  } else {
    server.send(405, "text/plain", "Method not allowed");
  }
}

void handleApiNoteClear() {
  int idx = server.hasArg("id") ? server.arg("id").toInt() : -1;
  if (idx >= 0 && idx < STICKY_COUNT) {
    stickyNote[idx] = "";
    prefs.putString(("sticky"+String(idx)).c_str(), "");
  } else {
    for (int i = 0; i < STICKY_COUNT; i++) { stickyNote[i] = ""; prefs.putString(("sticky"+String(i)).c_str(), ""); }
  }
  stickyDirty = true;
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleApiStatus() {
  String json = "{";
  json += "\"uptime\":" + String(millis()/1000);
  json += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += ",\"wifi\":\"" + wifiStatusText() + "\"";
  json += ",\"classes_today\":" + String(scheduleClassCount(scheduleTodayIndex()));
  json += ",\"hydration\":" + String(hydrationCount);
  json += "}";
  server.send(200, "application/json", json);
}

void handleApiHydrationReset() {
  hydrationCount = 0;
  prefs.putInt("hydCount", 0);
  for (int i = 0; i < HOME_SLOT_COUNT; i++) cacheHomeSlots[i] = "";
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleOtaPage() {
  String page = "<!doctype html><html><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Deskbuddy — Update firmware</title>"
    "<style>body{font-family:sans-serif;background:#111;color:#eee;display:flex;flex-direction:column;align-items:center;justify-content:center;min-height:100vh;margin:0;}"
    "h2{margin-bottom:8px;}p{color:#888;font-size:14px;margin-bottom:24px;}"
    "input[type=file]{margin-bottom:16px;}"
    "button{background:#2196F3;color:#fff;border:none;padding:10px 28px;border-radius:8px;font-size:16px;cursor:pointer;}"
    "button:hover{background:#1976D2;}"
    "#status{margin-top:20px;font-size:14px;color:#aaa;}"
    "</style></head><body>"
    "<h2>Firmware update</h2>"
    "<p>Select the <code>firmware.bin</code> from <code>.pio/build/esp32dev/</code></p>"
    "<form method='POST' enctype='multipart/form-data'>"
    "<input type='file' name='firmware' accept='.bin' required><br>"
    "<button type='submit'>Flash firmware</button>"
    "</form>"
    "<div id='status'></div>"
    "<p style='margin-top:32px;font-size:12px;'><a href='/' style='color:#555;'>Back to settings</a></p>"
    "</body></html>";
  server.send(200, "text/html; charset=utf-8", page);
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  // Sticky notes API — callable from any device on the LAN
  server.on("/api/note",  HTTP_POST, handleApiNote);
  server.on("/api/note",  HTTP_GET,  handleApiNote);
  server.on("/api/note/clear", HTTP_POST, handleApiNoteClear);
  // Status JSON API
  server.on("/api/status", HTTP_GET, handleApiStatus);
  server.on("/api/hydration/reset", HTTP_POST, handleApiHydrationReset);

  server.on("/update", HTTP_GET, handleOtaPage);
  server.on("/update", HTTP_POST,
    []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", Update.hasError() ? "Update failed" : "OK. Rebooting...");
      delay(300);
      ESP.restart();
    },
    []() {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        Update.begin(UPDATE_SIZE_UNKNOWN);
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        Update.write(upload.buf, upload.currentSize);
      } else if (upload.status == UPLOAD_FILE_END) {
        Update.end(true);
      }
    }
  );

  server.begin();
}

// =========================================================
// SETUP / LOOP
// =========================================================
void waitForNtpTime() {
  time_t now = time(nullptr);
  unsigned long t0 = millis();
  while (now < NTP_MIN_EPOCH && millis() - t0 < 10000) {
    delay(200);
    now = time(nullptr);
  }
}

void beginWiFiConnect() {
  if (!wifiEnabled) {
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    wifiConnectInProgress = false;
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  wifiConnectInProgress = true;
  wifiConnectStartedMs = millis();
}

void connectWiFi(bool waitForConnection = true) {
  beginWiFiConnect();
  if (!waitForConnection) return;

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(200);
  }
  wifiConnectInProgress = false;
}

void updateWiFiConnectionState() {
  if (!wifiEnabled || !wifiConnectInProgress) return;

  wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    wifiConnectInProgress = false;
    ensureSunTimesForToday();
    ensureWeather();
    ensureKpIndex();
    dataDirty = true;
    pageDirty = true;
    return;
  }

  if (millis() - wifiConnectStartedMs >= WIFI_CONNECT_TIMEOUT_MS) {
    wifiConnectInProgress = false;
    pageDirty = true;
  }
}

void setWifiEnabled(bool enabled) {
  wifiEnabled = enabled;
  prefs.putBool("wifiEnabled", wifiEnabled);

  if (!wifiEnabled) {
    wifiConnectInProgress = false;
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
  } else {
    beginWiFiConnect();
  }

  dataDirty = true;
  pageDirty = true;
}

void setup() {
  Serial.begin(115200);

  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);

  loadStoredSettings();
  lastInteractionMs = millis();

  tft.init();
  tft.setRotation(ROT);
  tft.invertDisplay(INV);
  tft.setSwapBytes(true);

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Booting Deskbuddy...", 10, 10, 2);

  touchSPI.begin(T_SCK, T_MISO, T_MOSI);
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);
  ts.begin(touchSPI);
  ts.setRotation(2);  // physical display orientation is effective rotation 2 (offset_rotation=2 + ROT=0)

  tft.drawString("Connecting WiFi...", 10, 34, 2);
  connectWiFi(true);

  tft.drawString("Syncing time...", 10, 58, 2);
  configTzTime(timezonePosixByKey(timezoneKey),
               "pool.ntp.org", "time.google.com", "time.cloudflare.com");
  waitForNtpTime();

  ensureSunTimesForToday();
  ensureWeather();
  ensureKpIndex();
  setupWebServer();

  pageDirty = true;
  dataDirty = true;
  notesDirty = true;

  drawCurrentPageFull();
  updateCurrentPageDynamic();

  lastClockTick = millis();
  lastDataTick = millis();

  Serial.print("Deskbuddy web: http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  server.handleClient();
  updateWiFiConnectionState();
  updateFocusTimerState();
  updateTimerDoneDialogState();
  handleAutoSleep();

  int tx = 0, ty = 0;
  if (touchNewPress(tx, ty)) {
    lastInteractionMs = millis();

    if (sleepOff) {
      if (manualDimMode) {
        sleepOff = false;
        sleepDimmed = true;
        setBacklight(BL_DIM);
        pageDirty = true;
      } else {
        wakeDisplay();
      }
      return;
    }

    if (handleTimerDoneDialogTouch(tx, ty)) {
      return;
    }

    if (eyeBreakActive) {
      eyeBreakActive = false;
      cacheEyeBreak = "";
      lastEyeBreakMs = millis();
      pageDirty = true;
      return;
    }

    if (moveActive) {
      moveActive = false;
      cacheMove = "";
      lastMoveMs = millis();
      pageDirty = true;
      return;
    }

    if (tx >= SCREEN_W - 36 && ty <= TOPBAR_H) {
      toggleSleepMode();
      pageDirty = true;
    } else if (tx >= 174 && tx <= 206 && ty <= TOPBAR_H) {
      // Info button → toggle Status page
      currentPage = (currentPage == PAGE_STATUS) ? PAGE_HOME : PAGE_STATUS;
      pageDirty = true;
    } else {
      if (sleepDimmed && !manualDimMode) {
        wakeDisplay();
      } else {
        if (!handleHomeTouch(tx, ty) && !handleStatusTouch(tx, ty) &&
            !handleHaTouch(tx, ty)) {
          handleNavTouch(tx, ty);
        }
      }
    }
  }

  if (millis() - lastDataTick >= DATA_TICK_MS) {
    lastDataTick = millis();
    ensureSunTimesForToday();
    ensureWeather();
    ensureKpIndex();
    ensureHaData();
    checkMidnightReset();
    fetchDailyQuote();
  }

  // Reminder triggers
  if (!eyeBreakActive && !moveActive && !timerDoneDialogOpen) {
    if (eyeBreakEnabled && millis() - lastEyeBreakMs >= (unsigned long)eyeBreakIntervalMin * 60000UL) {
      eyeBreakActive  = true;
      eyeBreakStartMs = millis();
      cacheEyeBreak   = "";
      wakeDisplay(false);
    } else if (moveEnabled && millis() - lastMoveMs >= (unsigned long)moveIntervalMin * 60000UL) {
      moveActive = true;
      cacheMove  = "";
      wakeDisplay(false);
    }
  }

  // Eye break auto-dismiss
  if (eyeBreakActive && millis() - eyeBreakStartMs >= EYE_BREAK_DURATION_MS) {
    eyeBreakActive = false;
    cacheEyeBreak  = "";
    lastEyeBreakMs = millis();
    pageDirty = true;
  }

  if (eyeBreakActive) {
    drawEyeBreakOverlay();
  } else if (moveActive) {
    drawMoveReminderOverlay();
  } else if (pageDirty || lastDrawnPage != currentPage) {
    drawCurrentPageFull();
    updateCurrentPageDynamic();
    pageDirty = false;
    dataDirty = false;
  } else if (millis() - lastClockTick >= CLOCK_TICK_MS) {
    lastClockTick = millis();
    updateCurrentPageDynamic();
    dataDirty = false;
  }

  delay(10);
}
