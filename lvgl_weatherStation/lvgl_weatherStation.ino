/*  Install the "lvgl" library version 9.2 by kisvegabor to interface with the TFT Display - https://lvgl.io/
    *** IMPORTANT: lv_conf.h available on the internet will probably NOT work with the examples available at Random Nerd Tutorials ***
    *** YOU MUST USE THE lv_conf.h FILE PROVIDED IN THE LINK BELOW IN ORDER TO USE THE EXAMPLES FROM RANDOM NERD TUTORIALS ***
    FULL INSTRUCTIONS AVAILABLE ON HOW CONFIGURE THE LIBRARY: https://RandomNerdTutorials.com/cyd-lvgl/ or https://RandomNerdTutorials.com/esp32-tft-lvgl/   */
#include <lvgl.h>

/*  Install the "TFT_eSPI" library by Bodmer to interface with the TFT Display - https://github.com/Bodmer/TFT_eSPI
    *** IMPORTANT: User_Setup.h available on the internet will probably NOT work with the examples available at Random Nerd Tutorials ***
    *** YOU MUST USE THE User_Setup.h FILE PROVIDED IN THE LINK BELOW IN ORDER TO USE THE EXAMPLES FROM RANDOM NERD TUTORIALS ***
    FULL INSTRUCTIONS AVAILABLE ON HOW CONFIGURE THE LIBRARY: https://RandomNerdTutorials.com/cyd-lvgl/ or https://RandomNerdTutorials.com/esp32-tft-lvgl/   */
#include <TFT_eSPI.h>

#include <bb_captouch.h>

// Touchscreen pins for bb_captouch (FT6336 on ESP32)
#define TOUCH_SDA 25
#define TOUCH_SCL 26
#define TOUCH_RST 33
#define TOUCH_INT 35
#define TOUCH_I2C_FREQ 400000
// Mirror correction for touch coordinates
#define TOUCH_MIRROR_X false
#define TOUCH_MIRROR_Y true
BBCapTouch touch;
TOUCHINFO touch_info;

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* WIFI_SSID = "Your_SSID";
const char* WIFI_PASS = "Your_PASSWORD";

void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connexion WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nWiFi connecté !");
}

// *** FORECAST *** //
float weather_temp_current  = 0;   // température actuelle
float weather_temp_min      = 0;
float weather_temp_max      = 0;
int   weather_code_morning  = 0;   // code à 08h00
int   weather_code_afternoon = 0;  // code à 14h00

// Retourne un symbole texte court compatible avec toutes les polices LVGL
// (les emojis Unicode couleur nécessitent une police spéciale — on utilise ici
//  des caractères ASCII/Latin qui s'affichent sur n'importe quelle config)
const char* weatherSymbol(int code) {
  if (code == 0)                      return "Soleil";
  if (code <= 2)                      return "Eclair.";
  if (code == 3)                      return "Nuageux";
  if (code == 45 || code == 48)       return "Brouill.";
  if (code >= 51 && code <= 55)       return "Bruine";
  if (code >= 61 && code <= 65)       return "Pluie";
  if (code >= 71 && code <= 75)       return "Neige";
  if (code == 95)                     return "Orage";
  return "?";
}

// Si votre police LVGL supporte les emojis UTF-8, remplacez weatherSymbol() par :
// const char* weatherEmoji(int code) {
//   if (code == 0)                    return "\xE2\x98\x80";  // ☀
//   if (code <= 2)                    return "\xE2\x9B\x85";  // ⛅
//   if (code == 3)                    return "\xE2\x98\x81";  // ☁
//   if (code == 45 || code == 48)     return "\U0001F32B";    // 🌫
//   if (code >= 51 && code <= 55)     return "\U0001F326";    // 🌦
//   if (code >= 61 && code <= 65)     return "\U0001F327";    // 🌧
//   if (code >= 71 && code <= 75)     return "\U00002744";    // ❄
//   if (code == 95)                   return "\U000026C8";    // ⛈
//   return "?";
// }

