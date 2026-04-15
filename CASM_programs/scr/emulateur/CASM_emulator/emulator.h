#ifndef EMULATOR_H
#define EMULATOR_H
#pragma once
#include <QObject>
#include <vector>

class Emulator : public QObject {
    Q_OBJECT

public:
    explicit Emulator(QObject* parent = nullptr);

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

signals:
    void frameReady(QByteArray frame); //pour affichage ecran

private:
    bool running = false;
};

#endif //EMULATOR_H
//magnus carlsen 2024-06