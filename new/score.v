`timescale 1ns / 1ps

module score( 
input clk, rst, win, fail, 
output reg [3:0] th, h, t , o
 );

reg [13:0] value;

always @(posedge clk or posedge rst) begin
    if(rst) begin
        value <= 0;
    end else if(fail) begin
        value <= 0;
    end else if(win) begin
        if(value < 9999) begin
            value <= value + 1;
        end
    end
end

always@(*) begin
th = (value /1000) % 10;
h =  (value / 100) % 10;
t = (value /10) % 10;
o = value % 10;
end

        
        
endmodule
