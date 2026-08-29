#pragma once

#include <QtCore>

class ZDLNullDevice : public QIODevice {
public:
    qint64 readData([[maybe_unused]] char *data, [[maybe_unused]] qint64 len) override {
        return 0;
    }

    qint64 writeData([[maybe_unused]] const char *data, [[maybe_unused]] qint64 len) override {
        return 0;
    }
};
