/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  Navico BR24 Radar Plugin
 * Author:   David Register
 *           Dave Cowell
 *           Kees Verruijt
 *           Douwe Fokkema
 *           Sean D'Epagnier
 ***************************************************************************
 *   Copyright (C) 2010 by David S. Register              bdbcat@yahoo.com *
 *   Copyright (C) 2012-2013 by Dave Cowell                                *
 *   Copyright (C) 2012-2016 by Kees Verruijt         canboat@verruijt.net *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************
 */

#ifndef _BR24RADARPI_H_
#define _BR24RADARPI_H_

#define MY_API_VERSION_MAJOR 1
#define MY_API_VERSION_MINOR 12

#include "pi_common.h"
#include "jsonreader.h"
#include "version.h"
#include "nmea0183/nmea0183.h"

PLUGIN_BEGIN_NAMESPACE

//    Forward definitions
class GuardZone;
class RadarInfo;

class br24ControlsDialog;
class br24MessageBox;
class br24OptionsDialog;
class br24Receive;
class br24Transmit;
class br24radar_pi;

#define SPOKES (4096)               // BR radars can generate up to 4096 spokes per rotation,
#define LINES_PER_ROTATION (2048)   // but use only half that in practice
#define RETURNS_PER_LINE (512)      // BR radars generate 512 separate values per range, at 8 bits each
#define DEGREES_PER_ROTATION (360)  // Classical math
#define RADARS (2)                  // Number of radars supported by this PI. 2 since 4G supports 2. More work
                                    // needed if you intend to add multiple radomes to network!
#define GUARD_ZONES (2)             // Could be increased if wanted
#define BEARING_LINES (2)           // And these as well

typedef int SpokeBearing;  // A value from 0 -- LINES_PER_ROTATION indicating a bearing (? = North,
                           // +ve = clockwise)

// Use the above to convert from 'raw' headings sent by the radar (0..4095) into classical degrees
// (0..359) and back
#define SCALE_RAW_TO_DEGREES(raw) ((raw) * (double)DEGREES_PER_ROTATION / SPOKES)
#define SCALE_RAW_TO_DEGREES2048(raw) ((raw) * (double)DEGREES_PER_ROTATION / LINES_PER_ROTATION)
#define SCALE_DEGREES_TO_RAW(angle) ((int)((angle) * (double)SPOKES / DEGREES_PER_ROTATION))
#define SCALE_DEGREES_TO_RAW2048(angle) ((int)((angle) * (double)LINES_PER_ROTATION / DEGREES_PER_ROTATION))
#define MOD_DEGREES(angle) (fmod(angle + 2 * DEGREES_PER_ROTATION, DEGREES_PER_ROTATION))
#define MOD_ROTATION(raw) (((raw) + 2 * SPOKES) % SPOKES)
#define MOD_ROTATION2048(raw) (((raw) + 2 * LINES_PER_ROTATION) % LINES_PER_ROTATION)

#define WATCHDOG_TIMEOUT (10)  // After 10s assume GPS and heading data is invalid

#define TIMED_OUT(t, timeout) (t >= timeout)
#define NOT_TIMED_OUT(t, timeout) (!TIMED_OUT(t, timeout))

#ifndef M_SETTINGS
#define M_SETTINGS m_pi->m_settings
#endif
#define LOGLEVEL_INFO 0
#define LOGLEVEL_VERBOSE 1
#define LOGLEVEL_DIALOG 2
#define LOGLEVEL_TRANSMIT 4
#define LOGLEVEL_RECEIVE 8
#define LOGLEVEL_GUARD 16
#define IF_LOG_AT_LEVEL(x) if ((M_SETTINGS.verbose & x) != 0)
#define IF_LOG_AT(x, y)       \
  do {                        \
    IF_LOG_AT_LEVEL(x) { y; } \
  } while (0)
#define LOG_INFO wxLogMessage
#define LOG_VERBOSE IF_LOG_AT_LEVEL(LOGLEVEL_VERBOSE) wxLogMessage
#define LOG_DIALOG IF_LOG_AT_LEVEL(LOGLEVEL_DIALOG) wxLogMessage
#define LOG_TRANSMIT IF_LOG_AT_LEVEL(LOGLEVEL_TRANSMIT) wxLogMessage
#define LOG_RECEIVE IF_LOG_AT_LEVEL(LOGLEVEL_RECEIVE) wxLogMessage
#define LOG_GUARD IF_LOG_AT_LEVEL(LOGLEVEL_GUARD) wxLogMessage

enum { BM_ID_RED, BM_ID_RED_SLAVE, BM_ID_GREEN, BM_ID_GREEN_SLAVE, BM_ID_AMBER, BM_ID_AMBER_SLAVE, BM_ID_BLANK, BM_ID_BLANK_SLAVE };