void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  // current_weather : température instantanée
  // daily           : min/max de DEMAIN (index 1, forecast_days=2)
  // hourly          : weathercode de DEMAIN (index 32 = 08h, index 38 = 14h du jour J+1)
  http.begin("https://api.open-meteo.com/v1/forecast"
             "?latitude=44.987&longitude=3.916"
             "&current_weather=true"
             "&daily=temperature_2m_min,temperature_2m_max"
             "&hourly=weathercode"
             "&timezone=Europe%2FParis"
             "&forecast_days=2");

  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();

    // Buffer agrandi : 2 jours hourly = 48 valeurs + current + daily
    StaticJsonDocument<4096> doc;
    deserializeJson(doc, payload);

    // Température actuelle
    weather_temp_current    = doc["current_weather"]["temperature"];

    // Min/Max d'AUJOURD'HUI (index 0 dans daily)
    weather_temp_min        = doc["daily"]["temperature_2m_min"][0];
    weather_temp_max        = doc["daily"]["temperature_2m_max"][0];

    // Codes météo de DEMAIN :
    // hourly index 0..23 = aujourd'hui, 24..47 = demain
    // index 24+8 = 32 => 08h demain, 24+14 = 38 => 14h demain
    weather_code_morning    = doc["hourly"]["weathercode"][32];
    weather_code_afternoon  = doc["hourly"]["weathercode"][38];

    Serial.printf("Actuelle: %.1f°C  Demain Min: %.1f°C  Max: %.1f°C  Matin: %d  AM: %d\n",
      weather_temp_current,
      weather_temp_min, weather_temp_max,
      weather_code_morning, weather_code_afternoon);
  } else {
    Serial.printf("Erreur HTTP: %d\n", httpCode);
  }
  http.end();
}

// *** DISPLAY *** //
// Historique de températures exemple (Celsius)
static const int16_t temp_history[] = {22, 23, 24, 25, 24, 26, 27, 28, 27, 26, 25, 24, 23, 22, 21};
static const size_t temp_history_count = sizeof(temp_history) / sizeof(temp_history[0]);

// Coordonnées tactiles : (x, y) et pression (z)
int x, y, z;

#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

// Pointeurs globaux vers les éléments à mettre à jour dynamiquement
static lv_obj_t * header_title;
static lv_obj_t * card_temp_lbl;
static lv_obj_t * card_min_lbl;
static lv_obj_t * card_max_lbl;
static lv_obj_t * slider_label;

// Logs LVGL vers le moniteur série
void log_print(lv_log_level_t level, const char * buf) {
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}

