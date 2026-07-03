`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 12/08/2025 02:13:36 AM
// Design Name: 
// Module Name: seed
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


module LFSR_4 (
    input  wire CLK,
    input  wire RST,
    output wire [3:0] LFSR_out
);
    parameter [3:0] SEED = 4'b1011;
    reg [3:0] LFSR;

    always @(posedge CLK or posedge RST) begin
        if (RST)
            LFSR <= SEED;
        else begin
            // taps: bit3 XOR bit2
            LFSR <= {LFSR[2:0], LFSR[3] ^ LFSR[2]};
        end
    end

    assign LFSR_out = LFSR;
endmodule
