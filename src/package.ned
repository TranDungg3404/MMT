@license(LGPL);
package mmt.src;

simple RouterDV
{
    gates:
        input in[];
        output out[];
}

// 1. ĐỊNH NGHĨA LOẠI CÁP IGRP (Chứa Băng thông và Độ trễ)
channel Cap_IGRP extends ned.DelayChannel {
    delay = default(10ms);           // Độ trễ mặc định
    double bandwidth_kbps = default(100000); // Băng thông mặc định (kbps)
}

network RIP_DistanceVector
{
    @display("bgb=600,400");
    submodules:
        R1: RouterDV { @display("p=500,200"); }
        R2: RouterDV { @display("p=400,300"); }
        R3: RouterDV { @display("p=200,300"); }
        R4: RouterDV { @display("p=100,200"); }
        R5: RouterDV { @display("p=200,100"); }
        R6: RouterDV { @display("p=400,100"); }
        
    connections:
        // --- 1. Tuyến 1 <-> 2 (Mức giá 2) ---
        R1.out++ --> Cap_IGRP { delay = 20ms; bandwidth_kbps = 50000; } --> R2.in++;
        R2.out++ --> Cap_IGRP { delay = 20ms; bandwidth_kbps = 50000; } --> R1.in++;

        // --- 2. Tuyến 1 <-> 6 (Mức giá 5 - Cáp siêu chậm) ---
        R1.out++ --> Cap_IGRP { delay = 50ms; bandwidth_kbps = 20000; } --> R6.in++;
        R6.out++ --> Cap_IGRP { delay = 50ms; bandwidth_kbps = 20000; } --> R1.in++;

        // --- 3. Tuyến 2 <-> 3 (Mức giá 1 - Cáp cực xịn) ---
        R2.out++ --> Cap_IGRP { delay = 10ms; bandwidth_kbps = 100000; } --> R3.in++;
        R3.out++ --> Cap_IGRP { delay = 10ms; bandwidth_kbps = 100000; } --> R2.in++;

        // --- 4. Tuyến 2 <-> 6 (Mức giá 1) ---
        R2.out++ --> Cap_IGRP { delay = 10ms; bandwidth_kbps = 100000; } --> R6.in++;
        R6.out++ --> Cap_IGRP { delay = 10ms; bandwidth_kbps = 100000; } --> R2.in++;

        // --- 5. Tuyến 3 <-> 4 (Mức giá 1) ---
        R3.out++ --> Cap_IGRP { delay = 10ms; bandwidth_kbps = 100000; } --> R4.in++;
        R4.out++ --> Cap_IGRP { delay = 10ms; bandwidth_kbps = 100000; } --> R3.in++;

        // --- 6. Tuyến 3 <-> 5 (Mức giá 2) ---
        R3.out++ --> Cap_IGRP { delay = 20ms; bandwidth_kbps = 50000; } --> R5.in++;
        R5.out++ --> Cap_IGRP { delay = 20ms; bandwidth_kbps = 50000; } --> R3.in++;

        // --- 7. Tuyến 3 <-> 6 (Mức giá 3) ---
        R3.out++ --> Cap_IGRP { delay = 30ms; bandwidth_kbps = 33333; } --> R6.in++;
        R6.out++ --> Cap_IGRP { delay = 30ms; bandwidth_kbps = 33333; } --> R3.in++;

        // --- 8. Tuyến 4 <-> 5 (Mức giá 2) ---
        R4.out++ --> Cap_IGRP { delay = 20ms; bandwidth_kbps = 50000; } --> R5.in++;
        R5.out++ --> Cap_IGRP { delay = 20ms; bandwidth_kbps = 50000; } --> R4.in++;

        // --- 9. Tuyến 5 <-> 6 (Mức giá 3) ---
        R5.out++ --> Cap_IGRP { delay = 30ms; bandwidth_kbps = 33333; } --> R6.in++;
        R6.out++ --> Cap_IGRP { delay = 30ms; bandwidth_kbps = 33333; } --> R5.in++;
}