enum HeadingSource { HEADING_NONE, HEADING_HDM, HEADING_HDT, HEADING_COG, HEADING_RADAR };

enum RadarState { RADAR_OFF, RADAR_STANDBY, RADAR_TRANSMIT };

struct receive_statistics {
  int packets;
  int broken_packets;
  int spokes;
  int broken_spokes;
  int missing_spokes;
};

typedef enum ControlType {
  CT_RANGE,
  CT_GAIN,
  CT_SEA,
  CT_RAIN,
  CT_TRANSPARENCY,
  CT_INTERFERENCE_REJECTION,
  CT_TARGET_SEPARATION,
  CT_NOISE_REJECTION,
  CT_TARGET_BOOST,
  CT_TARGET_EXPANSION,
  CT_REFRESHRATE,
  CT_SCAN_SPEED,
  CT_SCAN_AGE,
  CT_TIMED_IDLE,
  CT_BEARING_ALIGNMENT,
  CT_SIDE_LOBE_SUPPRESSION,
  CT_ANTENNA_HEIGHT,
  CT_LOCAL_INTERFERENCE_REJECTION,
  CT_TARGET_TRAILS,
  CT_MAX  // Keep this last, see below
} ControlType;

// The following are only for logging, so don't care about translations.
static string ControlTypeNames[CT_MAX] = {"Range",
                                          "Gain",
                                          "Sea",
                                          "Rain",
                                          "Transparency",
                                          "Interference rejection",
                                          "Target separation",
                                          "Noise rejection",
                                          "Target boost",
                                          "Target expansion",
                                          "Refresh rate",
                                          "Scan speed",
                                          "Scan age",
                                          "Timed idle",
                                          "Bearing alignment",
                                          "Side lobe suppression",
                                          "Antenna height",
                                          "Local interference rejection",
                                          "Target trails"};

typedef enum GuardZoneType { GZ_OFF, GZ_ARC, GZ_CIRCLE } GuardZoneType;

typedef enum RadarType {
  RT_UNKNOWN,
  RT_BR24,  // 3G is just a fancy BR24
  RT_4G
} RadarType;

enum BlobColor {
  BLOB_NONE,
  BLOB_HISTORY_0,
  BLOB_HISTORY_1,
  BLOB_HISTORY_2,
  BLOB_HISTORY_3,
  BLOB_HISTORY_4,
  BLOB_HISTORY_5,
  BLOB_HISTORY_6,
  BLOB_HISTORY_7,
  BLOB_HISTORY_8,
  BLOB_HISTORY_9,
  BLOB_BLUE,
  BLOB_GREEN,
  BLOB_RED
};

extern size_t convertMetersToRadarAllowedValue(int *range_meters, int units, RadarType radar_type);
extern const char *convertRadarToString(int range_meters, int units, int index);
extern double local_distance(double lat1, double lon1, double lat2, double lon2);
extern double local_bearing(double lat1, double lon1, double lat2, double lon2);

enum DisplayModeType { DM_CHART_OVERLAY, DM_CHART_NONE };
enum ToolbarIconColor { TB_RED, TB_AMBER, TB_GREEN };
enum VariationSource { VARIATION_SOURCE_NONE, VARIATION_SOURCE_NMEA, VARIATION_SOURCE_FIX, VARIATION_SOURCE_WMM };

static const int RangeUnitsToMeters[2] = {1852, 1000};

static const bool HasBitCount2[8] = {
    false,  // 000
    false,  // 001
    false,  // 010
    true,   // 011
    false,  // 100
    true,   // 101
    true,   // 110
    true,   // 111
};

#define DEFAULT_OVERLAY_TRANSPARENCY (5)
#define MIN_OVERLAY_TRANSPARENCY (0)
#define MAX_OVERLAY_TRANSPARENCY (10)
#define MIN_AGE (4)
#define MAX_AGE (12)

/**
 * The data that is stored in the opencpn.ini file. Most of this is set in the OptionsDialog,
 * some of it is 'secret' and can only be set by manipulating the ini file directly.
 */
