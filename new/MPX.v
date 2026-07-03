`timescale 1ns / 1ps

module MPX(
    input clk,
    input [6:0] s0,s1,s2,s3,s4,s5,s6,s7,
    output reg [6:0] segbus,
    output reg [7:0] ANbus
    );
    
reg [15:0] cnt = 0;
reg [2:0] dgt = 0;

always@(posedge clk) 
begin
cnt <= cnt + 1;
if (cnt ==0) 
dgt <= dgt + 1;
end

always @(*)
begin
 case(dgt)
 0: begin segbus = s0; ANbus = 8'b11111110; end
 1: begin segbus = s1; ANbus = 8'b11111101; end
 2: begin segbus = s2; ANbus = 8'b11111011; end
 3: begin segbus = s3; ANbus = 8'b11110111; end
 4: begin segbus = s4; ANbus = 8'b11101111; end
 5: begin segbus = s5; ANbus = 8'b11011111; end
 6: begin segbus = s6; ANbus = 8'b10111111; end
 7: begin segbus = s7; ANbus = 8'b01111111; end
 endcase
end

endmodule
