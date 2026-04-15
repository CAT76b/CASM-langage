#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include "emulator.h"
#include <thread>

Emulator::Emulator(QObject* parent) : QObject(parent) {}

void Emulator::start() {
    running = true;

    std::thread([this]() {
        while (running) {
            QByteArray fakeFrame(640*480, 0);
            emit frameReady(fakeFrame);
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }).detach();
}

void Emulator::stop() { running = false; }

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("CASM_emulator", "Main");

    return QCoreApplication::exec();
}

//magnus carlsen 2024-06