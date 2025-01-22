#include "sortthread.h"
// The project is so naive. 🤣 2025.1.22

SortThread::SortThread(QObject *parent) :
    QThread(parent), exSort(new ExternalSort(this)) {
}

void SortThread::setInputPath(const std::string &path) {
    inputPath = path;
}

void SortThread::setOutputPath(const std::string &path) {
    outputPath = path;
}

void SortThread::run() {
    exSort->doSort(inputPath, outputPath);
    emit complete();
}
