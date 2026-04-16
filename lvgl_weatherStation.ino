
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

const char* WIFI_SSID = "wwt-IoT";
const char* WIFI_PASS = "test1234";

void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connexion WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nWiFi connecté !");
}

//* ** FORECAST *** //
String weather_text = "Chargement...";
float weather_temp = 0;

String decodeWeatherCode(int code) {
  switch(code) {
    case 0: return "Ciel clair";
    case 1: case 2: return "Partiellement nuageux";
    case 3: return "Nuageux";
    case 45: case 48: return "Brouillard";
    case 51: case 53: case 55: return "Bruine";
    case 61: case 63: case 65: return "Pluie";
    case 71: case 73: case 75: return "Neige";
    case 95: return "Orage";
    default: return "Inconnu";
  }
}

void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin("https://api.open-meteo.com/v1/forecast?latitude=44.987&longitude=3.916&current_weather=true");

  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();

    StaticJsonDocument<512> doc;
    deserializeJson(doc, payload);

    float temp = doc["current_weather"]["temperature"];
    int code = doc["current_weather"]["weathercode"];

    weather_temp = temp;
    weather_text = decodeWeatherCode(code);

    Serial.println("Météo mise à jour !");
  }
  http.end();
}

//* ** DISPLAY *** //
// Example temperature history in Celsius
static const int16_t temp_history[] = {22, 23, 24, 25, 24, 26, 27, 28, 27, 26, 25, 24, 23, 22, 21};
static const size_t temp_history_count = sizeof(temp_history) / sizeof(temp_history[0]);

// Touchscreen coordinates: (x, y) and pressure (z)
int x, y, z;

#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

// If logging is enabled, it will inform the user about what is happening in the library
void log_print(lv_log_level_t level, const char * buf) {
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}

