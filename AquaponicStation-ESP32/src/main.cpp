#define WITH_SERIAL

// Defined by build_flags
// #define ESP32_CAM      /* Build for ESP32-CAM with camera service only */
// #define ESP32_WROOM_32  /* Build for ESP32-WROOM-32 with Temperature, Moisture and pH sensors */

// Device name to be shown on the html page
#if defined(ESP32_CAM)
#define DEVICE_NAME "ESP32-CAM 1"
#elif defined(ESP32_WROOM_32)
#define DEVICE_NAME "ESP32-WROOM-32 1"
#endif

#if defined(WITH_SERIAL)
#define Serial_print(...) Serial.print(__VA_ARGS__)
#define Serial_println(...) Serial.println(__VA_ARGS__)
#define Serial_printf(...) Serial.printf(__VA_ARGS__)
#else
#define Serial_print(...)
#define Serial_println(...)
#define Serial_printf(...)
#endif /* DEBUG */

#include <Arduino.h>

#include <WiFi.h>
#include <esp_http_server.h>
#include <esp_timer.h>

#if defined(ESP32_WROOM_32)
// Libraries for the temperature sensor
#include <OneWire.h>
#include <DallasTemperature.h>
#endif /* ESP32_WROOM_32 */

// ESP32 Cam
#if defined(ESP32_CAM)
#define CAMERA_MODEL_AI_THINKER
#include "esp_camera.h"
#include "camera_pins.h"
#include <img_converters.h>

typedef struct
{
    httpd_req_t *req;
    size_t len;
} jpg_chunking_t;

#define LED_FLASH_PIN 4
#endif /* ESP32_CAM */

#if defined(ESP32_WROOM_32)

// Sensors PINs

// data - white/green, orange - high, blue - low
#define SENSOR_TEMPERATURE_WATER_PIN 16

// data - white+green, brown - high, green - low
#define SENSOR_TEMPERATURE_AIR_PIN 17

// data - white/orange, orange - high, blue - low
#define SENSOR_MOISTURE_PIN 32

// data - white/brown, orange/white - high, white/blue - low
#define SENSOR_PH_PIN 35

// Led PINs
#define LED_GREEN_PIN 27
#define LED_RED_PIN 26

// Temperature sensor definitions
OneWire oneWireTemperatureWater(SENSOR_TEMPERATURE_WATER_PIN);
OneWire oneWireTemperatureAir(SENSOR_TEMPERATURE_AIR_PIN);
DallasTemperature dallasTemperatureWater(&oneWireTemperatureWater);
DallasTemperature dallasTemperatureAir(&oneWireTemperatureAir);
float sensor_temperature_water_c = -1;
float sensor_temperature_water_f = -1;
float sensor_temperature_air_c = -1;
float sensor_temperature_air_f = -1;

// Moisture sensor definitions
int sensor_moisture = -1;

// pH sensor definitions
float sensor_ph = -1;
float voltage_ph = -1;

#define PH_VOLTAGES_LEN 40 // Times of collection
#define PH_OFFSET 0        // Used to calibrate the pH value
#define PH_SAMPLING_INTERVAL 20
int voltages_ph[PH_VOLTAGES_LEN]; // Store the average value of the sensor
int voltages_ph_idx = 0;
#endif /* ESP32_WROOM_32 */

// WiFi access point settings
const char *wifi_ssid = "Alek_Nadia_Family";
const char *wifi_password = "Mishka53";

// HTTP web server
httpd_handle_t httpd = NULL;

// Start of html page
const char *html_head =
    "<!DOCTYPE html><html>"
    "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<link rel=\"icon\" href=\"data:,\">"
    // CSS to style the on/off buttons
    "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}"
    ".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;"
    "text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}"
    ".button2 {background-color: #555555;}</style></head>"

    // Web Page Heading
    "<body><h1>Aquaponics Station - " DEVICE_NAME "</h1>";

// End of html page
const char *html_tail =
    "</body></html>";

void connect_wifi()
{
    WiFi.begin(wifi_ssid, wifi_password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial_print(".");
    }
    Serial_println("WiFi connected on");
    Serial_println(WiFi.localIP());
}

void disconnect_wifi()
{
    WiFi.disconnect();
}

static esp_err_t index_handler(httpd_req_t *req)
{
    String html = html_head;
#if defined(ESP32_WROOM_32)
    html = html + "<p>Water Temperature: " + String(sensor_temperature_water_c) + "&#x2103;, " + String(sensor_temperature_water_f) + "&#x2109;</p>";
    html = html + "<p>Air Temperature: " + String(sensor_temperature_air_c) + "&#x2103;, " + String(sensor_temperature_air_f) + "&#x2109;</p>";
    html = html + "<p>Moisture: " + String(sensor_moisture) + "%</p>";
    html = html + "<p>pH: " + String(sensor_ph) + "</p>";
#endif /* ESP32_WROOM_32 */

#if defined(ESP32_CAM)
    html = html + "<img src='/capture' width='800' height='600'>";
#endif /* ESP32_CAM */
    html = html + html_tail;
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, (const char *)html.c_str(), html.length());
}

