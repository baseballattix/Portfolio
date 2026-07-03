
module SSD(
    input [3:0] bin_in, 
    output [6:0] hex_out
    );
    reg [6:0] hex_out;
 
         
         always @(bin_in)
            begin
            case (bin_in) //case statement
            0 : hex_out = 7'b0000001;
            1 : hex_out = 7'b1001111; //1
            2 : hex_out = 7'b0010010; //2
            3 : hex_out = 7'b0000110; //3
            4 : hex_out = 7'b1001100;
            5 : hex_out = 7'b0100100;
            6 : hex_out = 7'b0100000;
            7 : hex_out = 7'b0001111;
            8 : hex_out = 7'b0000000;
            9 : hex_out = 7'b0000100;
            10 : hex_out = 7'b0111111; //up
            11 : hex_out = 7'b1101111; //right
            12 : hex_out = 7'b1111011; //left
            13 : hex_out = 7'b1110111; //down
            14 : hex_out = 7'b1111111;
           // 8 : hex_out = 7'b0110001; //C
            
            //switch off 7 segment character 
            default : hex_out = 7'b1111111; 
        endcase
    end
endmodule
