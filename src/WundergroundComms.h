// WundergroundComms.h
// Class definition

#ifndef WUNDERGROUNDCOMMS_H
#define WUNDERGROUNDCOMMS_H

// Qt includes
#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>



// Class definition
class WundergroundComms
    : public QObject
{
    Q_OBJECT



    // ============================================================== Lifecycle
private:
    // Constructor
    WundergroundComms();

public:
    // Destructor
    virtual ~WundergroundComms();

    // Instanciator
    static WundergroundComms * Instance();

private:
    // Instance
    static WundergroundComms * m_Instance;



    // =============================================================== Database
public:
    // Set database name
    void SetDatabaseFile(const QString & mcrFilename);
private:
    QString m_DatabaseFilename;

public:
    bool OpenDatabase();
private:
    bool m_DatabaseConnected;

    // Create database
    bool CreateDatabase();

    // Read database
    bool ReadDatabase();

    // Save observation to database
    bool SaveToDatabase(const QHash < QString, QString > & mcrObservation);

public:
    // Update database
    void UpdateDatabase();

private:
    // Mapping of WU columns to database columns
    void Initialize_WUToDB();
    QHash < QString, QString > m_WUToDB;



    // ================================================================== Setup
public:
    // Set PWS name
    bool SetPWSName(const QString & mcrPWSName);
private:
    QString m_PWSName;

public:
    // Wunderground token
    bool SetToken(const QString & mcrToken);
private:
    QString m_Token;

public:
    // Start/stop updates
    void StartUpdates();
    void StopUpdates();
private:
    bool m_IsRunning;

public:
    QString GetStartDateTime() const;
    QString GetUptime() const;
private:
    QDateTime m_StartDateTime;



    // =============================================== Reading sfrom the Server
private slots:
    // Periodically getting updates
    void Periodic_GetUpdates();
public:
    void GetDate(const QString & mcrDate);

private slots:
    // Handle response
    bool HandleResponse(QNetworkReply * mpResponse);
private:
    QNetworkAccessManager * m_NetworkAccessManager;

private:
    bool Parse_Observations(const QJsonObject & mcrObservations);
    bool Parse_SingleObservation(const QJsonObject & mcrObservation);
    QHash < QString, QSet < QString > > m_StationToDateTimes;
    QList < QHash < QString, QString > > m_WeatherData;

signals:
    void DataReceived(const QString & mcrDate);
    void StatusUpdate(const QString & mcrUpdate);
};

#endif