#if defined(ESP32_CAM)
static size_t jpg_encode_stream(void *arg, size_t index, const void *data, size_t len)
{
    jpg_chunking_t *j = (jpg_chunking_t *)arg;
    if (!index)
    {
        j->len = 0;
    }
    if (httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK)
    {
        return 0;
    }
    j->len += len;
    return len;
}

static esp_err_t capture_handler(httpd_req_t *req)
{
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    int64_t fr_start = esp_timer_get_time();

    digitalWrite(LED_FLASH_PIN, HIGH);
    delay(250);
    fb = esp_camera_fb_get();
    digitalWrite(LED_FLASH_PIN, LOW);
    if (!fb)
    {
        Serial_println("Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    size_t fb_len = 0;
    if (fb->format == PIXFORMAT_JPEG)
    {
        fb_len = fb->len;
        res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    }
    else
    {
        jpg_chunking_t jchunk = {req, 0};
        res = frame2jpg_cb(fb, 80, jpg_encode_stream, &jchunk) ? ESP_OK : ESP_FAIL;
        httpd_resp_send_chunk(req, NULL, 0);
        fb_len = jchunk.len;
    }
    esp_camera_fb_return(fb);
    int64_t fr_end = esp_timer_get_time();
    Serial_printf("JPG: %uB %ums\n", (uint32_t)(fb_len), (uint32_t)((fr_end - fr_start) / 1000));
    return res;
}
#endif /* ESP32_CAM */

void startHttpServer()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_handler,
        .user_ctx = NULL};

#if defined(ESP32_CAM)
    httpd_uri_t capture_uri = {
        .uri = "/capture",
        .method = HTTP_GET,
        .handler = capture_handler,
        .user_ctx = NULL};
#endif /* ESP32_CAM */

    Serial_printf("Starting web server on port: '%d'\n", config.server_port);
    if (httpd_start(&httpd, &config) == ESP_OK)
    {
        httpd_register_uri_handler(httpd, &index_uri);
#if defined(ESP32_CAM)
        httpd_register_uri_handler(httpd, &capture_uri);
#endif /* ESP32_CAM */
    }
}

#if defined(ESP32_CAM)
void init_camera()
{
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    // init with high specs to pre-allocate larger buffers
    if (psramFound())
    {
        config.frame_size = FRAMESIZE_UXGA;
        config.jpeg_quality = 10;
        config.fb_count = 2;
    }
    else
    {
        config.frame_size = FRAMESIZE_SVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
    }

    // camera init
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        Serial_printf("Camera init failed with error 0x%x", err);
        return;
    }

    sensor_t *s = esp_camera_sensor_get();
    // initial sensors are flipped vertically and colors are a bit saturated
    if (s->id.PID == OV3660_PID)
    {
        Serial_println("Fine tunning OV3660 camera");
        s->set_vflip(s, 1);       //flip it back
        s->set_brightness(s, 1);  //up the blightness just a bit
        s->set_saturation(s, -2); //lower the saturation
    }
    //drop down frame size for higher initial frame rate
    s->set_framesize(s, FRAMESIZE_UXGA);
}
#endif /* ESP32_CAM */

void setup()
{
#if defined(WITH_SERIAL)
    Serial.begin(115200);
#endif

#if defined(ESP32_WROOM_32)
    pinMode(LED_GREEN_PIN, OUTPUT);
    pinMode(LED_RED_PIN, OUTPUT);

    // Start temperature sensors
    dallasTemperatureWater.begin();
    dallasTemperatureAir.begin();

    // Using moisture and pH sensor pins as analog inputs
    pinMode(SENSOR_MOISTURE_PIN, INPUT);
    pinMode(SENSOR_PH_PIN, INPUT);
#endif /* ESP32_WROOM_32 */

    // Init camera
#if defined(ESP32_CAM)
    init_camera();
    pinMode(LED_FLASH_PIN, OUTPUT);
#endif /* ESP32_CAM */

    // Connect to WiFi
    connect_wifi();

    // Start the web server
    startHttpServer();
}

#if defined(ESP32_WROOM_32)
void take_temperatures()
{
    dallasTemperatureWater.requestTemperatures();
    sensor_temperature_water_c = dallasTemperatureWater.getTempCByIndex(0);
    sensor_temperature_water_f = dallasTemperatureWater.getTempFByIndex(0);
    dallasTemperatureAir.requestTemperatures();
    sensor_temperature_air_c = dallasTemperatureAir.getTempCByIndex(0);
    sensor_temperature_air_f = dallasTemperatureAir.getTempFByIndex(0);
}

float voltage_to_moisture(float voltage)
{
    return min(100, (int)map(voltage, 0, 1600, 0, 100));
}

void take_moisture()
{
    sensor_moisture = voltage_to_moisture(analogRead(SENSOR_MOISTURE_PIN));
}

double voltage_to_ph(float voltage)
{
    // voltage of water -> 2000 -> 7
    // voltage of beer -> 500 -> 4
    return 4 + 3 * (voltage - 500) / (2000 - 500);
}

