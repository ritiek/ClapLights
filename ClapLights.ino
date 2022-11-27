#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "time.h"

// Comment out to disable Discord webhook message edits.
#define ENABLE_DISCORD_INTEGRATION

#define ONBOARD_LED  2
#define SOUND_SENSOR 4
#define TOUCH_SENSOR 15

// Replace these with your WiFi credentials.
const char* ssid = "ssid";
const char* password = "password";

// That's how this endpoint is for my Tasmota flashed bulb.
const char* get_endpoint_on_sound_detection =
    "http://192.168.1.3/cm?user=me&password=mypass&cmnd=Power0%20Toggle";


#ifdef ENABLE_DISCORD_INTEGRATION
struct DiscordConfiguration {
    const String message_template;
    const char* time_fmt;
    const String ipinfo;
    const String discord_message_hook;
    const int update_interval_in_msecs;
};

DiscordConfiguration config {
    .message_template = "**Internal IP:**\n{internal_ip}\n"
                        "**External IP:**\n{external_ip}\n"
                        "**From:**\n{city}, {region}, {country}, {isp}\n"
                        "**Updated:**\n{time}",
    .time_fmt = "%A, %B %d %Y %H:%M:%S",
    .ipinfo = "https://ipinfo.io/json",
    // Create a webhook and send some message with it. Then Replace the value for
    // `discord_message_hook` below with your webhook and the message ID obtained
    // by sending the previous message.
    // https://discord.com/developers/docs/resources/webhook#execute-webhook
    .discord_message_hook = "https://discord.com/api/webhooks/1234567891234567891/<loong-string>/messages/<message_id>",
    .update_interval_in_msecs = 30000,
};

struct IPInfo {
    String ip;
    String city;
    String region;
    String country;
    String org;
};

/* TaskHandle_t BulbTask; */
TaskHandle_t DiscordTask;
const char* ntp_server = "pool.ntp.org";
const long gmt_offset_in_secs = 19800;
IPInfo ip_info;

bool has_wifi_timed_out = false;
#endif
bool last_sound_value = false;
bool last_touch_value = false;
bool is_state_locked = false;


void toggle_bulb() {
    digitalWrite(ONBOARD_LED, HIGH);
    HTTPClient http;
    Serial.println("Send Network Request To Bulb");
    http.begin(get_endpoint_on_sound_detection);
    http.GET();
    http.end();
    digitalWrite(ONBOARD_LED, LOW);
}


void toggle_bulb_lock_state() {
    digitalWrite(ONBOARD_LED, HIGH);
    is_state_locked = !is_state_locked;
    Serial.print("State: ");
    if (!is_state_locked) {
        Serial.print("Un");
    }
    Serial.println("Locked");
    digitalWrite(ONBOARD_LED, LOW);
}


#ifdef ENABLE_DISCORD_INTEGRATION
void set_ipinfo_fields_through_api_call(DiscordConfiguration config) {
    HTTPClient http;
    DynamicJsonDocument ip_info_deserialized(1024);

    Serial.println("Update IPInfo Fields");
    http.begin(config.ipinfo);
    while (true) {
        int http_code = http.GET();
        if (http_code > 0) {
            break;
        } else {
            delay(2000);
        }
    }
    String response = http.getString();

    deserializeJson(ip_info_deserialized, response);
    ip_info = {
        .ip = ip_info_deserialized["ip"].as<String>(),
        .city = ip_info_deserialized["city"].as<String>(),
        .region = ip_info_deserialized["region"].as<String>(),
        .country = ip_info_deserialized["country"].as<String>(),
        .org = ip_info_deserialized["org"].as<String>(),
    };
}


String partial_format(String message_template, String key, String value) {
    message_template.replace("{" + key + "}", value);
    return message_template;
}


tm get_current_time() {
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    return timeinfo;
}


String get_time_fmt(const char* time_fmt) {
    tm timeinfo = get_current_time();
    char current_time[100] = {0};
    strftime(current_time, sizeof(current_time), time_fmt, &timeinfo);
    return String(current_time);
}


void update_discord_message(const char* message) {
    HTTPClient http;
    DynamicJsonDocument discord_webhook_data(1024);
    String discord_webhook_serialized_data;

    http.begin(config.discord_message_hook);
    http.addHeader("Content-Type", "application/json");
    discord_webhook_data["content"] = message;
    serializeJson(discord_webhook_data, discord_webhook_serialized_data);

    Serial.println("Update Discord Message");
    http.PATCH(discord_webhook_serialized_data);
    http.end();
}


