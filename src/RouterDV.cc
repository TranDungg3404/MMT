#include <map>
#include <string>
#include <sstream>
#include <iomanip>
#include <omnetpp.h>

using namespace omnetpp;

// Cấu trúc một dòng trong bảng định tuyến mở rộng
struct RouteInfo {
    int cost;
    int nextHopGate;
    std::string fullPath;
};

class RouterDV : public cSimpleModule {
  private:
    std::string myName;
    // Bảng định tuyến nội bộ lưu cấu trúc RouteInfo mở rộng
    std::map<std::string, RouteInfo> routingTable;
    cMessage *updateEvent = nullptr;

    // --- CÁC BIẾN ĐO ĐẠC HIỆU NĂNG (PERFORMANCE METRICS) & HỘI TỤ ---
    long numPacketsSent = 0;     // Tổng số gói tin đã phát đi
    long numPacketsReceived = 0; // Tổng số gói tin nhận thành công
    simtime_t totalDelay = 0;    // Tổng độ trễ tích lũy (s)
    double totalBitsReceived = 0; // Tổng số bits nhận được để tính băng thông
    simtime_t convergenceTime = 0.0; // Thời điểm bảng định tuyến thay đổi lần cuối

  public:
    void sendUpdate() {
        for (int i = 0; i < gateSize("out"); i++) {
            cMessage *msg = new cMessage("RIP_Update");
            msg->setContextPointer((void*)(myName.c_str()));

            // Đóng gói dữ liệu dạng: Dest:Cost:Path|Dest:Cost:Path|...
            std::stringstream ss;
            for (auto const& item : routingTable) {
                ss << item.first << ":" << item.second.cost << ":" << item.second.fullPath << "|";
            }
            msg->setName(ss.str().c_str());

            // Ghi lại thời gian bắt đầu gửi gói tin để đo trễ
            msg->setTimestamp();

            send(msg, "out", i);
            numPacketsSent++; // Tăng biến đếm kiểm tra tỉ lệ truyền
        }
    }

  protected:
    virtual void initialize() override {
        myName = getName();

        // Bản thân tới chính mình: Chi phí = 0, Cổng = -1, Đường đi = Tên nút ban đầu
        RouteInfo selfRoute = {0, -1, myName};
        routingTable[myName] = selfRoute;

        updateEvent = new cMessage("periodicUpdate");
        scheduleAt(simTime() + 10.0, updateEvent);
    }

    virtual void handleMessage(cMessage *msg) override {
        if (msg == updateEvent) {
            sendUpdate();
            scheduleAt(simTime() + 10.0, updateEvent);
        }
        else {
            numPacketsReceived++; // Nhận thành công một gói

            // 1. Tính toán độ trễ đường truyền (Delay = Thời gian hiện tại - Thời gian gửi)
            simtime_t delay = simTime() - msg->getTimestamp();
            totalDelay += delay;

            // 2. Tính toán dung lượng gói tin để đo băng thông (Mỗi ký tự = 8 bits)
            std::string data = msg->getName();
            totalBitsReceived += (data.length() * 8);

            int arrivalGate = msg->getArrivalGate()->getIndex();
            int costToNeighbor = 1;
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
                    int newCost = costToNeighbor + neighborCost;
                    std::string newPath = myName + " -> " + pathStr;

                    if (routingTable.find(dest) == routingTable.end() || newCost < routingTable[dest].cost) {
                        RouteInfo newRoute = {newCost, arrivalGate, newPath};
                        routingTable[dest] = newRoute;
                        tableChanged = true;
                    }
                }
            }

            // --- 3. THEO DÕI TIẾN TRÌNH CẬP NHẬT TỪNG BƯỚC KHI MẠNG ĐANG CHẠY ---
            EV << "\n[Thời gian: " << simTime() << "s] " << myName << " nhận cập nhật từ cổng " << arrivalGate;
            if (tableChanged) {
                convergenceTime = simTime(); // Ghi nhận mốc thời gian thay đổi mới nhất
                EV << " -> BẢNG THAY ĐỔI:\n";
                sendUpdate(); // Kích hoạt Triggered Update
            } else {
                EV << " -> KHÔNG THAY ĐỔI (Đã tối ưu):\n";
            }

            // In nhanh trạng thái bảng hiện tại ra log chạy từng bước
            for (auto const& item : routingTable) {
                EV << "    > Tới: " << item.first
                   << " | Cost: " << item.second.cost
                   << " | Cổng: " << item.second.nextHopGate
                   << " | Path: " << item.second.fullPath << "\n";
            }
            EV << "----------------------------------------------------------------------\n";

            delete msg;
        }
    }

    virtual void finish() override {
        // --- 4. IN BẢNG ĐỊNH TUYẾN CHUẨN ĐỒ HỌA KHI KẾT THÚC ---
        EV << "\n======================================================================\n";
        EV << " Xem bảng định tuyến cuối cùng của: " << myName << "\n";
        EV << "----------------------------------------------------------------------\n";
        EV << " " << std::left << std::setw(12) << "Đích đến" << std::setw(10) << "Chi phí" << std::setw(22) << "Trạm kế tiếp (Gate)" << "Đường đi chi tiết (Path)\n";
        EV << "----------------------------------------------------------------------\n";
        for (auto const& item : routingTable) {
            if (item.first == myName) continue;
            std::string nextHopStr = "Cổng " + std::to_string(item.second.nextHopGate);
            EV << " " << std::left << std::setw(12) << item.first << std::setw(10) << item.second.cost << std::setw(22) << nextHopStr << item.second.fullPath << "\n";
        }
        EV << "----------------------------------------------------------------------\n";
        EV << " >>> THỜI GIAN HỘI TỤ CỦA " << myName << ": " << convergenceTime << " giây <<<\n";
        EV << "======================================================================\n";

        // --- 5. IN ĐÁNH GIÁ PERFORMANCE, ĐỘ TRỄ, BĂNG THÔNG ---
        double pdr = 0.0;
        if (numPacketsSent > 0) {
            pdr = ((double)numPacketsReceived / numPacketsSent) * 100.0;
        }

        double avgDelay = 0.0;
        if (numPacketsReceived > 0) {
            avgDelay = totalDelay.dbl() / numPacketsReceived;
        }

        double throughput = 0.0;
        if (simTime() > 0) {
            throughput = totalBitsReceived / simTime().dbl();
        }

        EV << " [BÁO CÁO HIỆU NĂNG ĐƯỜNG TRUYỀN NÚT: " << myName << "]\n";
        EV << "    > Số gói tin đã gửi đi (Sent): " << numPacketsSent << " gói\n";
        EV << "    > Số gói tin nhận được (Received): " << numPacketsReceived << " gói\n";
        EV << "    > Tỉ lệ truyền gói thành công (PDR): " << std::fixed << std::setprecision(2) << pdr << " %\n";
        EV << "    > Độ trễ trung bình (Average Delay): " << std::scientific << avgDelay << " giây\n";
        EV << "    > Băng thông thực tế tiêu thụ (Throughput): " << std::fixed << std::setprecision(2) << throughput << " bps\n";
        EV << "======================================================================\n\n";
    }

    virtual ~RouterDV() {
        cancelAndDelete(updateEvent);
    }
};

Define_Module(RouterDV);
