#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include "weather_data.h"

#define API_URL "https://api.open-meteo.com/v1/forecast"
#define WIND_SPEED_THRESHOLD 20.0 
#define TEMPERATURE_THRESHOLD 20.0 

typedef struct {
    char *memory;
    size_t size;
} MemoryChunk;

// Function to send alerts using zenity
void send_alert(const char *message) {
    char command[512];
    snprintf(command, sizeof(command), "DISPLAY=:0 zenity --warning --text=\"%s\"", message);
    
    int ret = system(command);
    if (ret == -1) {
        perror("Error executing system command");
    } else if (WEXITSTATUS(ret) != 0) {
        fprintf(stderr, "Zenity command failed with exit code %d\n", WEXITSTATUS(ret));
    }
}

// Function to check thresholds and trigger alerts
void check_thresholds(const WeatherData *data) {
    if (data->windspeed_10m > WIND_SPEED_THRESHOLD) {
        char alert_message[256];
        snprintf(alert_message, sizeof(alert_message), 
                 "High Wind Speed Alert for %s: %.2f m/s", data->city_name, data->windspeed_10m);
        send_alert(alert_message);
    }

    if (data->temperature_2m > TEMPERATURE_THRESHOLD) {
        char alert_message[256];
        snprintf(alert_message, sizeof(alert_message), 
                 "High Temperature Alert for %s: %.2f °C", data->city_name, data->temperature_2m);
        send_alert(alert_message);
    }
}

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    MemoryChunk *mem = (MemoryChunk *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) {
        fprintf(stderr, "Not enough memory for realloc\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = '\0';

    return realsize;
}

// Fetch weather data for a given city
void fetch_weather_data(const char *city, double latitude, double longitude, WeatherData *data) {
    CURL *curl;
    CURLcode res;
    char url[512];
    MemoryChunk chunk = {NULL, 0};

    snprintf(url, sizeof(url), "%s?latitude=%.4f&longitude=%.4f&hourly=windspeed_10m,temperature_2m&current_weather=true",
             API_URL, latitude, longitude);

    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "CURL initialization failed\n");
        return;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "CURL request failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return;
    }

    // Parsing the JSON response
    cJSON *json = cJSON_Parse(chunk.memory);
    if (!json) {
        fprintf(stderr, "JSON Parsing Error for %s: %s\n", city, cJSON_GetErrorPtr());
        curl_easy_cleanup(curl);
        free(chunk.memory);
        return;
    }

    cJSON *current_weather = cJSON_GetObjectItemCaseSensitive(json, "current_weather");
    if (!current_weather) {
        fprintf(stderr, "Current weather data missing for %s\n", city);
        cJSON_Delete(json);
        curl_easy_cleanup(curl);
        free(chunk.memory);
        return;
    }

    // Parsing the weather variables
    cJSON *windspeed = cJSON_GetObjectItemCaseSensitive(current_weather, "windspeed");
    cJSON *temperature = cJSON_GetObjectItemCaseSensitive(current_weather, "temperature");
    cJSON *is_day = cJSON_GetObjectItemCaseSensitive(current_weather, "is_day");

    snprintf(data->city_name, sizeof(data->city_name), "%s", city);
    data->windspeed_10m = cJSON_IsNumber(windspeed) ? windspeed->valuedouble : 0.0;
    data->temperature_2m = cJSON_IsNumber(temperature) ? temperature->valuedouble : 0.0;
    data->is_day = cJSON_IsNumber(is_day) ? is_day->valueint : 0;
    
    printf("Parsed Data for %s: Wind Speed = %.2f, Temperature = %.2f, Is Day = %d\n",
           data->city_name, data->windspeed_10m, data->temperature_2m, data->is_day);
    // Cleanup
    cJSON_Delete(json);
    curl_easy_cleanup(curl);
    free(chunk.memory);
}

void write_raw_data(const WeatherData *data) {
    FILE *file = fopen("raw_data.txt", "a");
    if (!file) {
        perror("Failed to open raw_data.txt");
        return;
    }

    fprintf(file, "City: %s, Wind Speed: %.2f, Temperature: %.2f, Is Day: %d\n",
        data->city_name, data->windspeed_10m, data->temperature_2m, data->is_day);


    fclose(file);
}

void write_processed_data(double average_windspeed) {
    FILE *file = fopen("processed_data.txt", "a");
    if (!file) {
        perror("Failed to open processed_data.txt");
        return;
    }

    fprintf(file, "Average Wind Speed: %.2f\n", average_windspeed);

    fclose(file);
}

void calculate_average_windspeed() {
    const City cities[] = {
        {"Karachi", 24.8608, 67.0104},
        {"Lahore", 31.5580, 74.3507},
        {"Islamabad", 33.7215, 73.0433},
        {"Quetta", 30.1841, 67.0014},
        {"Peshawar", 34.008, 71.5785},
    };

    size_t num_cities = sizeof(cities) / sizeof(cities[0]);
    WeatherData *data = malloc(num_cities * sizeof(WeatherData));
    if (!data) {
        fprintf(stderr, "Memory allocation failed\n");
        return;
    }

    double total_windspeed = 0.0;
    for (size_t i = 0; i < num_cities; ++i) {
        fetch_weather_data(cities[i].city_name, cities[i].latitude, cities[i].longitude, &data[i]);
        write_raw_data(&data[i]);
        check_thresholds(&data[i]); // Check thresholds and send alerts
        total_windspeed += data[i].windspeed_10m;
    }

    double average_windspeed = total_windspeed / num_cities;
    printf("Average Wind Speed: %.2f\n", average_windspeed);
    write_processed_data(average_windspeed);

    free(data);
}

int main() {
    printf("Fetching weather data...\n");
    calculate_average_windspeed();  
    printf("Weather data processing complete.\n");
    return 0;
}
