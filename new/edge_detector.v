module edge_detector
(
    input clk, 
    input in, 
    input rst, 
    output out
);

reg prev;

always @(posedge clk or posedge rst) begin
    if (rst)
        prev <= 0;
    else
        prev <= in;
end

assign out = in & ~prev;

endmodule

