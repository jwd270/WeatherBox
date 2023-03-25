//#include <FreeRTOS.h>
#include <TFT_eSPI.h>
#include <Bme280.h>
#include <WiFi.h>
#include <time.h>
#include <SPI.h>
#include <string.h>
#include "ArduinoJson.h"
#include <HTTPClient.h>
#include "SparkFun_AS3935.h"
#include "Free_Fonts.h"





#define wifi_ssid   "*************"
#define wifi_pwd    "***************"
#define INDOOR 0x12
#define LIGHTNING_INT 0x08
#define DISTURBER_INT 0x04
#define NOISE_INT 0x01
#define LSENSE_CS   12
#define LSENSE_INT  13
#define BUTTON_1    6
#define BUTTON_2    18
#define WEATHER_URL "https://api.weather.gov/stations/KBOS/observations/latest?require_qc=false"

#define timezone_offset       -14400
#define INET_UPDATE_INTERVAL  900
#define STORM_INTERVAL        180

Bme280TwoWire sensor;
TFT_eSPI tft = TFT_eSPI();

TFT_eSprite title_block = TFT_eSprite(&tft);
TFT_eSprite indoor_data = TFT_eSprite(&tft);
TFT_eSprite outdoor_data = TFT_eSprite(&tft);
TFT_eSprite status_bar = TFT_eSprite(&tft);
TFT_eSprite storm_data = TFT_eSprite(&tft);
TFT_eSprite plot_data = TFT_eSprite(&tft);

SparkFun_AS3935 lsensor;

IPAddress local_ip;
float tempF = 0.0;
float inet_tempF = 0.0;
float pressure = 0.0;
float humidity = 0.0;
int btn1_clicks = 0;
int btn2_clicks = 0;
int strikes = 0;
int storm_distance = 0;
int lightning_noise_cnt = 0;
time_t current_time = 0;
time_t inet_weather_time = 0;
time_t last_strike_time = 0;
uint32_t delay_cnt = 0;

bool inet_temp_valid = false;
bool active_storm = false;


enum button_states {
    BUTTON_UP,
    BUTTON_DOWN
};

void setup()
{
    Wire.begin();
    SPI1.setRX(8);
    SPI1.setTX(11);
    SPI1.setSCK(10);
    SPI1.begin();
    Serial.begin(115200);
    sensor.begin(Bme280TwoWireAddress::Primary);
    sensor.setSettings(Bme280Settings::indoor());
    pinMode(BUTTON_1, INPUT_PULLDOWN);
    pinMode(BUTTON_2, INPUT_PULLDOWN);
    pinMode(LSENSE_INT, INPUT_PULLDOWN);
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid, wifi_pwd);
    while(WiFi.status() != WL_CONNECTED){delay(500);};
    local_ip = WiFi.localIP(); 
    sensor.begin(Bme280TwoWireAddress::Primary);
    sensor.setSettings(Bme280Settings::indoor());
    set_time();	
    lsensor.beginSPI(LSENSE_CS, 2000000, SPI1);
    lsensor.maskDisturber(true);
    lsensor.setIndoorOutdoor(INDOOR);
    lsensor.setNoiseLevel(1);
    lsensor.spikeRejection(1);
}

void setup1(){
    tft.init();
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK);
    title_block.createSprite(TFT_HEIGHT, 48);
    indoor_data.createSprite(TFT_HEIGHT/2, 72);
    outdoor_data.createSprite(TFT_HEIGHT/2, 72);
    storm_data.createSprite(TFT_HEIGHT/2, 120);
}

void loop()
{
    if (lightning_event()){
        update_lightning_data();
    }

    if (button_1_clicked()){
        lsensor.setIndoorOutdoor(OUTDOOR);
        Serial.println("Outdoor Mode");
 
    }
    
    if (button_2_clicked()){
        lsensor.setIndoorOutdoor(INDOOR);
        Serial.println("Indoor Mode");
    }

    if((last_strike_time + INET_UPDATE_INTERVAL) < current_time){
      strikes = 0;
      storm_distance = 50;
      active_storm = false;      
    }

    if ((current_time < (last_strike_time + STORM_INTERVAL)) && !active_storm && (strikes > 3)){
      active_storm = true;
    }
    
    delay(20);
}

