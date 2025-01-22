// By coconut, 2024.6

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "sortthread.h"
#include "indexthread.h"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <QFileDialog>
#include <QMessageBox>


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);
    sortThread = new SortThread(this);
    indexThread = new IndexThread(this);
    connect(ui->sortButton, &QPushButton::clicked, this, &MainWindow::on_sortButtonClicked);
    connect(sortThread, &SortThread::complete, this, &MainWindow::on_sortingComplete);
    connect(ui->browseButton, &QPushButton::clicked, this, &MainWindow::on_browseButtonClicked);
    connect(sortThread->exSort, &ExternalSort::updateSortingState, this, &MainWindow::on_sortingStateUpdated);
    connect(indexThread, &IndexThread::complete, this, &MainWindow::on_indexComplete);
    connect(indexThread, &IndexThread::updateCurrentState, this, &MainWindow::on_sortingStateUpdated);
    connect(ui->candleButton, &QPushButton::clicked, this, &MainWindow::on_candleButtonClicked);
    connect(ui->heatButton, &QPushButton::clicked, this, &MainWindow::on_heatButtonClicked);
    connect(ui->predictButton, &QPushButton::clicked, this, &MainWindow::on_predictButtonClicked);
    isGoingToSort = true;
    k = 10;
    ui->candleButton->setEnabled(false);
    ui->candleButton->setStyleSheet("QPushButton { color : grey; }");
    ui->heatButton->setEnabled(false);
    ui->heatButton->setStyleSheet("QPushButton { color : grey; }");
    ui->predictButton->setEnabled(false);
    ui->predictButton->setStyleSheet("QPushButton { color : grey; }");
}


MainWindow::~MainWindow() {
    delete ui;
    if (sortThread->isRunning()) {
        sortThread->quit();
        sortThread->wait();
    }
    delete sortThread;
    if (indexThread->isRunning()) {
        indexThread->quit();
        indexThread->wait();
    }
    delete indexThread;
}


void MainWindow::on_sortButtonClicked() {
    if (isGoingToSort) {
        QString defaultLine = "请选择文件(*.csv):";
        QString lineText = ui->inputPathLine->text();
        if (lineText != defaultLine) {
            inputPath = lineText.toStdString();
            std::filesystem::path iPath(inputPath);
            outputPath = iPath.parent_path().string() + "/output.txt";
        }
        if (inputPath.empty() || outputPath.empty()) {
            QMessageBox::warning(this, "注意", "请先选择路径");
            return;
        }
        sortThread->setInputPath(inputPath);
        sortThread->setOutputPath(outputPath);
        sortThread->start();
    }
    else {
        std::filesystem::path iPath(outputPath);
        indexPath = iPath.parent_path().string() + "/index.txt";
        indexThread->setInPath(outputPath);
        indexThread->setIndexPath(indexPath);
        indexThread->start();
    }
}


void MainWindow::on_sortingComplete() {
    QMessageBox::information(this, "报告", "排序已完成。");
    ui->sortButton->setText("创建索引");
    isGoingToSort = false;
}


void MainWindow::on_browseButtonClicked() {
    QString inputPath = QFileDialog::getOpenFileName(this, tr("选择文件"), QDir::homePath(), tr("CSV files (*.csv)"));
    if (!inputPath.isEmpty())
        ui->inputPathLine->setText(inputPath);
}


void MainWindow::on_sortingStateUpdated(const QString &message) {
    ui->inputPathLine->setText(message);
}


void MainWindow::on_indexComplete() {
    QMessageBox::information(this, "报告", "索引已生成。");
    loadIndex(indexPath);
    ui->sortButton->setText("一键排序");
    isGoingToSort = true;
    ui->candleButton->setEnabled(true);
    ui->candleButton->setStyleSheet("");
    ui->heatButton->setEnabled(true);
    ui->heatButton->setStyleSheet("");
    ui->predictButton->setEnabled(true);
    ui->predictButton->setStyleSheet("");
}


