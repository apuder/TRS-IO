`timescale 1ns / 1ps

module trigger(
  input clk,
  input cond,
  output one,
  output reg two,
  output reg three);

assign one = cond;

always @(posedge clk) begin
  two <= one;
  three <= two;
end

endmodule
