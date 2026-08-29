#pragma once

#include <qiodevice.h>

class ZDLNullDevice : public QIODevice {
Q_OBJECT

public:
    explicit ZDLNullDevice(QObject *parent = nullptr) : QIODevice(parent) {
    }

protected:
    qint64 readData([[maybe_unused]] char *data, [[maybe_unused]] qint64 len) override {
        return 0;
    }

    qint64 writeData([[maybe_unused]] const char *data, [[maybe_unused]] qint64 len) override {
        return 0;
    }
};