void MainWindow::on_candleButtonClicked() {
    std::string code = ui->codeLine->text().toStdString();
    std::string month = ui->timeLine->text().toStdString();
    if (code.empty() || month.empty())
        throw std::runtime_error("请输入有效数据。");
    // 以下为保护区，禁止修改
    ui->custom_plot->clearItems();
    ui->custom_plot->clearGraphs();
    ui->custom_plot->clearPlottables();
    ui->custom_plot->plotLayout()->clear();
    ui->custom_plot->replot();
    QCPAxisRect *cs_axis_rect = new QCPAxisRect(ui->custom_plot), *vol_axis_rect = new QCPAxisRect(ui->custom_plot);
    ui->custom_plot->plotLayout()->addElement(0, 0, cs_axis_rect);
    ui->custom_plot->plotLayout()->addElement(1, 0, vol_axis_rect);
    vol_axis_rect->setMaximumSize(QSize(QWIDGETSIZE_MAX, 100));
    for (QCPAxis *axis : cs_axis_rect->axes()) {
        axis->setLayer("axes");
        axis->grid()->setLayer("grid");
    }
    for (QCPAxis *axis : vol_axis_rect->axes()) {
        axis->setLayer("axes");
        axis->grid()->setLayer("grid");
    }
    QCPFinancial *candle_stick = new QCPFinancial(cs_axis_rect->axis(QCPAxis::atBottom), cs_axis_rect->axis(QCPAxis::atLeft));
    QCPBars *volume_pos = new QCPBars(vol_axis_rect->axis(QCPAxis::atBottom), vol_axis_rect->axis(QCPAxis::atLeft));
    QCPBars *volume_neg = new QCPBars(vol_axis_rect->axis(QCPAxis::atBottom), vol_axis_rect->axis(QCPAxis::atLeft));
    QCPDataContainer<QCPFinancialData> QCP_financial_data;
    QSharedPointer<QCPAxisTickerText> text_ticker(new QCPAxisTickerText);
    // 保护区结束
    long long offset = dict[code + ".SZ_" + month];
    if (offset == std::streampos(-1)) {
        QMessageBox::warning(this, "注意", "未找到当月数据TT");
        candle_stick->~QCPFinancial();
        volume_pos->~QCPBars();
        volume_neg->~QCPBars();
        return;
    }
    std::ifstream inputFile;
    inputFile.open(outputPath, std::ios::binary);
    if (!inputFile.is_open())
        throw std::runtime_error("源文件打开失败TT");
    inputFile.seekg(offset);
    std::string line;
    StockData data;
    if (offset == 0)
        std::getline(inputFile, line);
    while (std::getline(inputFile, line)) {
        QCPFinancialData fData;
        QCPBarsData bData;
        data = lineToData(line);
        fData.key = std::stoi(data.trade_date.substr(6, 2));
        fData.open = data.open;
        fData.high = data.high;
        fData.low = data.low;
        fData.close = data.close;
        bData.key = fData.key;
        bData.value = data.volume;
        if (data.trade_date.substr(0, 6) != month)
            break;
        double change = data.change;

        QCP_financial_data.add(fData);
        (change < 0 ? volume_neg : volume_pos)->addData(fData.key, bData.value);
        text_ticker->addTick(fData.key, QString::fromStdString(std::to_string(fData.key)));
    }
    inputFile.close();
    // 以下为保护区，禁止修改
    candle_stick->setName("日K");
    candle_stick->setChartStyle(QCPFinancial::csCandlestick);
    candle_stick->setBrushPositive(QColor("#EC0000"));
    candle_stick->setBrushNegative(QColor("#00DA3C"));
    candle_stick->setPenPositive(QColor("#8A0000"));
    candle_stick->setPenNegative(QColor("#008F28"));
    candle_stick->data()->set(QCP_financial_data);
    volume_pos->setPen(Qt::NoPen);
    volume_pos->setBrush(QColor("#EC0000"));
    volume_neg->setPen(Qt::NoPen);
    volume_neg->setBrush(QColor("#00DA3C"));
    connect(cs_axis_rect->axis(QCPAxis::atBottom), SIGNAL(rangeChanged(QCPRange)), vol_axis_rect->axis(QCPAxis::atBottom), SLOT(setRange(QCPRange)));
    connect(vol_axis_rect->axis(QCPAxis::atBottom), SIGNAL(rangeChanged(QCPRange)), cs_axis_rect->axis(QCPAxis::atBottom), SLOT(setRange(QCPRange)));
    vol_axis_rect->axis(QCPAxis::atBottom)->setTicker(text_ticker);
    vol_axis_rect->axis(QCPAxis::atBottom)->setTickLabelRotation(15);
    cs_axis_rect->axis(QCPAxis::atBottom)->setBasePen(Qt::NoPen);
    cs_axis_rect->axis(QCPAxis::atBottom)->setTickLabels(false);
    cs_axis_rect->axis(QCPAxis::atBottom)->setTicks(false);
    ui->custom_plot->rescaleAxes();
    cs_axis_rect->axis(QCPAxis::atBottom)->setRange(0, 32);
    QCPMarginGroup *group = new QCPMarginGroup(ui->custom_plot);
    ui->custom_plot->axisRect()->setMarginGroup(QCP::msLeft | QCP::msRight, group);
    cs_axis_rect->setMarginGroup(QCP::msLeft | QCP::msRight, group);
    vol_axis_rect->setMarginGroup(QCP::msLeft | QCP::msRight, group);
    ui->custom_plot->replot();
    // 保护区结束
    QMessageBox::information(this, "报告", "图像绘制完成。");
}


