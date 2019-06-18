/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  Copyright (C) 2015 Simon Stürz <simon.stuerz@guh.io>                   *
 *  Copyright (C) 2014 Michael Zanetti <michael_zanetti@gmx.net>           *
 *                                                                         *
 *  This file is part of nymea.                                            *
 *                                                                         *
 *  This library is free software; you can redistribute it and/or          *
 *  modify it under the terms of the GNU Lesser General Public             *
 *  License as published by the Free Software Foundation; either           *
 *  version 2.1 of the License, or (at your option) any later version.     *
 *                                                                         *
 *  This library is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *  Lesser General Public License for more details.                        *
 *                                                                         *
 *  You should have received a copy of the GNU Lesser General Public       *
 *  License along with this library; If not, see                           *
 *  <http://www.gnu.org/licenses/>.                                        *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*!
    \page commandlauncher.html
    \title Application and script launcher
    \brief Plugin for system commands.

    \ingroup plugins
    \ingroup nymea-plugins-maker

    The application and script launcher plugin allows you to execute bash commands and start bash scripts.

    \chapter Application launcher

    The application launcher \l{DeviceClass} allows you to call bash applications or commands (with parameters)
    from nymea. Once, the application started, the \tt running \l{State} will change to \tt true, if the application
    is finished, the \tt running \l{State} will change to \tt false.

    \section3 Example
    An example command could be \l{http://linux.die.net/man/1/espeak}{espeak}. (\tt{apt-get install espeak})
    \code
    espeak -v en "Chuck Norris is using nymea"
    \endcode


    \chapter Bashscript launcher

    The bashscript launcher \l{DeviceClass} allows you to call bash script (with parameters)
    from nymea. Once, the script is running, the \tt running \l{State} will change to \tt true, if the script
    is finished, the \tt running \l{State} will change to \tt false.

    \section3 Example
    An example for a very useful script could be a backup scrip like following \tt backup.sh script.
    \code
    #!/bin/sh
    # Directories to backup...
    backup_files="/home /etc /root /opt /var/www /var/lib/jenkins"

    # Destination of the backup...
    dest="/mnt/backup"

    # Create archive filename...
    day=$(date +%Y%m%d)
    hostname="nymea.io"
    archive_file="$day-$hostname.tgz"

    # Print start status message...
    echo "Backing up $backup_files to $dest/$archive_file"
    date
    echo

    # Backup the files using tar.
    tar czf $dest/$archive_file $backup_files

    echo
    echo "Backup finished"
    date
    echo "==========================="
    echo "  DONE, have a nice day!   "
    echo "==========================="
    \endcode

    To make the script executable use following command:

    \code
    chmod +x backup.sh
    \endcode

    \chapter Plugin properties
    Following JSON file contains the definition and the description of all available \l{DeviceClass}{DeviceClasses}
    and \l{Vendor}{Vendors} of this \l{DevicePlugin}.

    For more details how to read this JSON file please check out the documentation for \l{The plugin JSON File}.

    \quotefile plugins/deviceplugins/commandlauncher/deviceplugincommandlauncher.json
*/

#include "deviceplugincommandlauncher.h"

#include "devices/device.h"
#include "plugininfo.h"

#include <QDebug>

DevicePluginCommandLauncher::DevicePluginCommandLauncher()
{

}

Device::DeviceSetupStatus DevicePluginCommandLauncher::setupDevice(Device *device)
{
    // Application
    if(device->deviceClassId() == applicationDeviceClassId)
        return Device::DeviceSetupStatusSuccess;

    // Script
    if(device->deviceClassId() == scriptDeviceClassId){
        QStringList scriptArguments = device->paramValue(scriptDeviceScriptParamTypeId).toString().split(QRegExp("[ \r\n][ \r\n]*"));
        // check if script exists and if it is executable
        QFileInfo fileInfo(scriptArguments.first());
        if (!fileInfo.exists()) {
            qCWarning(dcCommandLauncher) << "script " << scriptArguments.first() << "does not exist.";
            return Device::DeviceSetupStatusFailure;
        }
        if (!fileInfo.isExecutable()) {
            qCWarning(dcCommandLauncher) << "script " << scriptArguments.first() << "is not executable. Please check the permissions.";
            return Device::DeviceSetupStatusFailure;
        }
        if (!fileInfo.isReadable()) {
            qCWarning(dcCommandLauncher) << "script " << scriptArguments.first() << "is not readable. Please check the permissions.";
            return Device::DeviceSetupStatusFailure;
        }

        return Device::DeviceSetupStatusSuccess;
    }
    return Device::DeviceSetupStatusFailure;
}

Device::DeviceError DevicePluginCommandLauncher::executeAction(Device *device, const Action &action)
{
    // Application
    if (device->deviceClassId() == applicationDeviceClassId ) {
        // execute application...
        if (action.actionTypeId() == applicationTriggerActionTypeId) {
            // check if we already have started the application
            if (m_applications.values().contains(device)) {
                if (m_applications.key(device)->state() == QProcess::Running) {
                    return Device::DeviceErrorDeviceInUse;
                }
            }
            QProcess *process = new QProcess(this);
            connect(process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(applicationFinished(int,QProcess::ExitStatus)));
            connect(process, &QProcess::stateChanged, this, &DevicePluginCommandLauncher::applicationStateChanged);

            m_applications.insert(process, device);
            m_startingApplications.insert(process, action.id());
            process->start("/bin/bash", QStringList() << "-c" << device->paramValue(applicationDeviceCommandParamTypeId).toString());

            return Device::DeviceErrorAsync;
        }
        // kill application...
        if (action.actionTypeId() == applicationKillActionTypeId) {
            // check if the application is running...
            if (!m_applications.values().contains(device)) {
                return Device::DeviceErrorNoError;
            }
            QProcess *process = m_applications.key(device);
            m_killingApplications.insert(process,action.id());
            process->kill();

            return Device::DeviceErrorAsync;
        }
        return Device::DeviceErrorActionTypeNotFound;
    }

    // Script
    if (device->deviceClassId() == scriptDeviceClassId ) {
        // execute script...
        if (action.actionTypeId() == scriptTriggerActionTypeId) {
            // check if we already have started the script
            if (m_scripts.values().contains(device)) {
                if (m_scripts.key(device)->state() == QProcess::Running) {
                    return Device::DeviceErrorDeviceInUse;
                }
            }
            QProcess *process = new QProcess(this);
            connect(process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(scriptFinished(int,QProcess::ExitStatus)));
            connect(process, &QProcess::stateChanged, this, &DevicePluginCommandLauncher::scriptStateChanged);

            m_scripts.insert(process, device);
            m_startingScripts.insert(process, action.id());
            process->start("/bin/bash", QStringList() << device->paramValue(scriptDeviceScriptParamTypeId).toString());

            return Device::DeviceErrorAsync;
        }
        // kill script...
        if (action.actionTypeId() == scriptKillActionTypeId) {
            // check if the script is running...
            if (!m_scripts.values().contains(device)) {
                return Device::DeviceErrorNoError;
            }
            QProcess *process = m_scripts.key(device);
            m_killingScripts.insert(process,action.id());
            process->kill();

            return Device::DeviceErrorAsync;
        }
        return Device::DeviceErrorActionTypeNotFound;
    }
    return Device::DeviceErrorDeviceClassNotFound;
}

void DevicePluginCommandLauncher::deviceRemoved(Device *device)
{
    if (m_applications.values().contains(device)) {
        QProcess * process = m_applications.key(device);
        if (process->state() != QProcess::NotRunning) {
            process->kill();
        }
        m_applications.remove(process);

        if (m_startingApplications.contains(process)) {
            m_startingApplications.remove(process);
        }
        if (m_killingApplications.contains(process)) {
            m_killingApplications.remove(process);
        }
        process->deleteLater();
    }
    if (m_scripts.values().contains(device)) {
        QProcess * process = m_scripts.key(device);
        if (process->state() != QProcess::NotRunning) {
            process->kill();
        }
        m_scripts.remove(process);

        if (m_startingScripts.contains(process)) {
            m_startingScripts.remove(process);
        }
        if (m_killingScripts.contains(process)) {
            m_killingScripts.remove(process);
        }
        process->deleteLater();
    }
}

void DevicePluginCommandLauncher::scriptStateChanged(QProcess::ProcessState state)
{
    QProcess *process = static_cast<QProcess*>(sender());
    Device *device = m_scripts.value(process);

    switch (state) {
    case QProcess::Running:
        device->setStateValue(scriptRunningStateTypeId, true);
        emit actionExecutionFinished(m_startingScripts.value(process), Device::DeviceErrorNoError);
        m_startingScripts.remove(process);
        break;
    case QProcess::NotRunning:
        device->setStateValue(scriptRunningStateTypeId, false);
        if (m_killingScripts.contains(process)) {
            emit actionExecutionFinished(m_killingScripts.value(process), Device::DeviceErrorNoError);
            m_killingScripts.remove(process);
        }
        break;
    default:
        break;
    }
}

void DevicePluginCommandLauncher::scriptFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);

    QProcess *process = static_cast<QProcess*>(sender());
    Device *device = m_scripts.value(process);

    device->setStateValue(scriptRunningStateTypeId, false);

    m_scripts.remove(process);
    process->deleteLater();
}

void DevicePluginCommandLauncher::applicationStateChanged(QProcess::ProcessState state)
{
    QProcess *process = static_cast<QProcess*>(sender());
    Device *device = m_applications.value(process);

    switch (state) {
    case QProcess::Running:
        device->setStateValue(applicationRunningStateTypeId, true);
        emit actionExecutionFinished(m_startingApplications.value(process), Device::DeviceErrorNoError);
        m_startingApplications.remove(process);
        break;
    case QProcess::NotRunning:
        device->setStateValue(applicationRunningStateTypeId, false);
        if (m_killingApplications.contains(process)) {
            emit actionExecutionFinished(m_killingApplications.value(process), Device::DeviceErrorNoError);
            m_killingApplications.remove(process);
        }
        break;
    default:
        break;
    }
}

void DevicePluginCommandLauncher::applicationFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);

    QProcess *process = static_cast<QProcess*>(sender());
    Device *device = m_applications.value(process);

    device->setStateValue(applicationRunningStateTypeId, false);

    m_applications.remove(process);
    process->deleteLater();
}