// Lecture du tactile via bb_captouch / FT6336
void touchscreen_read(lv_indev_t * indev, lv_indev_data_t * data) {
  if (touch.getSamples(&touch_info) && touch_info.count > 0) {
    x = touch_info.x[0];
    y = touch_info.y[0];
    z = touch_info.pressure[0];

    if (TOUCH_MIRROR_X) x = SCREEN_WIDTH  - x;
    if (TOUCH_MIRROR_Y) y = SCREEN_HEIGHT - y;

    x = constrain(x, 0, SCREEN_WIDTH);
    y = constrain(y, 0, SCREEN_HEIGHT);

    data->state   = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// Met à jour le header et les cards avec les données météo fraîches
void updateWeatherDisplay() {
  // Header : "Soleil / Pluie   12° / 28°"
  char buf[64];
  snprintf(buf, sizeof(buf), "%s / %s   %.0f° / %.0f°",
    weatherSymbol(weather_code_morning),
    weatherSymbol(weather_code_afternoon),
    weather_temp_min,
    weather_temp_max);
  lv_label_set_text(header_title, buf);

  // Card Temp actuelle
  char scur[12];
  snprintf(scur, sizeof(scur), "%.1f°C", weather_temp_current);
  lv_label_set_text(card_temp_lbl, scur);

  // Card Min demain
  char smin[12];
  snprintf(smin, sizeof(smin), "%.1f°C", weather_temp_min);
  lv_label_set_text(card_min_lbl, smin);

  // Card Max demain
  char smax[12];
  snprintf(smax, sizeof(smax), "%.1f°C", weather_temp_max);
  lv_label_set_text(card_max_lbl, smax);
}

// Callback bouton Action : rafraîchit la météo
static void action_btn_event(lv_event_t * e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    fetchWeather();
    updateWeatherDisplay();
  }
}

// Callback slider (affichage valeur)
static void slider_event_callback(lv_event_t * e) {
  lv_obj_t * slider = (lv_obj_t*) lv_event_get_target(e);
  char buf[8];
  lv_snprintf(buf, sizeof(buf), "%d%%", (int)lv_slider_get_value(slider));
  lv_label_set_text(slider_label, buf);
  lv_obj_align_to(slider_label, slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
}

void lv_create_main_gui(void) {

  /* ----------- CONTENEUR PRINCIPAL EN COLONNE ----------- */
  lv_obj_t * main = lv_obj_create(lv_screen_active());
  lv_obj_set_size(main, SCREEN_HEIGHT - 10, SCREEN_WIDTH);
  lv_obj_align(main, LV_ALIGN_TOP_MID, 0, 0);

  lv_obj_set_flex_flow(main, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(main, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);

  lv_obj_set_style_pad_row(main, 10, 0);
  lv_obj_set_style_bg_opa(main, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(main, 0, 0);

  lv_obj_set_scroll_dir(main, LV_DIR_VER);
  lv_obj_set_scroll_snap_y(main, LV_SCROLL_SNAP_START);
  lv_obj_scroll_to_y(main, 0, LV_ANIM_OFF);

  /* ---------------- HEADER ---------------- */
  lv_obj_t * header = lv_obj_create(main);
  lv_obj_set_size(header, SCREEN_HEIGHT, 50);
  lv_obj_set_style_bg_color(header, lv_color_hex(0x2C2C3A), 0);
  lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(header, 0, 0);

  // Label météo (symbole matin/PM + min/max) — mis à jour après fetchWeather()
  lv_obj_t * title = lv_label_create(header);
  lv_label_set_text(title, "chargement...");
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  header_title = title;   // pointeur global

  // Label fixe "demain" à droite du header
  lv_obj_t * time_lbl = lv_label_create(header);
  lv_label_set_text(time_lbl, "demain");
  lv_obj_align(time_lbl, LV_ALIGN_RIGHT_MID, -10, 0);
  lv_obj_set_style_text_color(time_lbl, lv_color_hex(0xD3DEF0), 0);

  /* ---------------- CARDS ---------------- */
  lv_obj_t * card_container = lv_obj_create(main);
  lv_obj_set_size(card_container, SCREEN_HEIGHT, 100);
  lv_obj_set_flex_flow(card_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(card_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_bg_opa(card_container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(card_container, 0, 0);

  // Fonction lambda pour créer une card et retourner son label valeur
  auto make_card = [&](const char * card_title, const char * init_value, lv_obj_t ** out_val_lbl) {
    lv_obj_t * card = lv_obj_create(card_container);
    lv_obj_set_size(card, 90, 70);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1E1E2F), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 0, 0);

    lv_obj_t * lbl1 = lv_label_create(card);
    lv_label_set_text(lbl1, card_title);
    lv_obj_align(lbl1, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_text_color(lbl1, lv_color_white(), 0);

    lv_obj_t * lbl2 = lv_label_create(card);
    lv_label_set_text(lbl2, init_value);
    lv_obj_align(lbl2, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_text_color(lbl2, lv_color_white(), 0);

    if (out_val_lbl) *out_val_lbl = lbl2;   // expose le label valeur si demandé
    return card;
  };

  // Card Temp actuelle — pointeur global pour updateWeatherDisplay()
  make_card("Temp", "--.-°C", &card_temp_lbl);

  // Cards Min et Max demain — pointeurs globaux pour updateWeatherDisplay()
  make_card("Min.", "--.-°C", &card_min_lbl);
  make_card("Max",  "--.-°C", &card_max_lbl);

  /* ---------------- GRAPHIQUE ---------------- */
  lv_obj_t * chart = lv_chart_create(main);
  lv_obj_set_size(chart, SCREEN_HEIGHT, 150);
  lv_obj_center(chart);

  lv_obj_set_style_bg_color(chart, lv_color_hex(0x1E1E2F), 0);
  lv_obj_set_style_bg_opa(chart, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(chart, 12, 0);
  lv_obj_set_style_border_width(chart, 0, 0);

  lv_chart_set_div_line_count(chart, 5, 5);
  lv_obj_set_style_line_color(chart, lv_color_hex(0x444444), LV_PART_MAIN);
  lv_obj_set_style_line_opa(chart, LV_OPA_40, LV_PART_MAIN);

  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(chart, temp_history_count);
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 18, 30);

  lv_chart_series_t * s = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_CYAN), LV_CHART_AXIS_PRIMARY_Y);
  lv_obj_set_style_line_width(chart, 3, LV_PART_ITEMS);
  lv_obj_set_style_line_rounded(chart, true, LV_PART_ITEMS);

  for (size_t i = 0; i < temp_history_count; i++) {
    lv_chart_set_next_value(chart, s, temp_history[i]);
  }

  /* ---------------- CONTROLS ---------------- */
  lv_obj_t * ctrl_container = lv_obj_create(main);
  lv_obj_set_size(ctrl_container, SCREEN_HEIGHT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(ctrl_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(ctrl_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_bg_opa(ctrl_container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ctrl_container, 0, 0);

  lv_obj_t * sw = lv_switch_create(ctrl_container);
  lv_obj_set_size(sw, 60, 30);

  lv_obj_t * slider = lv_slider_create(ctrl_container);
  lv_obj_set_width(slider, SCREEN_HEIGHT - 40);
  lv_slider_set_range(slider, 0, 100);
  lv_obj_add_event_cb(slider, slider_event_callback, LV_EVENT_VALUE_CHANGED, NULL);

  slider_label = lv_label_create(ctrl_container);
  lv_label_set_text(slider_label, "0%");

  lv_obj_t * btn = lv_button_create(ctrl_container);
  lv_obj_set_size(btn, 120, 40);
  lv_obj_t * lbl = lv_label_create(btn);
  lv_label_set_text(lbl, "Actualiser");
  lv_obj_center(lbl);
  lv_obj_add_event_cb(btn, action_btn_event, LV_EVENT_CLICKED, NULL);

  lv_obj_scroll_to_y(main, -100, LV_ANIM_OFF);
}

void setup() {
  Serial.begin(115200);
  String LVGL_Arduino = String("LVGL Library Version: ")
    + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.println(LVGL_Arduino);

  // Démarrage LVGL
  lv_init();
  lv_log_register_print_cb(log_print);

  // Initialisation du contrôleur tactile FT6336 via bb_captouch
  if (touch.init(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_INT, TOUCH_I2C_FREQ, &Wire) != CT_SUCCESS) {
    Serial.println("bb_captouch init failed");
  } else {
    Serial.println("bb_captouch init OK");
  }
  touch.setOrientation(0, SCREEN_WIDTH, SCREEN_HEIGHT);

  // Création de l'affichage TFT via TFT_eSPI
  lv_display_t * disp;
  disp = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, sizeof(draw_buf));
  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);

  // Périphérique d'entrée LVGL (tactile)
  lv_indev_t * indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchscreen_read);

  // Construction de l'interface graphique
  lv_create_main_gui();

  // Connexion WiFi + récupération météo initiale
  connectWiFi();
  fetchWeather();
  updateWeatherDisplay();
}

void loop() {
  lv_task_handler();  // laisse LVGL gérer l'interface
  lv_tick_inc(5);     // informe LVGL du temps écoulé
  delay(5);
}