void MainWindow::on_heatButtonClicked() {
    std::string month_h = ui->timeLine->text().toStdString();
    if (compared.size() != k) {
        QMessageBox::warning(this, "注意", QString::fromStdString("请输入" + std::to_string(k) + "只股票的代码"));
        return;
    }
    QVector<QVector<double>> arrays(k, QVector<double>(31, 0.0));
    for (int i = 0; i < k; i++) {
        arrays[i].clear();
        std::ifstream ifs;
        ifs.open(outputPath, std::ios::binary);
        if (!ifs.is_open())
            throw std::runtime_error("源文件打开失败TT");
        long long loc = dict[(compared[i] + "_").toStdString() + month_h];
        ifs.seekg(loc);
        std::string line;
        StockData data;
        if (loc == 0)
            std::getline(ifs, line);
        bool first = true;
        while (std::getline(ifs, line)) {
            data = lineToData(line);
            int day = std::stoi(data.trade_date.substr(6, 2));
            if (first && day > 1) {
                for (int j = 1; j < day; j++) {
                    arrays[i].push_back(data.close);
                }
            }
            if (day > arrays[i].size() + 1) {
                double gradient = ((data.close - arrays[i].last()) / (day - arrays[i].size()));
                for (int j = 1; j < day - arrays[i].last(); j++) {
                    arrays[i].push_back(arrays[i].last() + gradient);
                }
            }
            arrays[i].push_back(data.close);
            first = false;
            if (data.trade_date.substr(0, 6) != month_h)
                break;
        }
        while (arrays[i].size() < 31) {
            arrays[i].push_back(arrays[i].last());
        }
        while (arrays[i].size() > 31) {
            arrays[i].pop_back();
        }
    }
    QVector<QVector<double>> correlation(k, QVector<double>(k, 0.0));
    for (int i = 0; i < k; i++) {
        for (int j = 0; j < k; j++) {
            correlation[i][j] = calculatePearson(arrays[i], arrays[j]);
        }
    }
    // 以下为保护区，禁止修改
    ui->custom_plot->clearItems();
    ui->custom_plot->clearGraphs();
    ui->custom_plot->clearPlottables();
    ui->custom_plot->plotLayout()->clear();
    ui->custom_plot->replot();
    QCPAxisRect *axis_rect = new QCPAxisRect(ui->custom_plot);
    ui->custom_plot->plotLayout()->addElement(0, 0, axis_rect);
    QCPColorMap *color_map = new QCPColorMap(axis_rect->axis(QCPAxis::atBottom), axis_rect->axis(QCPAxis::atLeft));
    QSharedPointer<QCPAxisTickerText> text_ticker(new QCPAxisTickerText);
    color_map->data()->setSize(k, k);
    color_map->data()->setRange(QCPRange(0, k-1), QCPRange(0, k-1));
    for (int i = 0; i < k; i++) {
        for (int j = 0; j < k; j++) {
            color_map->data()->setCell(i, j, correlation[i][j]);
            QCPItemText *textLabel = new QCPItemText(ui->custom_plot);
            if (correlation[i][j] > 0)
                textLabel->setColor(Qt::white);
            else
                textLabel->setColor(Qt::black);
            textLabel->setPositionAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            textLabel->position->setAxes(axis_rect->axis(QCPAxis::atBottom), axis_rect->axis(QCPAxis::atLeft));
            textLabel->position->setAxisRect(axis_rect);
            textLabel->setClipToAxisRect(true);
            textLabel->position->setCoords(i, j); // 设置位置
            textLabel->setText(QString::number(correlation[i][j])); // 显示数值
        }
        text_ticker->addTick(i, QString::fromStdString(std::to_string(i)));
    }
    axis_rect->axis(QCPAxis::atLeft)->setTicker(text_ticker);
    axis_rect->axis(QCPAxis::atBottom)->setTicker(text_ticker);
    axis_rect->axis(QCPAxis::atBottom)->setTickLabelRotation(15);
    axis_rect->axis(QCPAxis::atLeft)->setTickLength(0);
    axis_rect->axis(QCPAxis::atBottom)->setTickLength(0);
    axis_rect->axis(QCPAxis::atLeft)->grid()->setPen(Qt::NoPen);
    axis_rect->axis(QCPAxis::atBottom)->grid()->setPen(Qt::NoPen);
    axis_rect->axis(QCPAxis::atLeft)->grid()->setZeroLinePen(Qt::NoPen);
    axis_rect->axis(QCPAxis::atBottom)->grid()->setZeroLinePen(Qt::NoPen);
    axis_rect->axis(QCPAxis::atLeft)->setRange(-0.5, k-0.5);
    axis_rect->axis(QCPAxis::atBottom)->setRange(-0.5, k-0.5);
    QCPColorScale *color_scale = new QCPColorScale(ui->custom_plot);
    ui->custom_plot->plotLayout()->addElement(0, 1, color_scale);
    color_scale->setType(QCPAxis::atRight);
    color_scale->setDataRange(QCPRange(-1.0, 1.0));
    color_map->setColorScale(color_scale);
    QCPColorGradient gradient;
    gradient.setColorStopAt(0.0, QColor("#ffffd0"));
    gradient.setColorStopAt(0.5, QColor("#3eb6c5"));
    gradient.setColorStopAt(1.0, QColor("#042060"));
    color_map->setGradient(gradient);
    color_map->setInterpolate(false);
    // 保护区结束
    ui->custom_plot->replot();
    correlation.clear();
    arrays.clear();
    QMessageBox::information(this, "报告", "相关系数热力图绘制完成。");
    compared.clear();
}

