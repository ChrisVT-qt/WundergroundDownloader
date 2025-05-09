// MainWindow.h
// Class definition

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// Qt includes
#include <QLabel>
#include <QMainWindow>
#include <QTextEdit>
#include <QPushButton>



// Class definition
class MainWindow
    : public QMainWindow
{
    Q_OBJECT
    
    
    
    // ============================================================== Lifecycle
private:
    // Constructor
    MainWindow();
    
public:
    // Destructor
    virtual ~MainWindow();
    
    // Instanciator
    static MainWindow * Instance();

private:
    // Instance
    static MainWindow * m_Instance;
    
    
    
    // ==================================================================== GUI
private:
    // Initialize Widgets
    void InitWidgets();

    QTabWidget * m_TabWidget;
    QTextEdit * m_LogWidget;
    QLabel * m_Status;

private slots:
    // Update status message
    void UpdateStatus(const QString & mcrUpdate);

    // Get particular date
    void GetSpecificDate();

private:
    // Add line to the log
    void AddLogLine(const QString & mcrDateTime, const QString & mcrMessage);
    QStringList m_LogMessages;



    // =========================================================== Wunderground
private slots:
    // Data received
    void DataReceived(const QString & mcrDate);
};

#endif