// Get the Touchscreen data from the FT6336 controller via bb_captouch
void touchscreen_read(lv_indev_t * indev, lv_indev_data_t * data) {
  if (touch.getSamples(&touch_info) && touch_info.count > 0) {
    x = touch_info.x[0];
    y = touch_info.y[0];
    z = touch_info.pressure[0];

    if (TOUCH_MIRROR_X) {
      x = SCREEN_WIDTH - x;
    }
    if (TOUCH_MIRROR_Y) {
      y = SCREEN_HEIGHT - y;
    }

    x = constrain(x, 0, SCREEN_WIDTH);
    y = constrain(y, 0, SCREEN_HEIGHT);

    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

int btn1_count = 0;
// Callback that is triggered when btn1 is clicked
static void event_handler_btn1(lv_event_t * e) {
  lv_event_code_t code = lv_event_get_code(e);
  if(code == LV_EVENT_CLICKED) {
    btn1_count++;
    LV_LOG_USER("Button clicked %d", (int)btn1_count);
  }
}

// Callback that is triggered when btn2 is clicked/toggled
static void event_handler_btn2(lv_event_t * e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t * obj = (lv_obj_t*) lv_event_get_target(e);
  if(code == LV_EVENT_VALUE_CHANGED) {
    LV_UNUSED(obj);
    LV_LOG_USER("Toggled %s", lv_obj_has_state(obj, LV_STATE_CHECKED) ? "on" : "off");
  }
}

static lv_obj_t * slider_label;
// Callback that prints the current slider value on the TFT display and Serial Monitor for debugging purposes
static void slider_event_callback(lv_event_t * e) {
  lv_obj_t * slider = (lv_obj_t*) lv_event_get_target(e);
  char buf[8];
  lv_snprintf(buf, sizeof(buf), "%d%%", (int)lv_slider_get_value(slider));
  lv_label_set_text(slider_label, buf);
  lv_obj_align_to(slider_label, slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
  LV_LOG_USER("Slider changed to %d%%", (int)lv_slider_get_value(slider));
}

static lv_obj_t * header_title;

static void action_btn_event(lv_event_t * e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    fetchWeather();

    char meteo_buf[64];
    snprintf(meteo_buf, sizeof(meteo_buf), "%.1f°C - %s", weather_temp, weather_text.c_str());
    lv_label_set_text(header_title, meteo_buf);
  }
}

void lv_create_main_gui(void) {

  /* ----------- CONTENEUR PRINCIPAL EN COLONNE ----------- */
  lv_obj_t * main = lv_obj_create(lv_screen_active());
  lv_obj_set_size(main, SCREEN_HEIGHT - 10, SCREEN_WIDTH);  // largeur réduite
  lv_obj_align(main, LV_ALIGN_TOP_MID, 0, 0);

  lv_obj_set_flex_flow(main, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(main, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);

  lv_obj_set_style_pad_row(main, 10, 0);
  lv_obj_set_style_bg_opa(main, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(main, 0, 0);

  // Scroll pour éviter le centrage automatique
  lv_obj_set_scroll_dir(main, LV_DIR_VER);
  lv_obj_set_scroll_snap_y(main, LV_SCROLL_SNAP_START);
  lv_obj_scroll_to_y(main, 0, LV_ANIM_OFF);

  /* ---------------- HEADER ---------------- */
  lv_obj_t * header = lv_obj_create(main);
  lv_obj_set_size(header, SCREEN_HEIGHT, 50);
  lv_obj_set_style_bg_color(header, lv_color_hex(0x2C2C3A), 0);
  lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(header, 0, 0);

  lv_obj_t * title = lv_label_create(header);
  char meteo_buf[64];
  snprintf(meteo_buf, sizeof(meteo_buf), "waiting for weather...");
  lv_label_set_text(title, meteo_buf);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);
  header_title = title; // mémorise le label pour mise à jour

  lv_obj_t * time_lbl = lv_label_create(header);
  lv_label_set_text(time_lbl, "12:45");
  lv_obj_align(time_lbl, LV_ALIGN_RIGHT_MID, -10, 0);

  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_set_style_text_color(time_lbl, lv_color_hex(0xd3def0), 0);

  /* ---------------- CARDS ---------------- */
  lv_obj_t * card_container = lv_obj_create(main);
  lv_obj_set_size(card_container, SCREEN_HEIGHT, 100);
  lv_obj_set_flex_flow(card_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(card_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_bg_opa(card_container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(card_container, 0, 0);

  auto make_card = [&](const char * title, const char * value) {
    lv_obj_t * card = lv_obj_create(card_container);
    lv_obj_set_size(card, 90, 70);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1E1E2F), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 0, 0);

    lv_obj_t * lbl1 = lv_label_create(card);
    lv_label_set_text(lbl1, title);
    lv_obj_align(lbl1, LV_ALIGN_TOP_MID, 0, 5);

    lv_obj_t * lbl2 = lv_label_create(card);
    lv_label_set_text(lbl2, value);
    lv_obj_align(lbl2, LV_ALIGN_BOTTOM_MID, 0, -5);

  lv_obj_set_style_text_color(lbl1, lv_color_white(), 0);
  lv_obj_set_style_text_color(lbl2, lv_color_white(), 0);


    return card;
  };

  make_card("Temp", "24,2°C");
  make_card("Min.", "-11,5°C");
  make_card("Max", "31,4°C");


  /* ---------------- GRAPHIQUE ---------------- */
  lv_obj_t * chart = lv_chart_create(main);
  //lv_obj_set_size(chart, SCREEN_HEIGHT - 20, 150);
  lv_obj_set_size(chart, SCREEN_HEIGHT, 150);
  lv_obj_center(chart);   // force le centrage dans le conteneur

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


  /* ---------------- CONTROLS ----------------*/ 
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

  lv_obj_t * btn = lv_button_create(ctrl_container);
  lv_obj_set_size(btn, 120, 40);
  lv_obj_t * lbl = lv_label_create(btn);
  lv_label_set_text(lbl, "Action");
  lv_obj_center(lbl);
  lv_obj_add_event_cb(btn, action_btn_event, LV_EVENT_CLICKED, NULL);
  
  lv_obj_scroll_to_y(main, -100, LV_ANIM_OFF);

}

void setup() {
  String LVGL_Arduino = String("LVGL Library Version: ") + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.begin(115200);
  Serial.println(LVGL_Arduino);
  
  // Start LVGL
  lv_init();
  // Register print function for debugging
  lv_log_register_print_cb(log_print);

  // Initialize FT6336 touch controller using bb_captouch
  if (touch.init(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_INT, TOUCH_I2C_FREQ, &Wire) != CT_SUCCESS) {
    Serial.println("bb_captouch init failed");
  } else {
    Serial.println("bb_captouch init OK");
  }
  touch.setOrientation(0, SCREEN_WIDTH, SCREEN_HEIGHT);

  
  // Create a display object
  lv_display_t * disp;
  // Initialize the TFT display using the TFT_eSPI library
  disp = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, sizeof(draw_buf));
  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);
    
  // Initialize an LVGL input device object (Touchscreen)
  lv_indev_t * indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  // Set the callback function to read Touchscreen input
  lv_indev_set_read_cb(indev, touchscreen_read);

  // Function to draw the GUI (text, buttons and sliders)
  lv_create_main_gui();

  connectWiFi();
  fetchWeather();

  char meteo_buf[64];
  snprintf(meteo_buf, sizeof(meteo_buf), "%.1f°C - %s", weather_temp, weather_text.c_str());
  lv_label_set_text(header_title, meteo_buf);
}

void loop() {
  lv_task_handler();  // let the GUI do its work
  lv_tick_inc(5);     // tell LVGL how much time has passed
  delay(5);           // let this time pass
}