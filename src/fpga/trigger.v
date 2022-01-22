`timescale 1ns / 1ps

module trigger(
  input clk,
  input trigger_action,
  input cmd,
  output one,
  output reg two,
  output reg three);

assign one = trigger_action && cmd;

always @(posedge clk) begin
  two <= one;
  three <= two;
end

endmodule
