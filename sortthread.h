#ifndef SORTTHREAD_H
#define SORTTHREAD_H
#include "externalsort.h"
#include <QThread>

class SortThread : public QThread {
    Q_OBJECT;
public:
    SortThread(QObject *parent=nullptr);
    void run() override;
    void setInputPath(const std::string &path);
    void setOutputPath(const std::string &path);
    ExternalSort *exSort;
signals:
    void complete();
private:
    std::string inputPath;
    std::string outputPath;
};

#endif // SORTTHREAD_H