void loop1(){
    update_time();
    update_sensor_data();
    draw_title(0,0);
    draw_indoor(0, 48);
    draw_outdoor(160, 48);
    draw_storm_data(160, 120);
    if (current_time > (inet_weather_time + INET_UPDATE_INTERVAL)){
        get_internet_weather();
    }
    delay(1000);
}

void set_time(){
    NTP.begin("pool.ntp.org");
    NTP.waitSet();
    Serial.println("Time Set...");
}

void update_time(void){
    current_time = time(nullptr);
    current_time += timezone_offset;
}

void draw_title(int x, int y){
    struct tm time_info;
    char tm_str[40];
    gmtime_r(&current_time, &time_info);
    strftime(tm_str, sizeof(tm_str), "%e %b %y %I:%M:%S %P", &time_info);
    title_block.fillSprite(TFT_BLACK);
    title_block.setFreeFont(FF18);
    title_block.setTextColor(TFT_WHITE, TFT_BLACK);
    title_block.drawCentreString("Current Weather", 160, 5, 1);
    title_block.setFreeFont(FF17);
    title_block.drawCentreString(tm_str, 160, 30, 2);
    title_block.pushSprite(x, y);
}

void draw_indoor(int x, int y){
    String temp_str = String(tempF, 1) + " F";
    indoor_data.fillSprite(TFT_BLACK);
    indoor_data.fillSmoothRoundRect(0,0, 160, 72, 5, TFT_SKYBLUE);
    indoor_data.setFreeFont(FSS9);
    indoor_data.setTextColor(TFT_BLACK, TFT_SKYBLUE);
    indoor_data.drawCentreString("Indoor", 80, 5, 1);
    indoor_data.setFreeFont(FSSB24);
    indoor_data.drawCentreString(temp_str, 80, 25, 1);
    indoor_data.pushSprite(x, y);
}

void draw_outdoor(int x, int y){
    String temp_str;
    if (inet_temp_valid){
        temp_str = String(inet_tempF, 1) + "F";
    } else {
        temp_str = "N/A";
    }
    struct tm time_info;
    char tm_str[40];
    gmtime_r(&inet_weather_time, &time_info);
    strftime(tm_str, sizeof(tm_str), "%I:%M %P", &time_info);
    outdoor_data.fillSprite(TFT_BLACK);
    outdoor_data.fillSmoothRoundRect(0,0, 160, 72, 5, TFT_GREENYELLOW);
    outdoor_data.setFreeFont(FSS9);
    outdoor_data.setTextColor(TFT_BLACK, TFT_GREENYELLOW);
    outdoor_data.drawCentreString("Outdoor", 80, 5, 1);
    outdoor_data.setFreeFont(FSSB24);
    outdoor_data.drawCentreString(temp_str, 80, 25, 1);
    outdoor_data.setFreeFont(TT1);
    outdoor_data.drawCentreString(tm_str, 80, 65, 1);
    outdoor_data.pushSprite(x, y);
}

void draw_storm_data(int x, int y){
    storm_data.fillSprite(TFT_BLACK);
    uint32_t fill_color = TFT_BLUE;
    uint32_t text_color = TFT_BLACK;
    if (active_storm){
        fill_color = TFT_RED;
        text_color = TFT_WHITE;
    } else {
        fill_color = TFT_BLUE;
        text_color = TFT_BLACK;
    }
    storm_data.setFreeFont(FSS9);
    storm_data.setTextColor(text_color);
    storm_data.fillSmoothRoundRect(0,0,160,120,5,fill_color);
    storm_data.drawCentreString("Strikes", 80, 5, 1);
    storm_data.drawCentreString("Distance", 80, 45, 1);
    storm_data.setFreeFont(FSS18);
    storm_data.drawCentreString(String(strikes), 80, 25, 1);
    if(storm_distance > 40){
        storm_data.drawCentreString("N/A", 80, 65, 1);
    } else {
        storm_data.drawCentreString(String(storm_distance), 80, 65, 1);
    }
    
    storm_data.pushSprite(x, y);
}

