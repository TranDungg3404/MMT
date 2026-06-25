#include <map>
#include <string>
#include <sstream>
#include <iomanip>
#include <omnetpp.h>

using namespace omnetpp;

// Định nghĩa VÔ CỰC cho IGRP (Vì IGRP cộng dồn Bandwidth nên số sẽ lớn, dùng 999999 làm vô cực)
#define INFINITY_COST 999999

struct RouteInfo {
    int cost;
    int nextHopGate;
    std::string fullPath;
};

class RouterDV : public cSimpleModule {
  private:
    std::string myName;
    std::map<std::string, RouteInfo> routingTable;
    cMessage *updateEvent = nullptr;

    long numPacketsSent = 0;
    long numPacketsReceived = 0;
    simtime_t totalDelay = 0;
    double totalBitsReceived = 0;
    simtime_t convergenceTime = 0.0;

  public:
    void sendUpdate() {
        for (int i = 0; i < gateSize("out"); i++) {
            cMessage *msg = new cMessage("IGRP_Update");
            msg->setContextPointer((void*)(myName.c_str()));

            std::stringstream ss;
            for (auto const& item : routingTable) {
                // --- LUẬT SPLIT HORIZON ---
                // Nếu đích đến này đang đi qua cổng 'i', KHÔNG quảng bá ngược lại cổng 'i' nữa
                if (item.second.nextHopGate == i) {
                    continue;
                }
                ss << item.first << ":" << item.second.cost << ":" << item.second.fullPath << "|";
            }

            std::string payload = ss.str();
            if (payload.empty()) { // Nếu bị Split Horizon chặn hết, không gửi gói tin thừa
                delete msg;
                continue;
            }

            msg->setName(payload.c_str());
            msg->setTimestamp();
            send(msg, "out", i);
            numPacketsSent++;
        }
    }

  protected:
    virtual void initialize() override {
        myName = getName();

        RouteInfo selfRoute = {0, -1, myName};
        routingTable[myName] = selfRoute;

        updateEvent = new cMessage("periodicUpdate");
        // IGRP gửi cập nhật mỗi 90 giây. Cộng thêm Jitter (nhiễu ngẫu nhiên) để tránh đụng độ
        scheduleAt(simTime() + 90.0 + uniform(0.0, 5.0), updateEvent);
    }

    virtual void handleMessage(cMessage *msg) override {
        if (msg == updateEvent) {
            sendUpdate();
            // Lên lịch cho 90 giây tiếp theo
            scheduleAt(simTime() + 90.0 + uniform(-5.0, 5.0), updateEvent);
        }
        else {
            numPacketsReceived++;
            simtime_t delay = simTime() - msg->getTimestamp();
            totalDelay += delay;
            std::string data = msg->getName();
            totalBitsReceived += (data.length() * 8);

            int arrivalGate = msg->getArrivalGate()->getIndex();

            int costToNeighbor = 1;

                        // LẤY TRỰC TIẾP KÊNH TRUYỀN TỪ CỔNG XUẤT PHÁT (Previous Gate)
                        cChannel *linkChannel = msg->getArrivalGate()->getPreviousGate()->getChannel();

                        if (linkChannel != nullptr) {
                            // Đọc băng thông (kbps) và độ trễ (s) từ thuộc tính của cáp
                            double bw_kbps = linkChannel->par("bandwidth_kbps").doubleValue();
                            double delay_sec = linkChannel->par("delay").doubleValue();

                            // Công thức IGRP: Đổi delay ra microseconds (us)
                            double delay_us = delay_sec * 1000000.0;

                            // Tính Metric = (10,000,000 / Băng thông_kbps) + (Độ trễ_us / 10)
                            double igrp_metric = (10000000.0 / bw_kbps) + (delay_us / 10.0);
                            costToNeighbor = (int)igrp_metric;
                        }

            bool tableChanged = false;
            std::stringstream ss(data);
            std::string item;

            while (std::getline(ss, item, '|')) {
                if (item.empty()) continue;

                std::stringstream itemSS(item);
                std::string dest, costStr, pathStr;

                std::getline(itemSS, dest, ':');
                std::getline(itemSS, costStr, ':');
                std::getline(itemSS, pathStr);

                if (!dest.empty() && !costStr.empty()) {
                    int neighborCost = std::stoi(costStr);
                    // Ngăn chặn cộng dồn vượt quá Vô cực
                    if (neighborCost >= INFINITY_COST) continue;

                    int newCost = costToNeighbor + neighborCost;
                    std::string newPath = myName + " -> " + pathStr;

                    if (routingTable.find(dest) == routingTable.end() || newCost < routingTable[dest].cost) {
                        RouteInfo newRoute = {newCost, arrivalGate, newPath};
                        routingTable[dest] = newRoute;
                        tableChanged = true;
                    }
                }
            }

            // Kích hoạt TRIGGERED UPDATE nếu bảng định tuyến có đường đi tốt hơn
            if (tableChanged) {
                convergenceTime = simTime();
                sendUpdate();
            }

            delete msg;
        }
    }

    virtual void finish() override {
        // [Phần in log đồ họa ra console được giữ nguyên]
        EV << "\n======================================================================\n";
        EV << " Xem bảng định tuyến cuối cùng của: " << myName << "\n";
        EV << "----------------------------------------------------------------------\n";
        EV << " " << std::left << std::setw(12) << "Đích đến" << std::setw(15) << "Chi phí (IGRP)" << std::setw(22) << "Trạm kế tiếp (Gate)" << "Đường đi chi tiết (Path)\n";
        EV << "----------------------------------------------------------------------\n";
        for (auto const& item : routingTable) {
            if (item.first == myName) continue;
            std::string nextHopStr = "Cổng " + std::to_string(item.second.nextHopGate);
            EV << " " << std::left << std::setw(12) << item.first << std::setw(15) << item.second.cost << std::setw(22) << nextHopStr << item.second.fullPath << "\n";
        }
        EV << "----------------------------------------------------------------------\n";
        EV << " >>> THỜI GIAN HỘI TỤ CỦA " << myName << ": " << convergenceTime << " giây <<<\n";
        EV << "======================================================================\n";
    }

    virtual ~RouterDV() {
        cancelAndDelete(updateEvent);
    }
};

Define_Module(RouterDV);
