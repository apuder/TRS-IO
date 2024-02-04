`timescale 1ns / 1ps

// This module was contributed by Mathew Boytim <maboytim@yahoo.com>

module vga(
  input clk,
  input srst,
  input vga_clk, // 25 MHz
  input [8:0] TRS_A,
  input [7:0] TRS_D,
  input TRS_OUT,
  input TRS_IN,
  input io_access,
  output [7:0] hires_dout,
  output hires_dout_rdy,
  output hires_enable,
  output VGA_VID,
  output VGA_HSYNC,
  output VGA_VSYNC,
  input genlock);


// The VGA display is 640x480.
// Each row of the TRS-80 display is repeated two times for an effective resolution of
// 640x240 which larger than the 512x192 native resolution of the M3 text display
// resulting in a small border around the M3 text display.
// In 64x16 mode the characters are 6x12 or 6x24 when rows are repeated.
// For convenience the VGA X and Y counters are partitioned into high and low parts which
// count the character position and the position within the character resepctively.
reg [2:0] vga_xxx;     // 0-7
reg [6:0] vga_XXXXXXX; // 0-79 active, -99 total
reg [4:0] vga_yyyy_y;  // 0-23 in 64x16 mode
reg [4:0] vga_YYYYY;   // 0-19 active, -20-20/24 total in 64x16 mode
// VGA in active area.
wire vga_act = ( (vga_XXXXXXX < 7'd80)
               & (vga_YYYYY < 5'd20));

// Instantiate the display RAM.  The display RAM is dual port.
// The A port is connected to the z80.
// The B port is connected to the video logic.
wire [7:0] _z80_dsp_data;
wire [7:0] z80_dsp_data_b;

// Center the 64x16 text display in the 640x480 VGA display.
wire [6:0] dsp_XXXXXXX = vga_XXXXXXX - 7'd8;
wire [2:0] dsp_xxx     = vga_xxx;
wire [4:0] dsp_YYYYY   = vga_YYYYY   - 5'd2;
wire [4:0] dsp_yyyy_y  = vga_yyyy_y;

// Display in active area.
wire dsp_act = ((dsp_XXXXXXX < 7'd64) & (dsp_YYYYY < 5'd16));


// hires graphics.
wire [6:0] hires_XXXXXXX = vga_XXXXXXX;
wire [7:0] hires_YYYYYYYY = (vga_YYYYY << 3) + (vga_YYYYY << 2) + vga_yyyy_y[4:1]; // 0-239 active
// Hires in active area.
wire hires_act = vga_act;

// Hi-res board
wire z80_hires_x_sel_out = (TRS_A == 9'h80) & ~TRS_OUT; // 80
wire z80_hires_y_sel_out = (TRS_A == 9'h81) & ~TRS_OUT; // 81
wire z80_hires_data_sel_out = (TRS_A == 9'h82) & ~TRS_OUT; // 82
wire z80_hires_data_sel_in = (TRS_A == 9'h82) & ~TRS_IN; // 82
wire z80_hires_options_sel_out = (TRS_A == 9'h83) & ~TRS_OUT; // 83

reg [6:0] z80_hires_x_reg;
reg [7:0] z80_hires_y_reg;
reg [7:0] z80_hires_options_reg = 8'hFC;
wire hires_options_graphics_alpha_n = z80_hires_options_reg[0];
wire hires_options_unused           = z80_hires_options_reg[1];
wire hires_options_x_dec_inc_n      = z80_hires_options_reg[2];
wire hires_options_y_dec_inc_n      = z80_hires_options_reg[3];
wire hires_options_x_read_clk_n     = z80_hires_options_reg[4];
wire hires_options_y_read_clk_n     = z80_hires_options_reg[5];
wire hires_options_x_write_clk_n    = z80_hires_options_reg[6];
wire hires_options_y_write_clk_n    = z80_hires_options_reg[7];


wire hires_rd_en, hires_rd_regce;

trigger hires_rd_trigger (
  .clk(clk),
  .cond(io_access & z80_hires_data_sel_in),
  .one(hires_rd_en),
  .two(hires_rd_regce),
  .three(hires_dout_rdy)
);

wire hires_wr_en;

trigger hires_wr_trigger (
  .clk(clk),
  .cond(io_access & z80_hires_data_sel_out),
  .one(),
  .two(hires_wr_en),
  .three()
);


always @(posedge clk)
begin
   if(io_access & z80_hires_x_sel_out)
      z80_hires_x_reg <= TRS_D[6:0];
   else if((hires_rd_en & ~hires_options_x_read_clk_n ) | 
           (hires_wr_en & ~hires_options_x_write_clk_n) )
   begin
      if(hires_options_x_dec_inc_n)
         z80_hires_x_reg <= z80_hires_x_reg - 7'd1;
      else
         z80_hires_x_reg <= z80_hires_x_reg + 7'd1;
   end

   if(io_access & z80_hires_y_sel_out)
      z80_hires_y_reg <= TRS_D;
   else if((hires_rd_en & ~hires_options_y_read_clk_n ) | 
           (hires_wr_en & ~hires_options_y_write_clk_n) )
   begin
      if(hires_options_y_dec_inc_n)
         z80_hires_y_reg <= z80_hires_y_reg - 8'd1;
      else
         z80_hires_y_reg <= z80_hires_y_reg + 8'd1;
   end
end

always @(posedge clk)
begin
   if(srst)
      z80_hires_options_reg <= 8'hFC;
   else
   begin
      if(io_access & z80_hires_options_sel_out)
         z80_hires_options_reg <= TRS_D;
   end
end


wire [7:0] z80_hires_data_b;

/*
 * True Dual Port RAM, Byte Write Enable, Byte size: 8
 * Port A: Read/Write Width: 8, Write depth: 20480, Operating Mode: Write First,
 *         Core Output Registers, REGCEA Pin
 * Port B: Read/Write Width: 8, Write depth: 20480, Operating Mode: Read First,
 *         Core Output Registers, REGCEB Pin
 * Coe file: splash_hires.mi
 */
blk_mem_gen_4 z80_hires (
   .clka(clk), // input
   .cea(hires_rd_en | hires_wr_en), // input
   .ada({z80_hires_x_reg, z80_hires_y_reg}), // input [14:0]
   .wrea(hires_wr_en), // input
   .dina(TRS_D), // input [7:0]
   .douta(hires_dout), // output [7:0]
   .ocea(hires_rd_regce),
   .reseta(1'b0),
 
   .clkb(vga_clk), // input
   .ceb(vga_act & (vga_xxx == 3'b100)), // input
   .adb({hires_XXXXXXX, hires_YYYYYYYY}), // input [14:0]
   .wreb(1'b0), // input
   .dinb(8'h00), // input [7:0]
   .doutb(z80_hires_data_b), // output [7:0]
   .oceb(vga_act & (vga_xxx == 3'b101)), // input
   .resetb(1'b0)
 );


// Bump the VGA counters.
always @ (posedge vga_clk)
begin
   if(genlock)
   begin
      vga_xxx <= 3'b000;
      vga_XXXXXXX <= 7'd0;
      vga_yyyy_y <= 5'd0;
      vga_YYYYY <= 5'd0;
   end
   else
   begin
   if(vga_xxx == 3'b111)
   begin
      if(vga_XXXXXXX == 7'd99)
      begin
         vga_XXXXXXX <= 7'd0;

         if({vga_YYYYY, vga_yyyy_y} == {5'd20, 5'd20})
         begin
            vga_yyyy_y <= 5'd0;
            vga_YYYYY <= 5'd0;
         end
         else if(vga_yyyy_y == 5'd23)
         begin
            vga_yyyy_y <= 5'd0;
            vga_YYYYY <= vga_YYYYY + 5'd1;
         end
         else
            vga_yyyy_y <= vga_yyyy_y + 5'd1;
      end
      else
         vga_XXXXXXX <= vga_XXXXXXX + 7'd1;
   end
   vga_xxx <= vga_xxx + 3'b1;
   end
end


// Load the hires pixel data into the pixel shift register, or shift current contents.
reg [7:0] hires_pixel_shift_reg;
 
always @ (posedge vga_clk)
begin
   if(vga_xxx == 3'b110)
   begin
      if(hires_act)
         hires_pixel_shift_reg <= z80_hires_data_b;
      else
         hires_pixel_shift_reg <= 8'h00;
   end
   else
      hires_pixel_shift_reg <= {hires_pixel_shift_reg[6:0], 1'b0};
end

reg h_sync, v_sync;

always @ (posedge vga_clk)
begin
   if({vga_XXXXXXX, vga_xxx} == {7'd82, 3'b010})
      h_sync <= 1'b1;
   else if({vga_XXXXXXX, vga_xxx} == {7'd94, 3'b010})
      h_sync <= 1'b0;

   if({vga_YYYYY, vga_yyyy_y} == {5'd20, 5'd9})
      v_sync <= 1'b1;
   else if({vga_YYYYY, vga_yyyy_y} == {5'd20, 5'd11})
      v_sync <= 1'b0;
end


reg vga_vid_out, h_sync_out, v_sync_out;

always @ (posedge vga_clk)
begin
   vga_vid_out <= hires_pixel_shift_reg[7];
   h_sync_out <= h_sync;
   v_sync_out <= v_sync;
end

assign hires_enable = hires_options_graphics_alpha_n;

assign VGA_VID   = vga_vid_out;
assign VGA_HSYNC = h_sync_out;
assign VGA_VSYNC = v_sync_out;

endmodule
