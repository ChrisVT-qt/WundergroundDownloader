// WundergroundDownloader - Download PWS data from WU
// Copyright (C) 2025 Chris von Toerne
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
// Contact the author by email: christian.vontoerne@gmail.com

// WundergroundComms.cpp
// Class implementation

// Project includes
#include "CallTracer.h"
#include "Config.h"
#include "DatabaseHelper.h"
#include "MessageLogger.h"
#include "WundergroundComms.h"

// Qt includes
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTimer>

#define DEBUG false



// ================================================================== Lifecycle



///////////////////////////////////////////////////////////////////////////////
// Constructor
WundergroundComms::WundergroundComms()
{
    CALL_IN("");

    // Create some directories
    QDir dir("/");
    if (!dir.exists(WU_DATABASE_DIR))
    {
        dir.mkpath(WU_DATABASE_DIR);
    }

    // Initialize key mapper
    Initialize_WUToDB();

    // Database not connected
    m_DatabaseConnected = false;

    // Not running from the get go (needs to be configured first)
    m_IsRunning = false;

    // Set up network access
    m_NetworkAccessManager = new QNetworkAccessManager(this);
    connect (m_NetworkAccessManager, SIGNAL(finished(QNetworkReply *)),
        this, SLOT(HandleResponse(QNetworkReply *)));

    // Timestamp for bot start
    m_StartDateTime = QDateTime::currentDateTime();

    // Start periodic updates
    // (won't actually do anything because m_IsRunning is false)
    Periodic_GetUpdates();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Destructor
WundergroundComms::~WundergroundComms()
{
    CALL_IN("");

    delete m_NetworkAccessManager;

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Instanciator
WundergroundComms * WundergroundComms::Instance()
{
    CALL_IN("");

    // Check if we already have an instance
    if (!m_Instance)
    {
        // Nope. Create one.
        m_Instance = new WundergroundComms;
    }

    // Return instance
    CALL_OUT("");
    return m_Instance;
}



///////////////////////////////////////////////////////////////////////////////
// Instance
WundergroundComms * WundergroundComms::m_Instance = nullptr;



// =================================================================== Database



///////////////////////////////////////////////////////////////////////////////
// Set database name
void WundergroundComms::SetDatabaseFile(const QString & mcrFilename)
{
    CALL_IN(QString("mcrFilename=%1")
        .arg(CALL_SHOW(mcrFilename)));

    // Check if filename is empty
    if (mcrFilename.isEmpty())
    {
        const QString reason = tr("Empty database filename provided.");
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT(reason);
        return;
    }

    // Check if database is already connected
    if (m_DatabaseConnected)
    {
        const QString reason =
            tr("Cannot set database filename; database is already connected.");
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT(reason);
        return;
    }

    // Set database filename
    m_DatabaseFilename = mcrFilename;

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Open database
bool WundergroundComms::OpenDatabase()
{
    CALL_IN("");

    // Check if database is already open
    if (m_DatabaseConnected)
    {
        const QString reason = tr("Cannot open an already open database.");
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT(reason);
        return false;
    }

    // Check if the filename has been set
    if (m_DatabaseFilename.isEmpty())
    {
        const QString reason = tr("No database name specified.");
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT(reason);
        return false;
    }

    // Check if the file exists
    const bool database_initialized = QFile::exists(m_DatabaseFilename);

    // Try to connect
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(m_DatabaseFilename);
    const bool successfully_connected = db.open();
    if (!successfully_connected)
    {
        const QString reason = tr("Could not open %1")
            .arg(m_DatabaseFilename);
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT(reason);
        return false;
    }

    if (!database_initialized)
    {
        // Create database
        const bool success = CreateDatabase();
        if (!success)
        {
            const QString reason = tr("Could not set up database in %1")
                .arg(m_DatabaseFilename);
            MessageLogger::Error(CALL_METHOD, reason);
            CALL_OUT(reason);
            return false;
        }
    }

    // Update database
    UpdateDatabase();

    // Database is connected now
    m_DatabaseConnected = true;

    // Read database
    const bool success = ReadDatabase();

    // Check for date range and incomplete data sets
    QHash < QString, int > observation_count;
    for (int observation = 0;
         observation < m_WeatherData.size();
         observation++)
    {
        const QString date = m_WeatherData[observation]["date_time"].left(10);
        observation_count[date]++;
    }
    QStringList sorted_dates = observation_count.keys();
    if (!sorted_dates.isEmpty())
    {
        std::sort(sorted_dates.begin(), sorted_dates.end());
        const QString min_date = sorted_dates.first();
        const QString max_date = sorted_dates.last();
        QDate this_date = QDate::fromString(min_date, "yyyy-MM-dd");
        const QDate end_date = QDate::fromString(max_date, "yyyy-MM-dd");
        QStringList no_data;
        QStringList incomplete_data;
        while (this_date <= end_date)
        {
            const QString this_date_text = this_date.toString("yyyy-MM-dd");
            if (!observation_count.contains(this_date_text))
            {
                no_data << this_date_text;
            } else if (observation_count[this_date_text] < 24*12 &&
                this_date_text != min_date &&
                this_date_text != max_date)
            {
                incomplete_data << this_date_text;
            }
            this_date = this_date.addDays(1);
        }
        QString message = tr("Observations range from %1 to %2.")
            .arg(min_date,
                max_date);
        if (!incomplete_data.isEmpty())
        {
            message += tr("<br/>Incomplete data for dates %1.")
                .arg(incomplete_data.join(", "));
        }
        if (!no_data.isEmpty())
        {
            message += tr("<br/>No data for dates %1.")
                .arg(no_data.join(", "));
        }
        emit StatusUpdate(message);
    }

    CALL_OUT("");
    return success;
}



///////////////////////////////////////////////////////////////////////////////
// Create database
bool WundergroundComms::CreateDatabase()
{
    CALL_IN("");

    // Check if database is already connected
    if (m_DatabaseConnected)
    {
        const QString reason =
            tr("Cannot initialize database; it is already connected.");
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT(reason);
        return false;
    }

    // Create table
    QSqlQuery query;
    query.exec(QString("CREATE TABLE wu_data ("
        "station_id text, "
        "timezone text, "
        "date_time datetime, "
        "latitude float, "
        "longitude float, "
        "solar_radiation_high float, "
        "uv_high float, "
        "wind_direction_avg_degree float, "
        "humidity_high_percent float, "
        "humidity_low_percent float, "
        "humidity_avg_percent float, "
        "temperature_high_c float, "
        "temperature_low_c float, "
        "temperature_avg_c float, "
        "windspeed_high_kmh float, "
        "windspeed_low_kmh float, "
        "windspeed_avg_kmh float, "
        "wind_gust_high_kmh float, "
        "wind_gust_low_kmh float, "
        "wind_gust_avg_kmh float, "
        "dew_point_high_c float, "
        "dew_point_low_c float, "
        "dew_point_avg_c float, "
        "wind_chill_high_c float, "
        "wind_chill_low_c float, "
        "wind_chill_avg_c float, "
        "heat_index_high_c float, "
        "heat_index_low_c float, "
        "heat_index_avg_c float, "
        "pressure_max_hpa float, "
        "pressure_min_hpa float, "
        "pressure_trend_hpa float, "
        "precipitation_rate_mm float, "
        "precipitation_total_mm float);"));
    if (DatabaseHelper::HasSQLError(query, __FILE__, __LINE__))
    {
        const QString reason = tr("SQL error creating table \"wu_data\"");
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT(reason);
        return false;
    }

    CALL_OUT("");
    return true;
}



///////////////////////////////////////////////////////////////////////////////
// Read database
bool WundergroundComms::ReadDatabase()
{
    CALL_IN("");

    // Check if have aleady read data
    if (!m_WeatherData.isEmpty())
    {
        const QString reason = tr("Weather data have already been read.");
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT(reason);
        return false;
    }

    // Check if database has been connected
    if (!m_DatabaseConnected)
    {
        const QString reason =
            tr("Cannot read database; it has not been connected yet.");
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT(reason);
        return false;
    }

    // Read table
    QStringList columns = m_WUToDB.values();
    columns.removeAll("");
    std::sort(columns.begin(), columns.end());
    QSqlQuery query;
    query.exec(QString("SELECT %1 from wu_data;")
        .arg(columns.join(", ")));
    if (DatabaseHelper::HasSQLError(query, __FILE__, __LINE__))
    {
        const QString reason = tr("SQL error reading table \"wu_data\"");
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT(reason);
        return false;
    }
    while (query.next())
    {
        QHash < QString, QString > line;
        for (int col_index = 0; col_index < columns.size(); col_index++)
        {
            if (columns[col_index] == "station_id" ||
                columns[col_index] == "timezone")
            {
                line[columns[col_index]] = query.value(col_index).toString();
            } else if (columns[col_index] == "date_time")
            {
                const QDateTime value = query.value(col_index).toDateTime();
                line[columns[col_index]] =
                    value.toString("yyyy-MM-dd hh:mm:ss");
            } else
            {
                const float value = query.value(col_index).toFloat();
                line[columns[col_index]] = QString::number(value, 'f', 1);
            }
        }
        m_WeatherData << line;

        const QString station_id = line["station_id"];
        const QString date_time = line["date_time"];
        m_StationToDateTimes[station_id] += date_time;
    }

    emit StatusUpdate(tr("Database read; %1 stations, %2 records in total.")
        .arg(QString::number(m_StationToDateTimes.size()),
             QString::number(m_WeatherData.size())));

    CALL_OUT("");
    return true;
}



///////////////////////////////////////////////////////////////////////////////
// Save observation to database
bool WundergroundComms::SaveToDatabase(
    const QHash < QString, QString > & mcrObservation)
{
    CALL_IN(QString("mcrObservation=%1")
        .arg(CALL_SHOW(mcrObservation)));

    // Check if database has been connected
    if (!m_DatabaseConnected)
    {
        const QString reason = tr("Cannot save observation to database; "
            "it has not been connected yet.");
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT(reason);
        return false;
    }

    // Save observation to table
    QStringList columns = m_WUToDB.values();
    columns.removeAll("");
    std::sort(columns.begin(), columns.end());
    QSqlQuery query;
    query.prepare(QString("INSERT INTO wu_data (%1) VALUES (:%2);")
        .arg(columns.join(", "),
             columns.join(", :")));
    for (auto column_iterator = columns.constBegin();
         column_iterator != columns.constEnd();
         column_iterator++)
    {
        const QString & column = *column_iterator;
        query.bindValue(":" + column, mcrObservation[column]);
    }
    query.exec();
    if (DatabaseHelper::HasSQLError(query, __FILE__, __LINE__))
    {
        const QString reason =
            tr("SQL error adding observation to \"wu_data\"");
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT(reason);
        return false;
    }

    CALL_OUT("");
    return true;
}



///////////////////////////////////////////////////////////////////////////////
// Update database
void WundergroundComms::UpdateDatabase()
{
    CALL_IN("");


    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Mapping of WU columns to database columns
void WundergroundComms::Initialize_WUToDB()
{
    CALL_IN("");

    m_WUToDB["dewptAvg"] = "dew_point_avg_c";
    m_WUToDB["dewptHigh"] = "dew_point_high_c";
    m_WUToDB["dewptLow"] = "dew_point_low_c";
    m_WUToDB["epoch"] = "";
    m_WUToDB["heatindexAvg"] = "heat_index_avg_c";
    m_WUToDB["heatindexHigh"] = "heat_index_high_c";
    m_WUToDB["heatindexLow"] = "heat_index_low_c";
    m_WUToDB["humidityAvg"] = "humidity_avg_percent";
    m_WUToDB["humidityHigh"] = "humidity_high_percent";
    m_WUToDB["humidityLow"] = "humidity_low_percent";
    m_WUToDB["lat"] = "latitude";
    m_WUToDB["lon"] = "longitude";
    m_WUToDB["obsTimeLocal"] = "date_time";
    m_WUToDB["obsTimeUtc"] = "";
    m_WUToDB["precipRate"] = "precipitation_rate_mm";
    m_WUToDB["precipTotal"] = "precipitation_total_mm";
    m_WUToDB["pressureMax"] = "pressure_max_hpa";
    m_WUToDB["pressureMin"] = "pressure_min_hpa";
    m_WUToDB["pressureTrend"] = "pressure_trend_hpa";
    m_WUToDB["qcStatus"] = "";
    m_WUToDB["solarRadiationHigh"] = "solar_radiation_high";
    m_WUToDB["stationID"] = "station_id";
    m_WUToDB["tempAvg"] = "temperature_avg_c";
    m_WUToDB["tempHigh"] = "temperature_high_c";
    m_WUToDB["tempLow"] = "temperature_low_c";
    m_WUToDB["tz"] = "timezone";
    m_WUToDB["uvHigh"] = "uv_high";
    m_WUToDB["windchillAvg"] = "wind_chill_avg_c";
    m_WUToDB["windchillHigh"] = "wind_chill_high_c";
    m_WUToDB["windchillLow"] = "wind_chill_low_c";
    m_WUToDB["winddirAvg"] = "wind_direction_avg_degree";
    m_WUToDB["windgustAvg"] = "wind_gust_avg_kmh";
    m_WUToDB["windgustHigh"] = "wind_gust_high_kmh";
    m_WUToDB["windgustLow"] = "wind_gust_low_kmh";
    m_WUToDB["windspeedAvg"] = "windspeed_avg_kmh";
    m_WUToDB["windspeedHigh"] = "windspeed_high_kmh";
    m_WUToDB["windspeedLow"] = "windspeed_low_kmh";

    CALL_OUT("");
}



// ====================================================================== Setup



///////////////////////////////////////////////////////////////////////////////
// Bot name
bool WundergroundComms::SetPWSName(const QString & mcrPWSName)
{
    CALL_IN(QString("mcrPWSName=%1")
        .arg(CALL_SHOW(mcrPWSName)));

    // Can't be empty
    if (mcrPWSName.isEmpty())
    {
        const QString reason = tr("PWS station name cannot be empty.");
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT(reason);
        return false;
    }

    // Can't be set already
    if (!m_PWSName.isEmpty())
    {
        const QString reason = tr("PWS station name has already been set.");
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT(reason);
        return false;
    }

    m_PWSName = mcrPWSName;

    CALL_OUT("");
    return true;
}



///////////////////////////////////////////////////////////////////////////////
// Set bot token
bool WundergroundComms::SetToken(const QString & mcrToken)
{
    CALL_IN(QString("mcrToken=%1")
        .arg(CALL_SHOW(mcrToken)));

    // Catch empty token
    if (mcrToken.isEmpty())
    {
        const QString reason = tr("Empty token provided.");
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT(reason);
        return false;
    }

    // Can't be set already
    if (!m_Token.isEmpty())
    {
        const QString reason =
            tr("Weather Underground token has already been set.");
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT(reason);
        return false;
    }

    // Check if token is valid
    static const QRegularExpression format_token("^[0-9a-z]{32}$");
    QRegularExpressionMatch match_token = format_token.match(mcrToken);
    if (!match_token.hasMatch())
    {
        const QString reason = tr("Token does not have a valid format: \"%1\"")
            .arg(mcrToken);
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT(reason);
        return false;
    }

    // Set new token
    m_Token = mcrToken;

    CALL_OUT("");
    return true;
}



///////////////////////////////////////////////////////////////////////////////
// Start updates
void WundergroundComms::StartUpdates()
{
    CALL_IN("");

    // Check if updates are already running
    if (m_IsRunning)
    {
        const QString reason = tr("Updates are already running.");
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT("");
        return;
    }

    // Check if PWS name is set
    if (m_PWSName.isEmpty())
    {
        const QString reason = tr("PWS name has not been set.");
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT("");
        return;
    }

    // Check if token is set
    if (m_Token.isEmpty())
    {
        const QString reason = tr("Token has not been set.");
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT("");
        return;
    }

    // Check if database is open
    if (!m_DatabaseConnected)
    {
        const QString reason = tr("Database is not connected.");
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT("");
        return;
    }

    // Bot is running
    m_IsRunning = true;

    // Remember start date/time
    m_StartDateTime = QDateTime::currentDateTime();

    // Log message
    const QString message =
        tr("Started updates from Weather Underground on %1")
            .arg(m_StartDateTime.toString("dd MMM yyyy, hh:mm:ss"));
    emit StatusUpdate(message);

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Stop updates
void WundergroundComms::StopUpdates()
{
    CALL_IN("");

    // Check if bot is already running
    if (!m_IsRunning)
    {
        const QString reason = tr("Updates not running.");
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT("");
        return;
    }

    // Bot is not running
    m_IsRunning = false;

    // Log message
    const QString now =
        QDateTime::currentDateTime().toString("dd MMM yyyy hh:mm:ss");
    const QString message =
        tr("Stopped updates from Weather Underground on %1")
            .arg(now);
    emit StatusUpdate(message);

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Uptime
QString WundergroundComms::GetUptime() const
{
    CALL_IN("");

    const qint64 secs = m_StartDateTime.secsTo(QDateTime::currentDateTime());
    QString uptime =
        QString::number(secs/3600) + ":" +
        ("0" + QString::number((secs/60)%60)).right(2) + ":" +
        ("0" + QString::number(secs%60)).right(2);

    CALL_OUT("");
    return uptime;
}



// ==================================================== Reading from the Server



///////////////////////////////////////////////////////////////////////////////
// For Periodically getting updates
void WundergroundComms::Periodic_GetUpdates()
{
    CALL_IN("");

    while (m_IsRunning)
    {
        // Check if we're in the range where we get updates
        const QString now = QTime::currentTime().toString("hh:mm");
        if (now >= CHECK_END_TIME ||
            now <= CHECK_START_TIME)
        {
            // Nope.
            const QString message =
                tr("Downloading data paused; resuming at %1.")
                    .arg(CHECK_START_TIME);
            emit StatusUpdate(message);
            break;
        }

        // Check if we switched the date
        static QString old_date =
            QDate::currentDate().addDays(-1).toString("yyyyMMdd");
        const QString today = QDate::currentDate().toString("yyyyMMdd");
        if (today != old_date)
        {
            // Get final update for yesterday (will then be complete data)
            GetDate(old_date);
            old_date = today;
            break;
        }

        // Regular update fetch
        GetDate(today);
        break;
    }

    // Try again in a bit
    QTimer::singleShot(CHECK_FOR_UPDATES_DELAY,
        this, &WundergroundComms::Periodic_GetUpdates);

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Get a particular date
void WundergroundComms::GetDate(const QString & mcrDate)
{
    CALL_IN(QString("mcrDate=%1")
        .arg(CALL_SHOW(mcrDate)));

    // Downloads will occur regardless of regular updates being paused.

    // Check if PWS name is set
    if (m_PWSName.isEmpty())
    {
        const QString reason = tr("PWS name has not been set.");
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT("");
        return;
    }

    // Check if token is set
    if (m_Token.isEmpty())
    {
        const QString reason = tr("Token has not been set.");
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT("");
        return;
    }

    // Check if database is connected
    if (!m_DatabaseConnected)
    {
        const QString reason = tr("Database is not connected.");
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT(reason);
        return;
    }

    // Trigger download
    QString url = QString("https://api.weather.com/v2/pws/history/all?"
        "stationId=%1&"
        "format=json&"
        "units=m&"
        "numericPrecision=decimal&"
        "date=%2&"
        "apiKey=%3")
        .arg(m_PWSName,
             mcrDate,
             m_Token);
    QNetworkRequest request;
    void finished(QNetworkReply * mpResponse);
    void readyRead(QNetworkReply * mpResponse);
    request.setUrl(url);
    m_NetworkAccessManager -> get(request);

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Handle response
bool WundergroundComms::HandleResponse(QNetworkReply * mpResponse)
{
    CALL_IN(QString("mpResponse=%1")
        .arg(CALL_SHOW(mpResponse)));

    // Check if we have an error
    const int network_error = mpResponse -> error();
    if (network_error != QNetworkReply::NoError)
    {
        // No response
        const QString reason =
            tr("An error has occurred processing the network request (%1).")
                .arg(QString::number(network_error));
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT(reason);
        return false;
    }

    // Read content of the response
    const QByteArray content = mpResponse -> readAll();
    if (content.isEmpty())
    {
        const QString reason = tr("No response content received");
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT(reason);
        return false;
    }

    // Parse JSON
    QJsonDocument doc_response = QJsonDocument::fromJson(content);
    if (doc_response.isNull())
    {
        // Not JSON format
        const QString reason = tr("No JSON response received");
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT(reason);
        return false;
    }
    if (DEBUG)
    {
        qDebug().noquote() << doc_response.toJson(QJsonDocument::Indented);
    }

    // Parse observations
    QJsonObject response = doc_response.object();
    bool success = Parse_Observations(response);

    CALL_OUT("");
    return success;
}



///////////////////////////////////////////////////////////////////////////////
// Array of observations
bool WundergroundComms::Parse_Observations(const QJsonObject & mcrObservations)
{
    CALL_IN(QString("mcrObservations=%1")
        .arg(CALL_SHOW(mcrObservations)));

    // {
    //   "observations": [...]
    // }

    // All keys
    int num_observations = 0;
    QString imported_date;
    QString min_time;
    QString max_time;
    for (auto key_iterator = mcrObservations.constBegin();
         key_iterator != mcrObservations.constEnd();
         key_iterator++)
    {
        const QString & key = key_iterator.key();
        if (key == "observations")
        {
            QJsonArray json_observations = mcrObservations[key].toArray();
            for (int index = 0; index < json_observations.size(); index++)
            {
                QJsonObject single_observation =
                    json_observations[index].toObject();
                const bool success =
                    Parse_SingleObservation(single_observation);
                if (!success)
                {
                    // Issue has already been reported,
                    CALL_OUT("");
                    return false;
                }

                // Min and max time
                const QString date_time_value =
                    single_observation["obsTimeLocal"].toString();
                const QDateTime date_time = QDateTime::fromString(
                    date_time_value, "yyyy-MM-dd hh:mm:ss");

                const QString this_time = date_time.toString("hh:mm:ss");
                if (min_time.isEmpty())
                {
                    min_time = this_time;
                } else
                {
                    min_time = qMin(min_time, this_time);
                }
                if (max_time.isEmpty())
                {
                    max_time = this_time;
                } else
                {
                    max_time = qMax(max_time, this_time);
                }

                // Dates that have been imported
                imported_date =date_time.toString("yyyy-MM-dd");

                num_observations++;
            }
            continue;
        }

        // Unknown key
        const QString message = tr("Unknown key \"%1\" in response [ignored]")
            .arg(key);
        MessageLogger::Error(CALL_METHOD, message);
    }

    // Data recevied
    emit DataReceived(imported_date);

    // Status update
    const QString message =
        tr("Obtained update for %1 from WU server (%2 observations, %3 to %4)")
        .arg(imported_date,
             QString::number(num_observations),
             min_time,
             max_time);
    emit StatusUpdate(message);
    CALL_OUT("");
    return true;
}



///////////////////////////////////////////////////////////////////////////////
// Parse single observation
bool WundergroundComms::Parse_SingleObservation(
    const QJsonObject & mcrObservation)
{
    CALL_IN(QString("mcrObservation=%1")
        .arg(CALL_SHOW_FULL(mcrObservation)));

    // {
    //   "stationID":"ISOLIN267",
    //   "tz":"Europe/Berlin",
    //   "obsTimeUtc":"2025-05-01T16:39:49Z",
    //   "obsTimeLocal":"2025-05-01 18:39:49",
    //   "epoch":1746117589,
    //   "lat":51.129,
    //   "lon":7.153,
    //   "solarRadiationHigh":167.2,
    //   "uvHigh":1.5,
    //   "winddirAvg":112,
    //   "humidityHigh":26.0,
    //   "humidityLow":23.0,
    //   "humidityAvg":24.0,
    //   "qcStatus":-1,
    //   "metric":
    //   {
    //     "tempHigh":28.7,
    //     "tempLow":28.2,
    //     "tempAvg":28.4,
    //     "windspeedHigh":6.1,
    //     "windspeedLow":0.0,
    //     "windspeedAvg":2.5,
    //     "windgustHigh":6.1,
    //     "windgustLow":0.0,
    //     "windgustAvg":2.5,
    //     "dewptHigh":7.3,
    //     "dewptLow":5.3,
    //     "dewptAvg":6.0,
    //     "windchillHigh":28.7,
    //     "windchillLow":28.2,
    //     "windchillAvg":28.4,
    //     "heatindexHigh":27.3,
    //     "heatindexLow":27.0,
    //     "heatindexAvg":27.1,
    //     "pressureMax":1008.47,
    //     "pressureMin":1008.47,
    //     "pressureTrend":0.00,
    //     "precipRate":0.00,
    //     "precipTotal":0.81
    //   }
    // }

    // Loop contents
    QHash < QString, QString > observation;
    const QStringList all_keys = mcrObservation.keys();
    for (const QString & key : all_keys)
    {
        if (key == "metric")
        {
            const QJsonObject json_metric = mcrObservation[key].toObject();
            const QStringList all_metric_keys = json_metric.keys();
            for (const QString & metric_key : all_metric_keys)
            {
                if (m_WUToDB.contains(metric_key))
                {
                    if (m_WUToDB[metric_key].isEmpty())
                    {
                        // Value is ingored
                        continue;
                    }
                    const float value = json_metric[metric_key].toDouble();
                    observation[m_WUToDB[metric_key]] = QString::number(value);
                    continue;
                } else
                {
                    const QString message =
                        tr("Unknown key \"%1\" in observation/metric")
                            .arg(metric_key);
                    MessageLogger::Error(CALL_METHOD, message);
                }
            }
            continue;
        }

        if (m_WUToDB.contains(key))
        {
            if (m_WUToDB[key].isEmpty())
            {
                // Value is ingored
                continue;
            }
            if (key == "stationID" ||
                key == "tz" ||
                key == "obsTimeLocal")
            {
                observation[m_WUToDB[key]] = mcrObservation[key].toString();
            } else
            {
                const float value = mcrObservation[key].toDouble();
                observation[m_WUToDB[key]] = QString::number(value);
            }
            continue;
        }

        // Unknown key
        const QString message = tr("Unknown key \"%1\" in observation")
            .arg(key);
        MessageLogger::Error(CALL_METHOD, message);
    }

    // Check if we already saved this observation in a previous update
    if (!observation.contains("station_id") ||
        !observation.contains("date_time"))
    {
        const QString reason =
            tr("Observation is missing station ID and date/time");
        MessageLogger::Error(CALL_METHOD, reason);
        CALL_OUT(reason);
        return false;
    }
    const QString station_id = observation["station_id"];
    const QString date_time = observation["date_time"];
    if (m_StationToDateTimes.contains(station_id) &&
        m_StationToDateTimes[station_id].contains(date_time))
    {
        // We know this observation already.
        CALL_OUT("");
        return true;
    }

    // Keep observation
    m_WeatherData << observation;
    m_StationToDateTimes[station_id] += date_time;

    // Save to database
    const bool success = SaveToDatabase(observation);
    if (!success)
    {
        // Has been reported previously.
        CALL_OUT("");
        return false;
    }

    CALL_OUT("");
    return true;
}