void MainWindow::on_jumpButton_clicked()
{
    inputPath = ui->inputPathLine->text().toStdString();
    std::filesystem::path iPath(inputPath);
    outputPath = iPath.parent_path().string() + "/output.txt";
    indexPath = iPath.parent_path().string() + "/index.txt";
    ui->candleButton->setEnabled(true);
    ui->candleButton->setStyleSheet("");
    ui->heatButton->setEnabled(true);
    ui->heatButton->setStyleSheet("");
    ui->predictButton->setEnabled(true);
    ui->predictButton->setStyleSheet("");
    loadIndex(indexPath);
}


void MainWindow::on_codeLine_returnPressed() {
    QString code_h = ui->codeLine->text() + ".SZ";
    if (compared.size() < k) {
        if (code_h.length() == 9)
            compared.push_back(code_h);
        else
            QMessageBox::warning(this, "注意", "请输入六位股票代码");
    }
    else {
        QMessageBox::warning(this, "注意", "只能读取十只股票");
    }
    ui->codeLine->setText(QString::fromStdString("请输入第" + std::to_string(compared.size() + 1) + "条数据："));
}


void MainWindow::on_predictButtonClicked() {
    std::string month_p = ui->timeLine->text().toStdString();
    std::string stock_p = ui->codeLine->text().toStdString();
    QVector<double> lastMonth;
    QVector<double> predicted;
    QVector<double> actual;
    lastMonth.clear();
    predicted.clear();
    actual.clear();
    loadMonthData(stock_p, month_p, lastMonth);
    std::string nextMonth_p = toNextMonth(month_p);
    loadMonthData(stock_p, nextMonth_p, actual);
    double mean_y = std::accumulate(lastMonth.begin(), lastMonth.end(), 0.0) / 31;
    double mean_x = 16, de_x = 0, co = 0;
    for (int i = 1; i <= 31; i++) {
        de_x += i*i;
        co += i*lastMonth[i-1];
    }
    de_x /= 31;
    co /= 31;
    double reg_k = (co - mean_x*mean_y) / (de_x - mean_x*mean_x);
    double reg_b = mean_y - mean_x*reg_k;
    for (int i = 0; i < 31; i++) {
        predicted.push_back((32 + i)*reg_k + reg_b);
    }
    // 以下为保护区，禁止修改
    ui->custom_plot->clearItems();
    ui->custom_plot->clearGraphs();
    ui->custom_plot->clearPlottables();
    ui->custom_plot->plotLayout()->clear();
    ui->custom_plot->replot();
    QCPAxisRect *last_axis_rect = new QCPAxisRect(ui->custom_plot), *next_axis_rect = new QCPAxisRect(ui->custom_plot);
    ui->custom_plot->plotLayout()->addElement(0, 0, last_axis_rect);
    ui->custom_plot->plotLayout()->addElement(0, 1, next_axis_rect);
    QCPGraph *last_line_plot = new QCPGraph(last_axis_rect->axis(QCPAxis::atBottom), last_axis_rect->axis(QCPAxis::atLeft));
    QCPGraph *next_target_line_plot = new QCPGraph(next_axis_rect->axis(QCPAxis::atBottom), next_axis_rect->axis(QCPAxis::atLeft));
    QCPGraph *next_predict_line_plot = new QCPGraph(next_axis_rect->axis(QCPAxis::atBottom), next_axis_rect->axis(QCPAxis::atLeft));
    last_line_plot->setPen(QPen(Qt::red));
    next_target_line_plot->setPen(QPen(Qt::red));
    next_predict_line_plot->setPen(QPen(Qt::green));
    for (int i = 1; i <= 31; i++) {
        last_line_plot->addData(i, lastMonth[i-1]);
        next_target_line_plot->addData(i, predicted[i-1]);
        next_predict_line_plot->addData(i, actual[i-1]);
    }
    for (QCPAxis *axis : last_axis_rect->axes()) {
        axis->setLayer("axes");
        axis->grid()->setLayer("grid");
    }
    for (QCPAxis *axis : next_axis_rect->axes()) {
        axis->setLayer("axes");
        axis->grid()->setLayer("grid");
    }
    ui->custom_plot->rescaleAxes();
    last_axis_rect->axis(QCPAxis::atBottom)->setRange(0, 32);
    next_axis_rect->axis(QCPAxis::atBottom)->setRange(0, 32);
    ui->custom_plot->replot();
    // 保护区结束
    QMessageBox::information(this, "报告", "预测已完成。");
}


