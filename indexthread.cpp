#include "indexthread.h"
#include <iostream>
#include <fstream>
#include <sstream>

IndexThread::IndexThread(QObject *parent) :
    QThread(parent) {
}

void IndexThread::setInPath(const std::string &path) {
    inPath = path;
}

void IndexThread::setIndexPath(const std::string &path) {
    indexPath = path;
}

StockData IndexThread::lineToData(const std::string &line) {
    StockData data;
    std::istringstream in(line);
    std::string field;
    std::vector<std::string> fields;
    while (std::getline(in, field, ','))
        fields.push_back(field);
    int s = fields.size();
    if (s != 11) {
        emit updateCurrentState("不好，数据格式有误");
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

void IndexThread::generateIndex(const std::string &inPath, const std::string &indexPath) {
    emit updateCurrentState("开始创建索引！");
    std::ifstream inputFile(inPath, std::ios::binary);
    std::ofstream indexFile(indexPath);
    if (!inputFile.is_open() || !indexFile.is_open())
        throw std::runtime_error("打开文件失败TT");
    std::string line;
    std::streampos pos = inputFile.tellg();
    std::getline(inputFile, line);
    StockData formerData = lineToData(line);
    StockData data;
    indexFile << formerData.symbol_code << "," << formerData.trade_date.substr(0, 6) << "," << pos << std::endl;
    pos = inputFile.tellg();
    while (std::getline(inputFile, line)) {
        data = lineToData(line);
        if (data.symbol_code != formerData.symbol_code || data.trade_date.substr(0, 6) != formerData.trade_date.substr(0, 6)) {
            indexFile << data.symbol_code << "," << data.trade_date.substr(0, 6) << "," << pos << std::endl;
            formerData = data;
        }
        pos = inputFile.tellg();
    }
}

void IndexThread::run() {
    generateIndex(inPath, indexPath);
    emit complete();
}
