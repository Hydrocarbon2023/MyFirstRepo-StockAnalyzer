#ifndef STOCKDATA_H
#define STOCKDATA_H
#include <fstream>
#include <string>

struct StockData {
    std::string symbol_code, trade_date;
    double open, high, low, close, pre_close, change, percent_change, volume, turnover;
    StockData() :
            open(0), high(0), low(0), close(0), pre_close(0),
            change(0), percent_change(0),
            volume(0), turnover(0) {
    }
    bool operator<(const StockData &another) const {
        if (symbol_code != another.symbol_code)
            return symbol_code < another.symbol_code;
        return trade_date < another.trade_date;
    }
    void writeIn(std::ofstream &file) const {
        file << symbol_code << "," << trade_date << "," << open << "," << high\
            << "," << low << "," << close << "," << pre_close << "," << change\
            << "," << percent_change << "," << volume << "," << turnover << std::endl;
    }
};

#endif // STOCKDATA_H
