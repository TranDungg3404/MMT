@license(LGPL);
package mmt.src;

simple RouterDV
{
    gates:
        input in[];
        output out[];
}

network RIP_DistanceVector
{
    @display("bgb=499,296");
    submodules:
        R1: RouterDV {
            @display("p=305,127");
        }
        R2: RouterDV {
            @display("p=242,60");
        }
        R3: RouterDV {
            @display("p=161,127");
        }
        R4: RouterDV {
            @display("p=161,223");
        }
        R5: RouterDV {
            @display("p=242,223");
        }
    connections:
        R1.out++ --> {  delay = 100ms; } --> R2.in++;
        R2.out++ --> {  delay = 100ms; } --> R1.in++;

        R2.out++ --> {  delay = 100ms; } --> R3.in++;
        R3.out++ --> {  delay = 100ms; } --> R2.in++;

        R3.out++ --> {  delay = 100ms; } --> R4.in++;
        R4.out++ --> {  delay = 100ms; } --> R3.in++;

        R4.out++ --> {  delay = 100ms; } --> R5.in++;
        R5.out++ --> {  delay = 100ms; } --> R4.in++;

        R2.out++ --> {  delay = 100ms; } --> R5.in++;
        R5.out++ --> {  delay = 100ms; } --> R2.in++;
}