bool get_internet_weather(void){
    if(WiFi.status() != WL_CONNECTED)
    {
        Serial.println("Wifi Not Connected...");
        return false;
    }

    HTTPClient inet_weather;
    bool success = false; 
    inet_weather.setInsecure();
    inet_weather.setTimeout(20);
    inet_weather.begin(WEATHER_URL);
    int res = inet_weather.GET();
   
    Serial.println("Result: " + String(res));
    if (res == 200){
        DynamicJsonDocument doc(4096);
        Serial.println("Weather Connection Successful");
        DeserializationError err = deserializeJson(doc, inet_weather.getString().c_str(), strlen(inet_weather.getString().c_str()));
        if (err){
            Serial.println("Deserialization Failed...");
            Serial.println(err.c_str());
            return false;
        }
        JsonObject inet_weather_data = doc.as<JsonObject>();
        JsonObject weather_properties = doc["properties"];
        Serial.println("Temp:" + String(weather_properties["temperature"]["value"]));
        auto tempC = weather_properties["temperature"]["value"].as<float>();
        inet_tempF = (tempC * 9)/5 + 32;
        inet_weather_time = (time(nullptr) + timezone_offset);
        inet_temp_valid = true;

        success = true;
    }
    else {
        Serial.println("Weather connection failed.");
        success = false;
    }
    inet_weather.end();
    return success;
}

void print_diag(time_t *now){
    struct tm timeinfo;
    char tm_str[12];
    gmtime_r(now, &timeinfo);
    strftime(tm_str, sizeof(tm_str), "%I:%M:%S %P", &timeinfo);

    Serial.print("Local IP: ");
    Serial.println(local_ip.toString());
    Serial.print("Current Time: ");
    Serial.println(tm_str);
}

void update_sensor_data(void){
    float tempC = sensor.getTemperature();
    tempF = ((tempC*9)/5) + 32;
    pressure = sensor.getPressure() / 100;
    humidity = sensor.getHumidity();
}

void update_lightning_data(void){
    int intVal = lsensor.readInterruptReg();
    
    switch(intVal){
        case NOISE_INT:
            break;
            lightning_noise_cnt++;
            Serial.println("Lightning Noise");
        case DISTURBER_INT:
        //Serial.println("Lightning Disturber Event");
            break;
        case LIGHTNING_INT:
            strikes++;
            storm_distance = lsensor.distanceToStorm();
            last_strike_time = time(nullptr) + timezone_offset; 
            break;
        default:
            break;
    }
}

bool lightning_event(void){
    if (digitalRead(LSENSE_INT) == 1)
        return true;
    else
        return false;
}

bool button_1_clicked(void){
    static button_states button_state = BUTTON_UP;

    if (button_state == BUTTON_UP){
        if(digitalRead(BUTTON_1) == 1){
            button_state = BUTTON_DOWN;
            return false;
        } 
    }
    else {
            if(digitalRead(BUTTON_1) == 0){
                button_state = BUTTON_UP;
                btn1_clicks++;
                return true;
            }
    }
    return false;
}

bool button_2_clicked(void){
    static button_states button_state = BUTTON_UP;
    if (button_state == BUTTON_UP){
        if(digitalRead(BUTTON_2) == 1){
            button_state = BUTTON_DOWN;
            return false;
        }
    }
    else {
            if(digitalRead(BUTTON_2) == 0){
                button_state = BUTTON_UP;
                btn2_clicks++;
                return true;
            }
    }
     return false;
}

void lightning_status(void){
    int spikeVal = lsensor.readSpikeRejection();
    int noiseLvl = lsensor.readNoiseLevel();
    int threshold = lsensor.readLightningThreshold();
    int wdog = lsensor.readWatchdogThreshold();
    Serial.println("Lightning Sensor Status");
    Serial.printf("Noise Level: %i\n", noiseLvl);
    Serial.printf("Spike Value: %i\n", spikeVal);
    Serial.printf("Watch Dog: %i\n", wdog);
}