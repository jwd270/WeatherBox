# WeatherBox
Arduino Script For Hackerbox 087

As most of my hackerbox projects go, this is a work in progress.  I just need a couple more days to finish it.... ;)

## Internet Weather
I used the national weather service API instead of OpenWeather, because it is open and does not require registering an account or any API keys.  However, it is limited to the USA.  Documentation can be fonund on their [website](https://www.weather.gov/documentation/services-web-api#/default/alerts_active).

To change the weather station, you just need to update the station ID in the URL.  For example, to use the weather information from Boston International Airport, add a 'K' to the airport identifier.

If you search for your location on weather.gov, the weather station ID is typically in parenthesis by the name of the station.  Go to [weather.gov](weather.gov) and search for your city or zip code.  For example, Circleville West Virginia uses Jennings Randolph Field who's identifier is KEKN.
```
#define WEATHER_URL "https://api.weather.gov/stations/KEKN/observations/latest?require_qc=false"
```

## Lightning Sensor
The lightnign sensor works, but the settings are not optimal.  In my loccation it seems to be overwhelmed by disturbers, and does not reliably detect actual lightning strikes.  YMMV.