double avergearray(int *arr, int number)
{
    int i;
    int max, min;
    double avg;
    long amount = 0;
    if (number <= 0)
    {
        Serial.println("Error number for the array to avraging!/n");
        return 0;
    }
    if (number < 5)
    { //less than 5, calculated directly statistics
        for (i = 0; i < number; i++)
        {
            amount += arr[i];
        }
        avg = amount / number;
        return avg;
    }
    else
    {
        if (arr[0] < arr[1])
        {
            min = arr[0];
            max = arr[1];
        }
        else
        {
            min = arr[1];
            max = arr[0];
        }
        for (i = 2; i < number; i++)
        {
            if (arr[i] < min)
            {
                amount += min; //arr<min
                min = arr[i];
            }
            else
            {
                if (arr[i] > max)
                {
                    amount += max; //arr>max
                    max = arr[i];
                }
                else
                {
                    amount += arr[i]; //min<=arr<=max
                }
            } //if
        }     //for
        avg = (double)amount / (number - 2);
    } //if
    return avg;
}

void take_ph()
{
    static unsigned long sampling_time = millis();
    if (millis() - sampling_time > PH_SAMPLING_INTERVAL)
    {
        voltage_ph = analogRead(SENSOR_PH_PIN);
        voltages_ph[voltages_ph_idx++] = voltage_ph;
        if (voltages_ph_idx == PH_VOLTAGES_LEN)
            voltages_ph_idx = 0;
        voltage_ph = avergearray(voltages_ph, PH_VOLTAGES_LEN);
        sensor_ph = voltage_to_ph(voltage_ph);
        sampling_time = millis();
    }
}

enum WaterTemperatureCondition { 
    WATER_TEMPERATURE_NORMAL,
    WATER_TOO_COLD,       // causes green to blink slow
    WATER_TOO_HOT         // causes green to blink quick
};

enum WaterPHCondition { 
    WATER_PH_NORMAL,
    WATER_PH_TOO_LOW,    // causes red to blink slow
    WATER_PH_TOO_HIGH    // causes red to blink quick
};

#define SLOW_DELAY 1000
#define FAST_DELAY 100
#define BIG_DELAY millis()
#define TEMPERATURE_LOW_F 50
#define TEMPERATURE_HIGH_F 70
#define PH_LOW 5
#define PH_HIGH 8

int green_last_set = 0;
int red_last_set = 0;

void set_leds()
{
    WaterTemperatureCondition temperatureCondition = WATER_TEMPERATURE_NORMAL;
    if (sensor_temperature_water_f <= TEMPERATURE_LOW_F)
        temperatureCondition = WATER_TOO_COLD;
    else if (sensor_temperature_water_f >= TEMPERATURE_HIGH_F)
        temperatureCondition = WATER_TOO_HOT;

    WaterPHCondition phCondition = WATER_PH_NORMAL;
    if (sensor_ph <= PH_LOW)
        phCondition = WATER_PH_TOO_LOW;
    else if (sensor_ph >= PH_HIGH)
        phCondition = WATER_PH_TOO_HIGH;

    Serial_print("millis=");
    Serial_print(millis());
    Serial_print(", temperatureCondition=");
    Serial_print(temperatureCondition);
    Serial_print(", phCondition=");
    Serial_println(phCondition);

    int green_state = digitalRead(LED_GREEN_PIN);
    int green_delay = BIG_DELAY;
    switch (temperatureCondition)
    {
        case WATER_TEMPERATURE_NORMAL:
            green_state = LOW;
        break;
        case WATER_TOO_COLD:
            // blink slowly
            green_delay = SLOW_DELAY;
        break;
        case WATER_TOO_HOT:
            // blink quickly
            green_delay = FAST_DELAY;
        break;
    }
    if (millis() - green_last_set >= green_delay)
    {
        green_state = !green_state;
        if (green_delay == FAST_DELAY)
        {
            digitalWrite(LED_GREEN_PIN, green_state);
            delay(FAST_DELAY);
            green_state = !green_state;
        }
        green_last_set = millis();
    }
    digitalWrite(LED_GREEN_PIN, green_state);

    int red_state = digitalRead(LED_RED_PIN);
    int red_delay = BIG_DELAY;
    switch (phCondition)
    {
        case WATER_PH_NORMAL:
            red_state = LOW;
        break;
        case WATER_PH_TOO_LOW:
            // blink slowly
            red_delay = SLOW_DELAY;
        break;
        case WATER_PH_TOO_HIGH:
            // blink quickly
            red_delay = FAST_DELAY;
        break;
    }
    if (millis() - red_last_set >= red_delay)
    {
        red_state = !red_state;
        if (red_delay == FAST_DELAY)
        {
            digitalWrite(LED_RED_PIN, red_state);
            delay(FAST_DELAY);
            red_state = !red_state;
        }
        red_last_set = millis();
    }
    digitalWrite(LED_RED_PIN, red_state);
}

#endif /* ESP32_WROOM_32 */

void loop()
{
#if defined(ESP32_WROOM_32)
    take_temperatures();
    take_moisture();
    take_ph();
    set_leds();
#endif /* ESP32_WROOM_32 */
    delay(10);
}
