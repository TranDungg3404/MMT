#include <map>
#include <string>
#include <sstream>
#include <iomanip>
#include <omnetpp.h>

using namespace omnetpp;

// Định nghĩa VÔ CỰC cho IGRP
#define INFINITY_COST 30000

// --- 4 BIẾN TOÀN CỤC NÀY ĐỂ GOM CHUNG THỐNG KÊ ---
long global_NumDataSent = 0;
long global_NumDataReceived = 0;
long global_NumDataDropped = 0;
double global_TotalDataDelay = 0.0;

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

    cMessage *failEvent = nullptr;
    int brokenGate = -1;

    bool splitHorizonEnabled;
    bool triggeredUpdateEnabled;

    long numPacketsSent = 0;
    long numPacketsReceived = 0;
    simtime_t totalDelay = 0;
    double totalBitsReceived = 0;
    simtime_t convergenceTime = 0.0;

    cMessage *dataGenerateEvent = nullptr; // Sự kiện sinh gói tin DATA

    int numDataSent = 0;       // Số gói đã gửi (chỉ dùng cho nguồn)
    int numDataReceived = 0;   // Số gói nhận thành công (chỉ dùng cho đích)
    int numDataDropped = 0;    // Số gói bị rớt do không tìm thấy đường đi
    simtime_t totalDataDelay = 0; // Tổng thời gian trễ của các gói tin

  public:
    void sendUpdate() {
            for (int i = 0; i < gateSize("out"); i++) {
                // [CỦA KỊCH BẢN 2]: Nếu cổng này đã đứt thì không gửi gì qua đây nữa
                if (i == brokenGate) continue;

                cMessage *msg = new cMessage("IGRP_Update");
                msg->setContextPointer((void*)(myName.c_str()));

                std::stringstream ss;
                for (auto const& item : routingTable) {

                    // ========================================================
                    // [CỦA KỊCH BẢN 3]: BỌC LẠI BẰNG LỆNH IF
                    // Chỉ thực hiện luật Split Horizon (bỏ qua không gửi)
                    // NẾU biến splitHorizonEnabled đang có giá trị là TRUE
                    // ========================================================
                    if (splitHorizonEnabled == true && item.second.nextHopGate == i) {
                        continue;
                    }

                    // Nếu splitHorizonEnabled là FALSE, lệnh if trên bị bỏ qua,
                    // dòng dưới này sẽ được chạy, Router sẽ gửi tuốt mọi thứ (gây ra Loop)
                    ss << item.first << ":" << item.second.cost << ":" << item.second.fullPath << "|";
                }

                std::string payload = ss.str();

                // Nếu bảng rỗng (bị Split Horizon chặn hết), không gửi gói tin thừa
                if (payload.empty()) {
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

        if (myName == "R1") {
                    global_NumDataSent = 0;
                    global_NumDataReceived = 0;
                    global_NumDataDropped = 0;
                    global_TotalDataDelay = 0.0;
                }

        RouteInfo selfRoute = {0, -1, myName};
        routingTable[myName] = selfRoute;

        splitHorizonEnabled = par("enableSplitHorizon").boolValue();
        triggeredUpdateEnabled = par("enableTriggeredUpdate").boolValue();

        updateEvent = new cMessage("periodicUpdate");
        // IGRP gửi cập nhật mỗi 90 giây. Cộng thêm Jitter (nhiễu ngẫu nhiên) để tránh đụng độ
        scheduleAt(simTime() + 90.0 + uniform(0.0, 5.0), updateEvent);

        double failTime = par("failTime").doubleValue();
            std::string failNeighbor = par("failNeighbor").stringValue();

            // Nếu có cài đặt thời gian đứt cáp > 0
            if (failTime > 0 && !failNeighbor.empty()) {
                failEvent = new cMessage("LinkFailureEvent");
                scheduleAt(failTime, failEvent); // Hẹn giờ đứt cáp
            }

            // KHỞI ĐỘNG TÍNH NĂNG SINH DỮ LIỆU (NẾU ĐƯỢC BẬT TRONG .INI)
            if (par("isSender").boolValue()) {
                dataGenerateEvent = new cMessage("GenerateData");
                        // Đợi 10 giây cho bảng định tuyến ban đầu hội tụ ổn định rồi mới bắt đầu gửi DATA
                scheduleAt(simTime() + 10.0, dataGenerateEvent);
                    }

    }

    virtual void handleMessage(cMessage *msg) override {
        // =============================================================
                // KHỐI 1: TỰ SINH GÓI TIN DATA TẠI NGUỒN (MỖI 1 GIÂY)
                // =============================================================
                if (msg == dataGenerateEvent) {
                    std::string dest = par("destNode").stringValue();
                    std::string packetName = "DATA|" + myName + "|" + dest;

                    // Dùng cPacket (có độ lớn Byte) thay vì cMessage thông thường.
                    // Đặt getKind() = 1 để đánh dấu đây là gói DATA (Bản tin Update mặc định Kind = 0)
                    cPacket *dataPkt = new cPacket(packetName.c_str(), 1);
                    dataPkt->setByteLength(1024); // Giả lập file nặng 1KB

                    // Tìm đường đi trong Bảng định tuyến
                    if (routingTable.find(dest) != routingTable.end() && routingTable[dest].cost < INFINITY_COST) {
                        int outGate = routingTable[dest].nextHopGate;
                        send(dataPkt, "out", outGate);
                        numDataSent++;
                        global_NumDataSent++;
                    } else {
                        numDataDropped++;
                        global_NumDataDropped++;// Không có đường đi -> Rớt gói
                        delete dataPkt;
                    }

                    // Lên lịch 1 giây sau gửi tiếp gói nữa
                    scheduleAt(simTime() + 1.0, dataGenerateEvent);
                    return;
                }


                // =============================================================
                // KHỐI 2 & 3: XỬ LÝ ĐỨT CÁP VÀ BẢN TIN UPDATE ĐỊNH KỲ
            if (msg == failEvent) {
                std::string targetNeighbor = par("failNeighbor").stringValue();

                // Tìm cổng đang nối với láng giềng bị đứt
                for (int i = 0; i < gateSize("out"); i++) {
                    cGate *nextGate = gate("out", i)->getNextGate();
                    if (nextGate != nullptr && nextGate->getOwnerModule()->getName() == targetNeighbor) {
                        brokenGate = i;
                        EV << "\n[!!!] " << simTime() << "s: ĐỨT CÁP tại " << myName
                           << " hướng đi " << targetNeighbor << " (Cổng " << i << ") [!!!]\n";
                        break;
                    }
                }

                // Xóa tất cả các đường đi đang mượn cổng này (Gán = Vô cực)
                bool tableChanged = false;
                for (auto& item : routingTable) {
                    if (item.second.nextHopGate == brokenGate) {
                        item.second.cost = INFINITY_COST;
                        item.second.fullPath = "UNREACHABLE";
                        tableChanged = true;
                    }
                }

                // Kích hoạt gửi cập nhật ngay lập tức (Triggered Update)
                if (tableChanged && triggeredUpdateEnabled) {
                                convergenceTime = simTime();
                                sendUpdate();
                            }

                delete msg;
                return;
            }

            // 2. XỬ LÝ GỬI UPDATE ĐỊNH KỲ (Giữ nguyên của nhóm bạn)
            if (msg == updateEvent) {
                sendUpdate();
                scheduleAt(simTime() + 90.0 + uniform(-5.0, 5.0), updateEvent);
                return;
            }

            // KHỐI 4: XỬ LÝ CHUYỂN TIẾP GÓI TIN DATA (KIND == 1)
           // =============================================================
                    if (msg->getKind() == 1) {
                        std::string pktName = msg->getName();
                        std::stringstream ss(pktName);
                        std::string type, src, dest;
                        std::getline(ss, type, '|');
                        std::getline(ss, src, '|');
                        std::getline(ss, dest);

                        // Tình huống A: Gói tin đã tới đích thành công!
                        if (dest == myName) {
                            numDataReceived++;
                            global_NumDataReceived++;
                            simtime_t delay = simTime() - msg->getCreationTime();
                            totalDataDelay += delay;
                            delete msg;
                        }
                        // Tình huống B: Mình chỉ là trạm trung chuyển, phải Forward đi tiếp
                        else {
                            if (routingTable.find(dest) != routingTable.end() && routingTable[dest].cost < INFINITY_COST) {
                                int outGate = routingTable[dest].nextHopGate;
                                send(msg, "out", outGate);
                            } else {
                                numDataDropped++; // Đứt cáp, mất đường -> Drop gói tin giữa chừng
                                global_NumDataDropped++;
                                delete msg;
                            }
                        }
                        return;
                    }

             // =============================================================
            // KHỐI 5: XỬ LÝ NHẬN BẢNG ĐỊNH TUYẾN TỪ LÁNG GIỀNG (CẬP NHẬT BELLMAN-FORD)
            else {
                int arrivalGate = msg->getArrivalGate()->getIndex();

                // Nếu nhận tin từ cổng đã đứt -> Bỏ qua ngay
                if (arrivalGate == brokenGate) {
                    delete msg;
                    return;
                }

                numPacketsReceived++;
                simtime_t delay = simTime() - msg->getTimestamp();
                totalDelay += delay;
                std::string data = msg->getName();
                totalBitsReceived += (data.length() * 8);

                // Tính costToNeighbor (giữ nguyên logic tính IGRP của nhóm)
                int costToNeighbor = 1;
                cChannel *linkChannel = msg->getArrivalGate()->getPreviousGate()->getChannel();
                if (linkChannel != nullptr) {
                    double bw_kbps = linkChannel->par("bandwidth_kbps").doubleValue();
                    double delay_sec = linkChannel->par("delay").doubleValue();
                    double delay_us = delay_sec * 1000000.0;
                    costToNeighbor = (int)((10000000.0 / bw_kbps) + (delay_us / 10.0));
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
                        int newCost = costToNeighbor + neighborCost;
                        if (newCost > INFINITY_COST) newCost = INFINITY_COST; // Tránh tràn số

                        std::string newPath = myName + " -> " + pathStr;

                        // Lấy thông tin cũ để so sánh
                        int oldCost = routingTable.count(dest) ? routingTable[dest].cost : INFINITY_COST;
                        int currentNextHop = routingTable.count(dest) ? routingTable[dest].nextHopGate : -1;

                        if (routingTable.find(dest) == routingTable.end()) {
                            // Thêm mới nếu chi phí < Vô cực
                            if (newCost < INFINITY_COST) {
                                routingTable[dest] = {newCost, arrivalGate, newPath};
                                tableChanged = true;
                            }
                        } else {
                            // LUẬT ĐỊNH TUYẾN ĐỘNG:
                            // Trường hợp A: Tin đến từ láng giềng ĐANG LÀ trạm trung chuyển của ta
                            // BẮT BUỘC nghe theo (dù nó báo giá tăng lên hay đứt mạng = Vô cực)
                            if (arrivalGate == currentNextHop) {
                                if (oldCost != newCost) {
                                    routingTable[dest].cost = newCost;
                                    routingTable[dest].fullPath = (newCost >= INFINITY_COST) ? "UNREACHABLE" : newPath;
                                    tableChanged = true;
                                }
                            }
                            // Trường hợp B: Tin đến từ đường khác -> Chỉ lấy nếu TỐT HƠN
                            else if (newCost < oldCost) {
                                routingTable[dest] = {newCost, arrivalGate, newPath};
                                tableChanged = true;
                            }
                        }
                    }
                }

                if (tableChanged && triggeredUpdateEnabled) {
                                convergenceTime = simTime();
                                sendUpdate();
                            }

                delete msg;
            }
        }

    virtual void finish() override {
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

        // ==========================================================
                // BẢNG TỔNG KẾT DỮ LIỆU ĐƯỢC IN RA BỞI MÁY PHÁT
                // ==========================================================
                if (par("isSender").boolValue()) {
                    EV << "\n======================================================================\n";
                    EV << " >>> BẢNG TỔNG KẾT TRUYỀN DỮ LIỆU TOÀN MẠNG <<<\n";
                    EV << "======================================================================\n";
                    EV << " - Nguồn phát: " << myName << " | Đích nhận: " << par("destNode").stringValue() << "\n";
                    EV << " - Tổng số gói đã phát đi (Sent)     : " << global_NumDataSent << " gói\n";
                    EV << " - Số gói nhận đích thành công (Recv): " << global_NumDataReceived << " gói\n";
                    EV << " - Tổng số gói bị rớt trên mạng      : " << global_NumDataDropped << " gói\n";

                    if (global_NumDataSent > 0) {
                        double deliveryRatio = ((double)global_NumDataReceived / global_NumDataSent) * 100.0;
                        EV << " - XÁC SUẤT TRUYỀN THÀNH CÔNG        : " << std::fixed << std::setprecision(2) << deliveryRatio << " %\n";
                    }

                    if (global_NumDataReceived > 0) {
                        double avgDelay = (global_TotalDataDelay / global_NumDataReceived) * 1000.0; // đổi ra ms
                        EV << " - Độ trễ trung bình (Avg Delay)     : " << std::fixed << std::setprecision(2) << avgDelay << " ms\n";
                    }
                    EV << "======================================================================\n";
                }

    }

    virtual ~RouterDV() {
        cancelAndDelete(updateEvent);
        cancelAndDelete(failEvent);
        cancelAndDelete(dataGenerateEvent);
    }
};

Define_Module(RouterDV);
