// By coconut, 2024.6

#include "externalsort.h"
#include "stockdata.h"

typedef std::tuple<StockData, long long> HeapTuple;

struct CompareTuple {
    bool operator()(const HeapTuple &a, const HeapTuple &b) const {
        return std::get<0>(a) < std::get<0>(b);
    }
};

ExternalSort::ExternalSort(QObject *parent) : QObject(parent) {
}

StockData ExternalSort::lineToData(const std::string &line) {
    StockData data;
    std::istringstream in(line);
    std::string field;
    std::vector<std::string> fields;
    while (std::getline(in, field, ','))
        fields.push_back(field);
    int s = fields.size();
    if (s != 11) {
        emit updateSortingState("不好，数据格式有误");
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

void ExternalSort::splitCSV(const std::string &inputPath) {
    std::ifstream inputFile(inputPath);
    if (!inputFile.is_open())
        throw std::runtime_error("打开失败，请输入正确路径");
    const size_t CHUNK_SIZE = 16 * 1024 * 1024;
    size_t currentSize = 0;
    long long cntChunks = 0;
    std::string line;
    std::getline(inputFile, line);
    std::ofstream outFile;
    std::filesystem::path targetPath(inputPath);
    targetPath = targetPath.parent_path();
    while (std::getline(inputFile, line)) {
        if (currentSize == 0) {
            std::ostringstream oss;
            oss << targetPath.string() << "/chunk_" << ++cntChunks << ".csv";
            outFile.open(oss.str(), std::ios::out | std::ios::app);
            if (!outFile.is_open())
                throw std::runtime_error("创建临时文件失败TT");
            tmpFiles.push_back(oss.str());
        }
        outFile << line << std::endl;
        currentSize += line.size();
        if (currentSize >= CHUNK_SIZE) {
            outFile.close();
            currentSize = 0;
        }
    }
    if (outFile.is_open())
        outFile.close();
    inputFile.close();
    emit updateSortingState("源文件读取完毕！");
}

void ExternalSort::sortingRewrite(const std::string &chunkPath) {
    std::ifstream chunk(chunkPath);
    if (!chunk.is_open())
        throw std::runtime_error("读取临时文件失败TT");
    std::vector<StockData> tmpVector;
    std::string line;
    while (std::getline(chunk, line)) {
        StockData data = lineToData(line);
        tmpVector.push_back(data);
    }
    chunk.close();
    std::sort(tmpVector.begin(), tmpVector.end());
    std::ofstream newChunk(chunkPath, std::ios::out | std::ios::trunc);
    if (!newChunk.is_open())
        throw std::runtime_error("重写临时文件失败TT");
    for (const StockData &data : tmpVector)
        data.writeIn(newChunk);
    newChunk.close();
    tmpVector.clear();
}

void ExternalSort::doSort(const std::string &inputPath, const std::string &outputPath) {
    emit updateSortingState("排序开始！");
    splitCSV(inputPath);
    for (std::string &chunkPath : tmpFiles)
        sortingRewrite(chunkPath);
    std::priority_queue<HeapTuple, std::vector<HeapTuple>, CompareTuple> minHeap;
    long long cntChunks = 0;
    std::vector<std::ifstream*> ins;
    for (const std::string &chunkPath : tmpFiles) {
        std::ifstream *in = new std::ifstream(chunkPath, std::ios::in);
        if (!in->is_open())
            throw std::runtime_error("打开临时文件失败TT");
        std::string line;
        if (std::getline(*in, line)) {
            StockData first = lineToData(line);
            if (!first.symbol_code.empty())
                minHeap.push(HeapTuple(first, cntChunks));
        }
        cntChunks++;
        ins.push_back(in);
    }
    std::ofstream outputFile(outputPath, std::ios::out);
    if (!outputFile.is_open())
        throw std::runtime_error("生成路径错误TT");
    std::ifstream *continueIn;
    while (!minHeap.empty()) {
        StockData ready = std::get<0>(minHeap.top());
        ready.writeIn(outputFile);
        long long substituteIndex = std::get<1>(minHeap.top());
        minHeap.pop();
        emit updateSortingState("小顶堆运行中！");
        continueIn = ins[substituteIndex];
        std::string nextLine;
        if (std::getline(*continueIn, nextLine)) {
            StockData next = lineToData(nextLine);
            minHeap.push(HeapTuple(next, substituteIndex));
        }
    }
    for (const auto &chunk : tmpFiles)
        std::filesystem::remove(chunk);
    tmpFiles.clear();
    emit updateSortingState("排序成功！");
}