struct PersistentSettings {
  int overlay_transparency;
  int range_index;              // index into range array, see RadarInfo.cpp
  int verbose;                  // Loglevel 0..4.
  int display_option;           // Monocolor-red or Multi-color
  int guard_zone_threshold;     // How many blobs must be sent by radar before we fire alarm
  int guard_zone_render_style;  // 0 = Shading, 1 = Outline, 2 = Shading + Outline
  int guard_zone_on_overlay;    // 0 = false, 1 = true
  int trails_on_overlay;        // 0 = false, 1 = true
  double skew_factor;           // Set to -1 or other value to correct skewing
  int range_units;              // 0 = Nautical miles, 1 = Kilometers
#define RANGE_NAUTICAL (0)
#define RANGE_METRIC (1)
  int range_unit_meters;         // ... 1852 or 1000, depending on range_units
  int max_age;                   // Scans older than this in seconds will be removed
  int timed_idle;                // 0 = off, 1 = 5 mins, etc. to 7 = 35 mins
  int idle_run_time;             // how long, in seconds, should a idle run be? Value < 30 is ignored set to 30.
  int refreshrate;               // How quickly to refresh the display
  int show;                      // whether to show any radar (overlay or window)
  int show_radar[RADARS];        // whether to show radar window
  int transmit_radar[RADARS];    // whether radar should be transmitting (persistent)
  int chart_overlay;             // -1 = none, otherwise = radar number
  int menu_auto_hide;            // 0 = none, 1 = 10s, 2 = 30s
  bool pass_heading_to_opencpn;  //
  bool enable_dual_radar;        // Should the dual radar be enabled for 4G?
  bool emulator_on;              // Emulator, useful when debugging without radar
  int drawing_method;            // VertexBuffer, Shader, etc.
  int ignore_radar_heading;      // For testing purposes
  int reverse_zoom;              // 0 = normal, 1 = reverse
  int threshold_red;
  int threshold_green;
  int threshold_blue;
  int threshold_multi_sweep;
  wxString alert_audio_file;
  wxString mcast_address;
};

struct scan_line {
  int range;                            // range of this scan line in decimeters
  wxLongLong age;                       // how old this scan line is. We keep old scans on-screen for a while
  UINT8 data[RETURNS_PER_LINE + 1];     // radar return strength, data[512] is an additional element,
                                        // accessed in drawing the spokes
  UINT8 history[RETURNS_PER_LINE + 1];  // contains per bit the history of previous scans.
  // Each scan this byte is left shifted one bit. If the strength (=level) of a return is above the
  // threshold
  // a 1 is added in the rightmost position, if below threshold, a 0.
};

//----------------------------------------------------------------------------------------------------------
//    The PlugIn Class Definition
//----------------------------------------------------------------------------------------------------------

#define BR24RADAR_TOOL_POSITION -1  // Request default positioning of toolbar tool

#define PLUGIN_OPTIONS                                                                                                       \
  (WANTS_DYNAMIC_OPENGL_OVERLAY_CALLBACK | WANTS_OPENGL_OVERLAY_CALLBACK | WANTS_OVERLAY_CALLBACK | WANTS_TOOLBAR_CALLBACK | \
   INSTALLS_TOOLBAR_TOOL | USES_AUI_MANAGER | WANTS_CONFIG | WANTS_NMEA_EVENTS | WANTS_NMEA_SENTENCES | WANTS_PREFERENCES |  \
   WANTS_PLUGIN_MESSAGING | WANTS_CURSOR_LATLON | WANTS_MOUSE_EVENTS)

class br24radar_pi : public wxTimer, public opencpn_plugin_112 {
 public:
  br24radar_pi(void *ppimgr);
  ~br24radar_pi();
  void PrepareRadarImage(int angle);

  //    The required PlugIn Methods
  int Init(void);
  bool DeInit(void);

  int GetAPIVersionMajor();
  int GetAPIVersionMinor();
  int GetPlugInVersionMajor();
  int GetPlugInVersionMinor();

  wxBitmap *GetPlugInBitmap();
  wxString GetCommonName();
  wxString GetShortDescription();
  wxString GetLongDescription();

  //    The required override PlugIn Methods
  bool RenderGLOverlay(wxGLContext *pcontext, PlugIn_ViewPort *vp);
  bool RenderOverlay(wxDC &dc, PlugIn_ViewPort *vp);
  void SetPositionFix(PlugIn_Position_Fix &pfix);
  void SetPositionFixEx(PlugIn_Position_Fix_Ex &pfix);
  void SetPluginMessage(wxString &message_id, wxString &message_body);
  void SetNMEASentence(wxString &sentence);
  void SetDefaults(void);
  int GetToolbarToolCount(void);
  void OnToolbarToolCallback(int id);
  void OnContextMenuItemCallback(int id);
  void ShowPreferencesDialog(wxWindow *parent);
  void SetCursorLatLon(double lat, double lon);
  bool MouseEventHook(wxMouseEvent &event);

  // The wxTimer overrides

  void Notify();

  // Other public methods

  void SetRadarWindowViz();

  void OnControlDialogClose(RadarInfo *ri);
  void SetDisplayMode(DisplayModeType mode);

