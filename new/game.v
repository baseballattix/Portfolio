`timescale 1ns / 1ps

module game( 
input  clk, rst,
input  up, dwn, lft, rgt,
input  [2:0] sw,
output [6:0] seg,
output [7:0] AN,
output reg [1:0] RED,
output reg [15:0] LED
);

parameter sec1 = 100_000_000;
parameter sec2 = 300_000_000;
parameter delay = 10_000_000;
parameter sechalf = 50_000_000;

wire [6:0] s0,s1,s2,s3,s4,s5,s6,s7;
reg  [3:0] d0,d1,d2,d3,d4,d5,d6,d7;

wire [3:0] lfsr_out;
LFSR_4 rng(clk, rst, lfsr_out);
reg [3:0] rand ;
wire [2:0] sel = (lfsr_out % 7);

always@(*) begin
    case(sel) 
        0: rand = 4'd1;
        1: rand = 4'd2;
        2: rand = 4'd3;
        3: rand = 4'd10; // up
        4: rand = 4'd11; // right
        5: rand = 4'd12; // left
        6: rand = 4'd13; // down
    endcase
end

reg [3:0] curr, next;
reg [31:0] cnt;
reg [3:0] tmp;
reg [3:0] target1;  
reg [3:0] target2; 
reg [3:0] target3; 
reg [3:0] target4;
reg [3:0] user_val;
reg [1:0] fill;

reg [3:0] level = 1;
reg [3:0] nxt_lvl;

reg lvl2flag1; 
reg lvl2flag2;

reg lvl3flag1; 
reg lvl3flag2;
reg lvl3flag3;

// --- New signals for edge detector latch output ---
wire up_db, dwn_db, lft_db, rgt_db;
wire sw0_db, sw1_db, sw2_db;
wire [3:0] score_th, score_h, score_t, score_o;
wire win = (curr == 4'b0011 && next == 4'b0000);
wire fail = (curr == 4'b1000 && next == 4'b0000);

score score_tracker(clk, rst, win, fail, score_th, score_h, score_t, score_o);

edge_detector upb  (clk, up,  rst, up_db);
edge_detector dwnb (clk, dwn, rst, dwn_db);
edge_detector lftb (clk, lft, rst, lft_db);
edge_detector rgtb (clk, rgt, rst, rgt_db);

edge_detector sw0  (clk, sw[0], rst, sw0_db);
edge_detector sw1  (clk, sw[1], rst, sw1_db);
edge_detector sw2  (clk, sw[2], rst, sw2_db);

function [3:0] collect_input;
    input [2:0] sw;
    input up, dwn, lft, rgt;
begin
    if (up_db)        collect_input = 4'd10;
    else if (rgt_db)  collect_input = 4'd11;
    else if (lft_db)  collect_input = 4'd12;
    else if (dwn_db)  collect_input = 4'd13;

    else if (sw0_db) collect_input = 4'd1;
    else if (sw1_db) collect_input = 4'd2;
    else if (sw2_db) collect_input = 4'd3;

    else collect_input = 4'd0;
end
endfunction

always @(posedge clk or posedge rst) begin //target gen reg m
    if (rst) begin
        target1 <= 0;  
        target2 <= 0;
        target3 <= 0;
        target4 <= 0;
    end 
    else if(curr == 4'b0000) begin
        
    case(fill)
    2'b00: tmp = rand;
    2'b01: tmp = rand;
    2'b10: tmp = rand;
    2'b11: tmp = rand;
    endcase

    case(fill)
    2'b00: target1 <= tmp;
    2'b01: target2 <= tmp;
    2'b10: target3 <= tmp;
    2'b11: target4 <= tmp;
    endcase  
    end
end

always@(posedge clk or posedge rst) begin //fill reg for creating target m
    if(rst)
        fill <= 2'b00;
    else if (curr == 4'b0000 && fill < 2'b11)
        fill <= fill + 1;
    else if (curr == 4'b0001 || curr == 4'b0100 || curr == 4'b0110)
        fill <= 2'b00;
end

always @(posedge clk or posedge rst) begin //lvl reg m
    if (rst)
        level <= 1;            
    else
        level <= nxt_lvl;     
end

always @(posedge clk or posedge rst) begin //m 
    if (rst)
        user_val <= 0;
    else if (curr == 4'b0010 || curr == 4'b0101 || curr == 4'b0111)
        user_val <= collect_input({sw2_db, sw1_db, sw0_db},
                                   up_db, dwn_db, lft_db, rgt_db);
    else
        user_val <= 0;
end

always@(posedge clk or posedge rst) begin //cnt reg
    if(rst)
        cnt <= 0;
    else if((curr != next))
        cnt <= 0;
    else
        cnt <= cnt + 1;
end
    
always @(posedge clk or posedge rst) begin // lvl 2 flag reg
    if (rst) begin
        lvl2flag1 <= 0;
        lvl2flag2 <= 0;
    end
    else if (curr == 4'b0100) begin
        lvl2flag1 <= 0;   
        lvl2flag2 <= 0;
    end
    else if (curr == 4'b0101) begin
        if (user_val == target1)
            lvl2flag1 <= 1;
        if (user_val == target2 && (lvl2flag1 == 1))
            lvl2flag2 <= 1;
    end
end

always @(posedge clk or posedge rst) begin // lvl 3 flag reg 
    if (rst) begin
        lvl3flag1 <= 0;
        lvl3flag2 <= 0;
        lvl3flag3 <= 0;
    end
    else if (curr == 4'b0110) begin
        lvl3flag1 <= 0;
        lvl3flag2 <= 0;
        lvl3flag3 <= 0;
    end
    else if (curr == 4'b0111) begin
        if (user_val == target1)
            lvl3flag1 <= 1;
        if (user_val == target2 && (lvl3flag1 == 1))
            lvl3flag2 <= 1;
        if (user_val == target3 && (lvl3flag2 == 1))
            lvl3flag3 <= 1;
    end
end

always @(posedge clk or posedge rst) begin //state reg 
    if (rst)
        curr <= 4'b0000;
    else
        curr <= next;
end

always @(*) begin
     
    nxt_lvl = level;
    d0=15; 
    d1=15; 
    d2=15; 
    d3=15; 
    d7 = score_th;
    d6 = score_h;
    d5 = score_t;
    d4 = score_o;

    LED = 0;
    RED = 0;
    next = curr;

case (curr) 
    
    4'b0000: begin //intial springboard

        if (cnt >= delay && level == 1) begin
            next = 4'b0001;
        end else if(cnt >= delay && level == 2)begin
            next = 4'b0100;
        end else if(cnt >= delay && level == 3) begin
            next = 4'b0110; 
        end else
            next = 4'b0000;
    end

    4'b0001: begin //display lvl 1
        d0 = target1;

        if (cnt >= sec1) begin
            next = 4'b0010;
        end else begin
            next = 4'b0001;
        end
    end

    4'b0010: begin //check lvl 1

        if (user_val != 0 && user_val != target1) begin
            nxt_lvl = 1;
            next = 4'b1000;
        end 
        else if (user_val == target1 && user_val != 0) begin
            nxt_lvl = level + 1;
            next = 4'b0011;
        end 
        else if (cnt >= sec2) begin
            nxt_lvl = 1;
            next = 4'b1000;
        end
    end

    4'b0011: begin //win
        LED = 16'hFFFF;

        if (cnt >= sechalf)
            next = 4'b0000;
    end

    4'b0100: begin //display lvl 2
        d0 = target1;
        d1 = target2;
        
        if(cnt >= sec1) begin
            next = 4'b0101; 
        end else begin
            next = 4'b0100;
        end 
    end

    4'b0101: begin   //lvl 2 check input
    
        if(lvl2flag1 && lvl2flag2) begin
            nxt_lvl = nxt_lvl+1;
            next = 4'b0011;
        end else if (cnt >= sec2) begin
            nxt_lvl = 1;
            next = 4'b1000;
        end else begin
            next = 4'b0101;
        end
        
    end

    4'b0110 : begin //display lvl 3
        d0 = target1;
        d1 = target2;
        d2 = target3; 
        
        if(cnt>=sec1) begin
            next = 4'b0111;
        end else begin
            next = 4'b0110;
        end
     end

    4'b0111 : begin //lvl 3 check input
    
        if (lvl3flag1 && lvl3flag2 && lvl3flag3) begin
             nxt_lvl = 3;
              next = 4'b0011;
        end else if (user_val != 0 &&
            (user_val != target1) &&
            (user_val != target2) &&
            (user_val != target3)) begin
                nxt_lvl = 1;
                next = 4'b1000;
        end else if (cnt >= sec2) begin
                nxt_lvl = 1;
                 next = 4'b1000;
        end else begin
            next = 4'b0111;
        end
    end

    4'b1000 : begin
        RED = 2'b11;
     
        if(cnt>=sec2) begin
            next = 4'b0000;
        end else begin
            next = 4'b1000;
        end
    end

    default:
        next = 4'b0000;

endcase
end

//multiplex and digit intialization
SSD digit0(d0, s0);
SSD digit1(d1, s1);
SSD digit2(d2, s2);
SSD digit3(d3, s3);
SSD digit4(d4, s4);
SSD digit5(d5, s5);
SSD digit6(d6, s6);
SSD digit7(d7, s7);

MPX MPX_Display(
    .clk(clk),
    .s0(s0), .s1(s1), .s2(s2), .s3(s3),
    .s4(s4), .s5(s5), .s6(s6), .s7(s7),
    .segbus(seg),
    .ANbus(AN)
);

endmodule
