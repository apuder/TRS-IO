`timescale 1ns / 1ps

// This module was contributed by Mathew Boytim <maboytim@yahoo.com>

module vga(
  input clk,     // 100 MHz
  input vga_clk, // 12 MHz
  input [15:0] TRS_A,
  input [7:0] TRS_D,
  input WR_falling_edge,
  input z80_dsp_sel,
  output VGA_RGB,
  output VGA_HSYNC,
  output VGA_VSYNC);

// Forward references
//reg vga_80_64_n;
reg opreg_page;
//reg opreg_80_64_n;

// The VGA display is 640x480.
// Each row of the TRS-80 display is repeated twice for an effective resolution of 640x240.
// In 64x16 mode the characters are 8x12 or 8x24 when rows are repeated.
// In 80x24 mode the characters are 8x10 or 8x20 when rows are repeated.
// For convenience the VGA X and Y counters are partitioned into high and low parts which
// count the character position and the position within the charater resepctively.
reg [2:0] vga_xxx;     // 0-7
reg [6:0] vga_XXXXXXX; // 0-79 active, -99 total
reg [4:0] vga_yyyyy;   // 0-23 in 64x16 mode, 0-19 in 80x24 mode
reg [4:0] vga_YYYYY;   // 0-19 active, -21-20/24 total in 64x16 mode, 0-23 active, -26-4/20 totalin 80x24 mode
//reg vga_Z;
// VGA in active area.
wire vga_act = ((vga_XXXXXXX < 7'd80) & (vga_YYYYY < 5'd20));


// Instantiate the display RAM.  The display RAM is dual port.
// The A port is connected to the z80.
// The B port is connected to the video logic.
wire [7:0] _z80_dsp_data;
wire [7:0] z80_dsp_data_b;
wire [7:0] z80_dsp_data_raw;

// Center the 64x16 text display in the 640x480 VGA display.
wire [6:0] dsp_XXXXXXX = vga_XXXXXXX - 7'd8;
wire [4:0] dsp_YYYYY   = vga_YYYYY   - 5'd2;
// Display in active area.
wire dsp_act = ((dsp_XXXXXXX < 7'd64) & (dsp_YYYYY < 5'd16));
// 64/32 or 80/40 column display mode.
// If modsel=1 then in 32/40 column mode.
// in 32/40 column mode only the even columns are active.
reg mod_modsel = 0; // forward reference
wire col_act = (mod_modsel ? ~dsp_XXXXXXX[0] : 1'b1);

/*
 * True Dual Port RAM, Byte Write Enable, Byte size: 8
 * Port A: Read/Write Width: 8, Write depth: 1024, Operating Mode: Write First,
 *         Core Output Registers, REGCEA Pin
 * Port B: Read/Write Width: 8, Write depth: 1024, Operating Mode: Read First,
 *         Core Output Registers, REGCEB Pin
 */
blk_mem_gen_2 z80_dsp (
   .clka(clk), // input
   .ena(WR_falling_edge && z80_dsp_sel), // input
   .addra(TRS_A[9:0]), // input [9:0]
   .wea(1'b1), // input
   .dina(TRS_D), // input [7:0]
   .douta(), // output [7:0]
   .regcea(),

   .clkb(vga_clk), // input
   .enb(dsp_act & col_act & (vga_xxx == 3'b000)), // input
   .addrb({dsp_YYYYY[3:0], dsp_XXXXXXX[5:0]}), // input [10:0]
   .web(1'b0), // input
   .dinb(8'h00), // input [7:0]
   .doutb(z80_dsp_data_raw), // output [7:0]
   .regceb(dsp_act & col_act & (vga_xxx == 3'b001)) // input
);

assign z80_dsp_data_b = ((z80_dsp_data_raw & 8'b11100000) ? z80_dsp_data_raw : (z80_dsp_data_raw | 64));

// Instantiate the character generator ROM.
// The character ROM has a latency of 2 clock cycles.
wire [7:0] char_rom_data;

reg opreg_invvide = 1'b0;
reg mod_enaltset = 1'b0;

/*
 * Single Port ROM
 * Port A: Width: 8, Depth: 2048, Primitives Output Register, REGCEA Pin
 *         Load Init File: trs80m3_chr.coe
 */
blk_mem_gen_3 char_rom (
   .clka(vga_clk), // input
   .addra({z80_dsp_data_b[7] & ~opreg_invvide,
           z80_dsp_data_b[6] & ~(z80_dsp_data_b[7] & mod_enaltset),
           z80_dsp_data_b[5:0], vga_yyyyy[3:1]}), // input [11:0]
   .douta(char_rom_data), // output [7:0]
   .ena(dsp_act & col_act & (vga_yyyyy[4] == 1'b0) & (vga_xxx == 3'b010)),
   .regcea(dsp_act & col_act & (vga_yyyyy[4] == 1'b0) & (vga_xxx == 3'b011))
);

// Latch the character rom address with the same latency as the rom.
// This is the block graphic.
reg [11:0] char_rom_addr, _char_rom_addr;

always @ (posedge vga_clk)
begin
   if(dsp_act & col_act & (vga_xxx == 3'b010))
      _char_rom_addr <= {z80_dsp_data_b, vga_yyyyy[4:1]};
   if(dsp_act & col_act & (vga_xxx == 3'b011))
      char_rom_addr <= _char_rom_addr;
end


// Bump the VGA counters.
always @ (posedge vga_clk)
begin
   if(vga_xxx == 3'b111)
   begin
      if(vga_XXXXXXX == 7'd99)
      begin
         vga_XXXXXXX <= 7'd0;

         if({vga_YYYYY, vga_yyyyy} == {5'd21, 5'd20})
         begin
            vga_yyyyy <= 5'd0;
            vga_YYYYY <= 5'd0;
            //vga_Z <= ~vga_Z;
            //vga_80_64_n <= opreg_80_64_n;
         end
         else if(vga_yyyyy == 5'd23)
         begin
            vga_yyyyy <= 5'd0;
            vga_YYYYY <= vga_YYYYY + 5'd1;
         end
         else
            vga_yyyyy <= vga_yyyyy + 5'd1;
      end
      else
         vga_XXXXXXX <= vga_XXXXXXX + 7'd1;
   end
   vga_xxx <= vga_xxx + 3'b1;
end


// Load the display pixel data into the pixel shift register, or shift current contents.
reg [7:0] dsp_pixel_shift_reg;

always @ (posedge vga_clk)
begin
   // If the msb's are 10 and not inverse video then it's block graphic.
   // Otherwise it's character data from the character rom.
   if(dsp_act & col_act & (vga_xxx == 3'b100))
   begin
      if(~((char_rom_addr[11:10] == 2'b10) & ~opreg_invvide))
         dsp_pixel_shift_reg <= (char_rom_addr[3] ? 8'h00 : char_rom_data) ^ {8{char_rom_addr[11] & opreg_invvide}};
      else
      begin
         // The character is 12 rows.
         case(char_rom_addr[3:2])
            2'b00: dsp_pixel_shift_reg <= {{4{char_rom_addr[4]}}, {4{char_rom_addr[5]}}};
            2'b01: dsp_pixel_shift_reg <= {{4{char_rom_addr[6]}}, {4{char_rom_addr[7]}}};
            2'b10: dsp_pixel_shift_reg <= {{4{char_rom_addr[8]}}, {4{char_rom_addr[9]}}};
            2'b11: dsp_pixel_shift_reg <= 8'h00; // should never happen
         endcase
      end
   end
   else
   begin
      // If 32 column mode then shift only every other clock.
      // Note the vga_xxx[0] value here (0 or 1) must be the same as the lsb used above
      // so that the load cycle would also be a shift cycle.
      if(mod_modsel ? (vga_xxx[0] == 1'b0) : 1'b1)
         dsp_pixel_shift_reg <= {dsp_pixel_shift_reg[6:0], 1'b0};
   end
end

assign VGA_RGB = dsp_pixel_shift_reg[7];

reg h_sync, v_sync;

always @ (posedge vga_clk)
begin
   if({vga_XXXXXXX, vga_xxx} == {7'd82, 3'b010})
      h_sync <= 1'b1;
   else if({vga_XXXXXXX, vga_xxx} == {7'd94, 3'b010})
      h_sync <= 1'b0;

   if({vga_YYYYY, vga_yyyyy} == {5'd20, 5'd9})
      v_sync <= 1'b1;
   else if({vga_YYYYY, vga_yyyyy} == {5'd20, 5'd11})
      v_sync <= 1'b0;
end

assign VGA_HSYNC = h_sync;
assign VGA_VSYNC = v_sync;

endmodule