class Message {
  public:
    IPInfo ip_info;
    String message_template;
    const char* time_fmt;

    Message(IPInfo ip_info, String message_template, const char* time_fmt) {
        this->ip_info = ip_info;
        this->time_fmt = time_fmt;
        this->message_template = message_template;
    }

    String build() {
        this->set_ip_info_fields();
        this->set_local_ip_field();
        this->set_time_field();
        return this->message_template;
    }

  private:
    void set_ip_info_fields() {
        String message_template = partial_format(
            this->message_template,
            "external_ip",
            this->ip_info.ip
        );
        message_template = partial_format(
            message_template,
            "city",
            this->ip_info.city
        );
        message_template = partial_format(
            message_template,
            "region",
            this->ip_info.region
        );
        message_template = partial_format(
            message_template,
            "country",
            this->ip_info.country
        );
        message_template = partial_format(
            message_template,
            "isp",
            this->ip_info.org
        );
        this->message_template = message_template;
    }

    void set_local_ip_field() {
        this->message_template = partial_format(
            this->message_template,
            "internal_ip",
            WiFi.localIP().toString()
        );
    }

    void set_time_field() {
        String current_time = get_time_fmt(this->time_fmt);
        this->message_template = partial_format(
            this->message_template,
            "time",
            current_time
        );
    }
};
#endif


void setup() {
    Serial.begin(115200);
    pinMode(ONBOARD_LED, OUTPUT);
    pinMode(SOUND_SENSOR, INPUT);
    pinMode(TOUCH_SENSOR, INPUT);

    digitalWrite(ONBOARD_LED, HIGH);
    delay(4000);
    WiFi.begin(ssid, password);

    String ssid_as_string = ssid;
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to " + ssid_as_string);
    }
    Serial.println("Connected to " + ssid_as_string);
    digitalWrite(ONBOARD_LED, LOW);

    Serial.println("");

    /* xTaskCreatePinnedToCore(bulb_task, "BulbTask", 5000, NULL, 1, &BulbTask, 0); */
#ifdef ENABLE_DISCORD_INTEGRATION
    xTaskCreatePinnedToCore(discord_task, "DiscordTask", 15000, NULL, 2, &DiscordTask, 0);
#endif
}


/*
void bulb_task(void * pvParameters) {
    Serial.println("Executing BulbTask");
    while (true) {
        int touch_value = digitalRead(TOUCH_SENSOR);
        if (touch_value == last_touch_value) {
            int sound_value = digitalRead(SOUND_SENSOR);
            if (sound_value != last_sound_value) {
                last_sound_value = sound_value;
                if (sound_value == 1) {
                    Serial.println("Sound Trigger");
                    toggle_bulb();
                    Serial.println("");
                }
            }
        } else {
            last_touch_value = touch_value;
            if (touch_value == 1) {
                Serial.println("Touch Trigger");
                toggle_bulb();
                Serial.println("");
            }
        }
    }
}
*/


#ifdef ENABLE_DISCORD_INTEGRATION
void discord_task(void * pvParameters) {
    Serial.println("DiscordTask execution started on Core 2");
    configTime(gmt_offset_in_secs, 0, ntp_server);
    set_ipinfo_fields_through_api_call(config);
    while (true) {
        if ((WiFi.status() != WL_CONNECTED)) {
            delay(5000);
            has_wifi_timed_out = true;
            return;
        }
        if (has_wifi_timed_out) {
            set_ipinfo_fields_through_api_call(config);
            has_wifi_timed_out = false;
        }
        Message message = Message(ip_info, config.message_template, config.time_fmt);
        String crafted_message = message.build();
        update_discord_message(crafted_message.c_str());
        Serial.println("");
        delay(config.update_interval_in_msecs);
    }
}
#endif


void loop() {
    bool touch_value = digitalRead(TOUCH_SENSOR);
    if (touch_value == last_touch_value) {
        if (!is_state_locked) {
            bool sound_value = digitalRead(SOUND_SENSOR);
            if (sound_value != last_sound_value) {
                last_sound_value = sound_value;
                if (sound_value == 1) {
                    Serial.println("Sound Trigger");
                    toggle_bulb();
                    Serial.println("");
                }
            }
        }
    } else {
        last_touch_value = touch_value;
        if (touch_value == 1) {
            Serial.println("Touch Trigger");
            toggle_bulb_lock_state();
            Serial.println("");
        }
    }
}
