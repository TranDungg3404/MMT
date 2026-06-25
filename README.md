# Đồ án Mạng Máy Tính: Mô phỏng Giao thức Định tuyến IGRP (Distance Vector)

## 📖 Giới thiệu Dự án
Dự án này được xây dựng trên phần mềm mô phỏng **OMNeT++ 6.x**, nhằm mục đích mô phỏng và phân tích chuyên sâu hoạt động của giao thức định tuyến Distance Vector. Cụ thể, thuật toán sử dụng metric của **IGRP** (Interior Gateway Routing Protocol), tính toán chi phí đường đi (Cost) dựa trên hai thông số vật lý thực tế của dây cáp: **Băng thông (Bandwidth)** và **Độ trễ (Delay)**.

Bên cạnh việc trao đổi bảng định tuyến, dự án còn tích hợp tính năng truyền tải gói tin dữ liệu (DATA) xuyên suốt mạng lưới để đánh giá hiệu năng thực tế thông qua các chỉ số: **Xác suất truyền thành công (Packet Delivery Ratio)** và **Độ trễ trung bình End-to-End (Avg Delay)**.

## 📂 Cấu trúc Thư mục và Tệp tin
- **`src/RouterDV.cc`**: Tệp mã nguồn C++ chứa logic cốt lõi của Router. Bao gồm: thuật toán Bellman-Ford, cơ chế chống lặp (Split Horizon), cơ chế cập nhật tức thời (Triggered Update), và logic sinh/chuyển tiếp gói tin DATA.
- **`src/package.ned`**: Tệp định nghĩa cấu trúc mạng (Topology). Khai báo các module Router, các chuẩn cáp `Cap_IGRP` với thông số băng thông/độ trễ khác nhau và sơ đồ kết nối mạng gồm 7 Router.
- **`simulations/omnetpp.ini`**: Tệp cấu hình chứa các kịch bản (Scenarios) điều khiển toàn bộ quá trình chạy mô phỏng.

## 🚀 Các Kịch bản Mô phỏng (Scenarios)
Dự án cung cấp 3 kịch bản chính để đánh giá toàn diện thuật toán:

### 1. Kịch bản 1: Mạng hoạt động bình thường (`KichBan_1_BinhThuong`)
- **Mô tả:** Mạng hoạt động trong điều kiện lý tưởng, không có sự cố vật lý. R1 liên tục sinh gói tin DATA và gửi đến đích R4.
- **Mục đích:** Tạo mốc thống kê cơ sở (Baseline) chứng minh thuật toán tính IGRP Cost hoạt động chính xác, tự động chọn tuyến đường tối ưu nhất. Tỉ lệ truyền tin thành công đạt 100%.

### 2. Kịch bản 2: Tự phục hồi khi Đứt cáp (`KichBan_2_DutCap_R2_R3`)
- **Mô tả:** Giả lập sự cố đứt cáp vật lý giữa R2 và R3 tại giây thứ 150.
- **Mục đích:** Kiểm chứng khả năng tự động cập nhật lại bảng định tuyến (Rerouting) của Distance Vector. Quan sát quá trình hội tụ mới và đánh giá số lượng gói tin DATA bị rớt (Drop) trong thời gian mạng đang tìm đường vòng qua R6.

### 3. Kịch bản 3: Vòng lặp định tuyến - Count-to-Infinity (`KichBan_3_RoutingLoop`)
- **Mô tả:** Tắt hoàn toàn 2 cơ chế bảo vệ là `Split Horizon` và `Triggered Update` trên tất cả các Router. Tiến hành ngắt kết nối mạng cụt (Stub-network) tại R7.
- **Mục đích:** Chứng minh yếu điểm "đếm đến vô cùng" kinh điển của Distance Vector. Quan sát hiện tượng các Router liên tục tung hứng bản tin sai lệch cho nhau làm chi phí (Cost) tăng vọt lên vô cực, gây tắc nghẽn và làm rớt toàn bộ dữ liệu đi qua khu vực này.

## 🛠 Hướng dẫn Chạy Mô phỏng
1. Mở OMNeT++ IDE và Import thư mục dự án vào Workspace.
2. Nhấn nút **Build (F7)** hoặc biểu tượng cái búa để biên dịch mã nguồn C++.
3. Mở tệp `simulations/omnetpp.ini`.
4. Nhấn nút **Run (F5)** trên thanh công cụ (Biểu tượng nút Play màu xanh lá).
5. Trong hộp thoại hiện ra, chọn Kịch bản bạn muốn chạy.
6. Sử dụng chế độ **Express Run (F7)** để tua nhanh quá trình mô phỏng.
7. Khi thanh tiến trình kết thúc, cuộn cửa sổ **Console Log** xuống dưới cùng để xem *Bảng Định Tuyến Cuối Cùng* và *Bảng Tổng Kết Thống Kê Truyền Dữ Liệu*.

## 📊 Chỉ số Thống kê Thu thập
Cuối mỗi kịch bản, hệ thống sẽ in ra một bảng tổng kết duy nhất bao gồm:
- **Tổng số gói đã phát đi (Sent)**
- **Số gói nhận đích thành công (Recv)**
- **Tổng số gói bị rớt trên mạng (Drop)**
- **Xác suất truyền thành công (%)**
- **Độ trễ trung bình (Avg Delay - ms)**
