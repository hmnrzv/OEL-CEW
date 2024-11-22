#ifndef WEATHER_DATA_H
#define WEATHER_DATA_H

typedef struct {
    const char *city_name;
    double latitude;
    double longitude;
} City;

typedef struct {
    char city_name[50];
    double windspeed_10m;
    double temperature_2m;
    double precipitation;
    int is_day;
} WeatherData;

// Function declarations
void fetch_weather_data(const char *city, double latitude, double longitude, WeatherData *data);
void write_raw_data(const WeatherData *data);
void write_processed_data(double average_windspeed);
void calculate_average_windspeed();

#endif 
