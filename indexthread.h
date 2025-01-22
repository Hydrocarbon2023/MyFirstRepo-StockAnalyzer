// By coconut, 2024.6

#ifndef INDEXTHREAD_H
#define INDEXTHREAD_H

#include "stockdata.h"
#include <QThread>

class IndexThread : public QThread {
    Q_OBJECT
public:
    IndexThread(QObject *parent=nullptr);
    void run() override;
    void setInPath(const std::string &path);
    void setIndexPath(const std::string &path);
signals:
    void updateCurrentState(const QString &message);
    void complete();
private:
    std::string inPath;
    std::string indexPath;
    StockData lineToData(const std::string &line);
    void generateIndex(const std::string &inPath, const std::string &indexPath);
};

#endif // INDEXTHREAD_H
