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

// MainWindow.cpp
// Class definition

// Project includes
#include "CallTracer.h"
#include "Config.h"
#include "MainWindow.h"
#include "WundergroundComms.h"

// Qt includes
#include <QDialog>
#include <QDir>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPainter>
#include <QRegularExpressionMatch>
#include <QScrollBar>
#include <QThread>
#include <QTimer>



// ================================================================== Lifecycle



///////////////////////////////////////////////////////////////////////////////
// Constructor
MainWindow::MainWindow()
{
    CALL_IN("");

    // Initialize Widgets
    InitWidgets();

    // Initialize WU
    WundergroundComms * wc = WundergroundComms::Instance();
    connect (wc, SIGNAL(DataReceived(const QString)),
        this, SLOT(DataReceived(const QString)));
    connect (wc, SIGNAL(StatusUpdate(const QString)),
        this, SLOT(UpdateStatus(const QString)));

    // Open database
    wc -> SetPWSName(WU_PWS_NAME);
    wc -> SetToken(WU_TOKEN);

    wc -> SetDatabaseFile(WU_DATABASE_FILE);
    wc -> OpenDatabase();

    // Start bot
    wc -> StartUpdates();

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Destructor
MainWindow::~MainWindow()
{
    CALL_IN("");

    // Nothing to do

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Instanciator
MainWindow * MainWindow::Instance()
{
    CALL_IN("");

    if (!m_Instance)
    {
        m_Instance = new MainWindow();
    }

    CALL_OUT("");
    return m_Instance;
}
    


///////////////////////////////////////////////////////////////////////////////
// Instance
MainWindow * MainWindow::m_Instance = nullptr;



// ======================================================================== GUI



///////////////////////////////////////////////////////////////////////////////
// All GUI stuff
void MainWindow::InitWidgets()
{
    CALL_IN("");

    setWindowTitle(QString("Weather Underground - %1").arg(WU_PWS_NAME));

    QWidget * central_widget = new QWidget();
    setCentralWidget(central_widget);
    QVBoxLayout * layout = new QVBoxLayout();
    central_widget -> setLayout(layout);
    
    // Tab Widget
    m_TabWidget = new QTabWidget();
    m_TabWidget -> setMinimumSize(800, 300);
    layout -> addWidget(m_TabWidget);

    // Log
    m_LogWidget = new QTextEdit();
    m_LogWidget -> setReadOnly(true);
    m_TabWidget -> addTab(m_LogWidget, tr("Log"));

    // Bottom row
    QHBoxLayout * bottom_layout = new QHBoxLayout();
    layout -> addLayout(bottom_layout);

    m_Status = new QLabel();
    bottom_layout -> addWidget(m_Status);

    QPushButton * pb_getdate = new QPushButton(tr("Get specific date"));
    pb_getdate -> setFixedWidth(150);
    connect (pb_getdate, SIGNAL(clicked()),
        this, SLOT(GetSpecificDate()));
    bottom_layout -> addWidget(pb_getdate);

    layout -> setStretch(0, 1);
    layout -> setStretch(1, 0);

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Update bot status
void MainWindow::UpdateStatus(const QString & mcrStatus)
{
    CALL_IN(QString("mcrStatus=%1")
        .arg(CALL_SHOW(mcrStatus)));

    m_Status -> setText(mcrStatus);

    const QString now =
        QDateTime::currentDateTime().toString("dd MMM yyyy hh:mm:ss");
    AddLogLine(now, mcrStatus);

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Get particular date
void MainWindow::GetSpecificDate()
{
    CALL_IN("");

    QDialog * dialog = new QDialog();
    QHBoxLayout * layout = new QHBoxLayout();
    dialog -> setLayout(layout);

    QLabel * l_date = new QLabel(tr("Date (YYYYMMDD): "));
    layout -> addWidget(l_date);

    QLineEdit * date = new QLineEdit();
    connect (date, SIGNAL(editingFinished()),
        dialog, SLOT(accept()));
    layout -> addWidget(date);

    const int result = dialog -> exec();
    if (result == QDialog::Accepted)
    {
        WundergroundComms * wc = WundergroundComms::Instance();
        wc -> GetDate(date -> text());
    }

    delete dialog;

    CALL_OUT("");
}



///////////////////////////////////////////////////////////////////////////////
// Add line to the log
void MainWindow::AddLogLine(const QString & mcrDateTime,
    const QString & mcrMessage)
{
    CALL_IN(QString("mcrDateTime=%1, mcrMessage=%2")
        .arg(CALL_SHOW(mcrDateTime),
             CALL_SHOW(mcrMessage)));

    m_LogMessages << QString("<tr>"
        "<td align=\"top\">[%1]</td>"
        "<td>%2</td>"
        "</tr>\n")
        .arg(mcrDateTime,
             mcrMessage);

    // Update log
    QScrollBar * vertical = m_LogWidget -> verticalScrollBar();
    int vertical_position = vertical -> value();
    const bool at_bottom = (vertical_position == vertical -> maximum());
    m_LogWidget -> setHtml(
        "<table width=\"100%\">"
        + m_LogMessages.join("")
        + "</table>");
    if (at_bottom)
    {
        vertical_position = vertical -> maximum();
    }
    vertical -> setValue(vertical_position);

    CALL_OUT("");
}



// =============================================================== Wunderground



///////////////////////////////////////////////////////////////////////////////
// Data received
void MainWindow::DataReceived(const QString & mcrDate)
{
    CALL_IN(QString("mcrDate=%1")
        .arg(CALL_SHOW(mcrDate)));

    // Nothing to do right now

    CALL_OUT("");
}
