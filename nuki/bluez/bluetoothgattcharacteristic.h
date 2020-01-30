/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  Copyright (C) 2016-2018 Simon Stürz <simon.stuerz@guh.io>              *
 *                                                                         *
 *  This file is part of guh.                                              *
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

#ifndef BLUETOOTHGATTCHARACTERISTIC_H
#define BLUETOOTHGATTCHARACTERISTIC_H

#include <QHash>
#include <QFlag>
#include <QObject>
#include <QBluetoothUuid>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>

#include "blueztypes.h"
#include "bluetoothgattdescriptor.h"

// Note: DBus documentation https://git.kernel.org/pub/scm/bluetooth/bluez.git/tree/doc/gatt-api.txt

class BluetoothManager;
class BluetoothGattService;

class BluetoothGattCharacteristic : public QObject
{
    Q_OBJECT
    Q_FLAGS(Properties)

    friend class BluetoothManager;
    friend class BluetoothGattService;

public:
    enum Property {
        Unknown = 0x00,
        Broadcasting = 0x01,
        Read = 0x02,
        WriteNoResponse = 0x04,
        Write = 0x08,
        Notify = 0x10,
        Indicate = 0x20,
        WriteAuthenticatedSigned = 0x40,
        ReliableWrite = 0x80,
        WritableAuxiliaries = 0x100,
        EncryptRead = 0x200,
        EncryptWrite = 0x400,
        EncryptAuthenticatedRead = 0x800,
        EncryptAuthenticatedWrite = 0x1000,
        SecureRead = 0x2000, // Server only
        SecureWrite = 0x4000, // Server only
    };
    Q_DECLARE_FLAGS(Properties, Property)

    QString chararcteristicName() const;
    QBluetoothUuid uuid() const;
    bool notifying() const;
    Properties properties() const;
    QByteArray value() const;
    QList<BluetoothGattDescriptor *> descriptors() const;


private:
    explicit BluetoothGattCharacteristic(const QDBusObjectPath &path, const QVariantMap &properties, QObject *parent = 0);

    QDBusObjectPath m_path;
    QDBusInterface *m_characteristicInterface;

    QString m_characteristicName;
    QBluetoothUuid m_uuid;
    bool m_notifying;
    Properties m_properties;
    QByteArray m_value;
    QList<BluetoothGattDescriptor *> m_descriptors;

    QHash<QDBusPendingCallWatcher *, QByteArray> m_asyncWrites;

    void processProperties(const QVariantMap &properties);

    // Methods called from BluetoothManager
    void addDescriptorInternally(const QDBusObjectPath &path, const QVariantMap &properties);
    bool hasDescriptor(const QDBusObjectPath &path);
    BluetoothGattDescriptor *getDescriptor(const QDBusObjectPath &path);

    void setValueInternally(const QByteArray &value);
    void setNotifyingInternally(const bool &notifying);

    Properties parsePropertyFlags(const QStringList &characteristicProperties);

signals:
    void notifyingChanged(const bool &notifying);
    void valueChanged(const QByteArray &value);
    void readingFinished(const QByteArray &value);
    void writingFinished(const QByteArray &value);

private slots:
    void onPropertiesChanged(const QString &interface, const QVariantMap &changedProperties, const QStringList &invalidatedProperties);

    void onReadingFinished(QDBusPendingCallWatcher *call);
    void onWritingFinished(QDBusPendingCallWatcher *call);
    void onStartNotificationFinished(QDBusPendingCallWatcher *call);
    void onStopNotificationFinished(QDBusPendingCallWatcher *call);

public slots:
    bool readCharacteristic();
    bool writeCharacteristic(const QByteArray &value);
    bool startNotifications();
    bool stopNotifications();

};

Q_DECLARE_OPERATORS_FOR_FLAGS(BluetoothGattCharacteristic::Properties)

QDebug operator<<(QDebug debug, BluetoothGattCharacteristic *characteristic);

#endif // BLUETOOTHGATTCHARACTERISTIC_H