//以下不再为槽函数，作为辅助函数


void MainWindow::loadIndex(const std::string &path) {
    dict.clear();
    std::ifstream indexFile(path);
    std::string line;
    if (!indexFile.is_open())
        throw std::runtime_error("打开索引失败TT");
    while (std::getline(indexFile, line)) {
        std::istringstream iss(line);
        std::string symbolCode, tradeMonth;
        std::string tmpIndex;
        if (std::getline(iss, symbolCode, ',') && (std::getline(iss, tradeMonth, ',')) && (iss >> tmpIndex)) {
            std::string key = symbolCode + "_" + tradeMonth;
            long long index = std::stoll(tmpIndex);
            dict[key] = index;
        }
    }
    indexFile.close();
}


StockData MainWindow::lineToData(const std::string &line) {
    StockData data;
    std::istringstream in(line);
    std::string field;
    std::vector<std::string> fields;
    while (std::getline(in, field, ','))
        fields.push_back(field);
    int s = fields.size();
    if (s != 11) {
        QMessageBox::warning(this, "警告", "不好，数据格式有误");
        return data;
    }
    data.symbol_code = fields[0];
    data.trade_date = fields[1];
    data.open = std::stod(fields[2]);
    data.high = std::stod(fields[3]);
    data.low = std::stod(fields[4]);
    data.close = std::stod(fields[5]);
    data.pre_close = std::stod(fields[6]);
    data.change = std::stod(fields[7]);
    data.percent_change = std::stod(fields[8]);
    data.volume = std::stod(fields[9]);
    data.turnover = std::stod(fields[10]);
    return data;
}


