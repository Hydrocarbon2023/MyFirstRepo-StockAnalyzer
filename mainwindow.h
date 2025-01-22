// By coconut, 2024.6

#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "sortthread.h"
#include "indexthread.h"
#include <qcustomplot.h>
#include <QMainWindow>
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent=nullptr);
    ~MainWindow();
private slots:
    void on_sortButtonClicked();
    void on_sortingComplete();
    void on_browseButtonClicked();
    void on_sortingStateUpdated(const QString &message);
    void on_indexComplete();
    void on_candleButtonClicked();
    void on_heatButtonClicked();
    void on_jumpButton_clicked();
    void on_codeLine_returnPressed();
    void on_predictButtonClicked();
private:
    bool isGoingToSort;
    Ui::MainWindow *ui;
    SortThread *sortThread;
    std::string inputPath;
    std::string outputPath;
    IndexThread *indexThread;
    std::string indexPath;
    void loadIndex(const std::string &path);
    std::unordered_map<std::string, long long> dict;
    StockData lineToData(const std::string &line);
    int k;
    QVector<QString> compared;
    double calculatePearson(const QVector<double> &a, const QVector<double> &b);
    void loadMonthData(const std::string &targetStock, const std::string &targetMonth, QVector<double> &arr);
    std::string toNextMonth(const std::string &current);
};

#endif // MAINWINDOW_H