  void ShowRadarControl(int radar, bool show = true);
  void ShowGuardZoneDialog(int radar, int zone);
  void OnGuardZoneDialogClose(RadarInfo *ri);
  void ConfirmGuardZoneBogeys();

  bool SetControlValue(int radar, ControlType controlType, int value);

  // Various state decisions
  bool IsRadarOnScreen(int radar) {
    return m_settings.show > 0 && (m_settings.show_radar[radar] || m_settings.chart_overlay == radar);
  }
  bool IsOverlayOnScreen(int radar) { return m_settings.show > 0 && m_settings.chart_overlay == radar; }

  bool LoadConfig();
  bool SaveConfig();

  long GetRangeMeters();
  long GetOptimalRangeMeters();

  wxString GetGuardZoneText(RadarInfo *ri, bool withTimeout);

  void SetMcastIPAddress(wxString &msg);

  wxFont m_font;      // The dialog font at a normal size
  wxFont m_fat_font;  // The dialog font at a bigger size, bold
  int m_display_width, m_display_height;

  PersistentSettings m_settings;
  RadarInfo *m_radar[RADARS];
  wxString m_perspective[RADARS];  // Temporary storage of window location when plugin is disabled

  br24MessageBox *m_pMessageBox;

  wxGLContext *m_opencpn_gl_context;
  bool m_opencpn_gl_context_broken;

  double m_hdt;          // this is the heading that the pi is using for all heading operations, in degrees.
                         // m_hdt will come from the radar if available else from the NMEA stream.
  time_t m_hdt_timeout;  // When we consider heading is lost

  // Variation. Used to convert magnetic into true heading.
  // Can come from SetPositionFixEx, which may hail from the WMM plugin
  // and is thus to be preferred, or GPS or a NMEA sentence. The latter will probably
  // have an outdated variation model, so is less preferred. Besides, some devices
  // transmit invalid (zero) values. So we also let non-zero values prevail.
  double m_var;  // local magnetic variation, in degrees
  VariationSource m_var_source;
  time_t m_var_timeout;

  HeadingSource m_heading_source;
  bool m_opengl_mode;
  volatile bool m_opengl_mode_changed;
  bool m_bpos_set;
  time_t m_bpos_timestamp;

  // Cursor position. Used to show position in radar window
  double m_cursor_lat, m_cursor_lon;
  double m_ownship_lat, m_ownship_lon;

  bool m_initialized;      // True if Init() succeeded and DeInit() not called yet.
  bool m_first_init;       // True in first Init() call.
  wxLongLong m_boot_time;  // millis when started


 private:
  void RadarSendState(void);
  void UpdateState(void);
  void DoTick(void);
  void Select_Clutter(int req_clutter_index);
  void Select_Rejection(int req_rejection_index);
  void CheckGuardZoneBogeys(void);
  void RenderRadarBuffer(wxDC *pdc, int width, int height);
  void RenderRadarOverlay(wxPoint radar_center, double v_scale_ppm, double rotation);
  void PassHeadingToOpenCPN();
  void CacheSetToolbarToolBitmaps(int bm_id_normal, int bm_id_rollover);
  void CheckTimedTransmit(RadarState state);
  void SetDesiredStateAllRadars(RadarState desiredState);

  wxCriticalSection m_exclusive;  // protects callbacks that come from multiple radars

  wxFileConfig *m_pconfig;
  wxWindow *m_parent_window;
  wxMenu *m_context_menu;
  int m_context_menu_control_id;
  int m_context_menu_show_id;
  int m_context_menu_hide_id;

  int m_tool_id;
  wxBitmap *m_pdeficon;

  //    Controls added to Preferences panel
  wxCheckBox *m_pShowIcon;

  wxBitmap *m_ptemp_icon;
  int m_sent_bm_id_normal;
  int m_sent_bm_id_rollover;

  NMEA0183 m_NMEA0183;

  double llat, llon, ulat, ulon, dist_y, pix_y, v_scale_ppm;

  ToolbarIconColor m_toolbar_button;

  double m_hdm;

  bool m_old_data_seen;

  int m_auto_range_meters;  // What the range should be, at least, when AUTO mode is selected
  int m_previous_auto_range_meters;

  // Timed Transmit
  time_t m_idle_timeout;  // When we will flip transmit/standby in automatic Timed Transmit

#define HEADING_TIMEOUT (5)

  bool m_guard_bogey_confirmed;
  time_t m_alarm_sound_timeout;
#define ALARM_TIMEOUT (10)
};

PLUGIN_END_NAMESPACE

#include "br24OptionsDialog.h"
#include "br24ControlsDialog.h"
#include "br24MessageBox.h"
#include "br24Transmit.h"
#include "GuardZone.h"
#include "RadarInfo.h"

#endif /* _BR24RADAR_PI_H_ */