double MainWindow::calculatePearson(const QVector<double> &a, const QVector<double> &b) {
    if (a.size() != b.size() || a.empty())
        return 0.0;
    double mean_a = std::accumulate(a.begin(), a.end(), 0.0) / a.size();
    double mean_b = std::accumulate(b.begin(), b.end(), 0.0) / b.size();
    double numerator = 0.0, denominator_x = 0.0, denominator_y = 0.0;
    for (int i = 0; i < a.size(); i++) {
        double diff_x = a[i] - mean_a;
        double diff_y = b[i] - mean_b;
        numerator += diff_x * diff_y;
        denominator_x += diff_x * diff_x;
        denominator_y += diff_y * diff_y;
    }
    double denominator = std::sqrt(denominator_x) * std::sqrt(denominator_y);
    return (denominator == 0) ? 0 : numerator / denominator;
}

void MainWindow::loadMonthData(const std::string &targetStock, const std::string &targetMonth, QVector<double> &arr) {
    arr.clear();
    std::ifstream ifs;
    ifs.open(outputPath, std::ios::binary);
    if (!ifs.is_open())
        throw std::runtime_error("源文件打开失败TT");
    long long loc = dict[targetStock + ".SZ_" + targetMonth];
    ifs.seekg(loc);
    std::string line;
    StockData data;
    if (loc == 0)
        std::getline(ifs, line);
    bool first = true;
    while (std::getline(ifs, line)) {
        data = lineToData(line);
        int day = std::stoi(data.trade_date.substr(6, 2));
        if (first && day > 1) {
            for (int j = 1; j < day; j++) {
                arr.push_back(data.close);
            }
        }
        if (day > arr.size() + 1) {
            double gradient = ((data.close - arr.last()) / (day - arr.size()));
            for (int j = 1; j < day - arr.last(); j++) {
                arr.push_back(arr.last() + gradient);
            }
        }
        arr.push_back(data.close);
        first = false;
        if (data.trade_date.substr(0, 6) != targetMonth)
            break;
    }
    while (arr.size() < 31) {
        arr.push_back(arr.last());
    }
    while (arr.size() > 31) {
        arr.pop_back();
    }
}

std::string MainWindow::toNextMonth(const std::string &current) {
    std::string ans;
    if (current.substr(4, 2) == "12")
        ans = std::to_string(std::stoi(current.substr(0, 4)) + 1) + "01";
    else if (current.substr(4, 2) == "09")
        ans = current.substr(0, 4) + "10";
    else
        ans = current.substr(0, 5) + std::to_string(std::stoi(current.substr(5, 1)) + 1);
    return ans;
}
