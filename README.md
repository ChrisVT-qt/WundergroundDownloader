# WundergroundDownloader
Download data of your Personal Weather Station from Weather Underground to your 
computer.

General goal
- I own a Personal Weather Station (PWS) that records all kind of weather
data (such as temperature, pressure, humidity, wind direction and speed,
sun intensity etc.). The PWS does not allow another device in the same
wireless network to access its data.
- Workaround: It does allow upload of data to Weather Underground
(Wunderground, WU). And you can download your data from there.
- This tool facilitates this activity.

Current status
- This tool takes your PWS device ID and a WU API key that you can
obtain from WU for free.
- It downloads all available data from WU ever houry and stores them
in a local SQL database.
- This works pretty fine except for the fact that WU does not necessarily
provide the latest data for the current day but may (randomly) shorten
the amount of data, e.g. report data until 12 noon only at 5pm.
- The software will obtain the full dataset for any incomplete date
the next day which usually works.
- If not, you can force immediate download data of any date.

# Getting things started...
In order to get started, you will need to put a few things into the 
configuration file `config.h` 
1. Make a copy of `config_empty.h` and name it `config.h`.
2. Set your PWS ID in `WU_PWS_NAME` and the API token in `WU_TOKEN`.
3. Set a directory where stuff gets stored (`WU_ROOT`)
4. Pick a database name where all weather data are stored 
(`WU_DATABASE_FILE`)
5. You can adjust the remaining setting as you want.
6. `qmake`
7. `make`
