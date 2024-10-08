`timescale 1ns / 1ps

// Implementation of a TRS-80 Model 4 using the T80 core.
//
// Inputs:
//    z80_clk - Z80 clock input.
//    z80_reset_n - Reset input, active low.
//    keyb_matrix - Keyboard input (basically row (address) and column (data)
//                  data from keyboard matrix).
//    vga_clk - VGA clock input, 25.2MHz for 640x480@60Hz display.
//
// Outputs:
//    pixel_data - Single bit (on/off) pixel signal.  If driving the Z (blanking)
//                 input of an oscilloscope this may need to be inverted at the
//                 top level.

module TTRS80 (
   // Inputs
   input z80_clk,
   input z80_reset_n,
   input z80_pause,
   input [7:0] keyb_matrix[0:7],
   input vga_clk,
   input genlock,

   // Display RAM and ROM/RAM interface
   input clk,
   input dsp_ce,
   input rom_ce,
   input ram_ce,
   input chr_ce,
   input [15:0] dsp_rom_ram_addr,
   input dsp_rom_ram_wre,
   input [7:0] dsp_rom_ram_din,
   output [7:0] dsp_rom_ram_dout,

   // Outputs
   output cpu_fast,
   output pixel_data,
   output reg h_sync,
   output reg v_sync,
   output cass_motor_on,
   output [1:0] cass_out,
   output cass_out_sel,
   output is_80col,
   output is_doublwide,
   output is_hires,

   // Cassette
   input cass_in,

   // Expansion connector
   // Inputs
   input xio_int_n,
   input xio_wait_n,
   input xio_sel_n,
   // Outputs
   output xio_mreq_n,
   output xio_rd_n,
   output xio_wr_n,
   output xio_iorq_n,
   output xio_m1_n,
   output xio_in_n,
   output xio_out_n,
   output [15:0] xio_addr,
   output xio_enab,
   // Inputs/Outputs
   input [7:0] xio_data_in,
   output [7:0] xio_data_out
);


wire z80_wait_n;
wire z80_int_n;
wire z80_nmi_n;
wire z80_m1_n;
wire z80_mreq_n;
wire z80_iorq_n;
wire z80_rd_n;
wire z80_wr_n;
wire z80_rfsh_n;
wire z80_halt_n;

wire [15:0] z80_addr;
wire [7:0] z80_data;

wire z80_is_running;

T80a T80a (
   .RESET_n(z80_reset_n),    //: in std_logic;
   .CLK_n(z80_clk),          //: in std_logic;
   .WAIT_n(z80_wait_n),      //: in std_logic;
   .INT_n(z80_int_n),        //: in std_logic;
   .NMI_n(z80_nmi_n),        //: in std_logic;
   .BUSRQ_n(~z80_pause),     //: in std_logic;
   .M1_n(z80_m1_n),          //: out std_logic;
   .MREQ_n(z80_mreq_n),      //: out std_logic;
   .IORQ_n(z80_iorq_n),      //: out std_logic;
   .RD_n(z80_rd_n),          //: out std_logic;
   .WR_n(z80_wr_n),          //: out std_logic;
   .RFSH_n(z80_rfsh_n),      //: out std_logic;
   .HALT_n(z80_halt_n),      //: out std_logic;
   .BUSAK_n(z80_is_running), //: out std_logic;
   .A(z80_addr),             //: out std_logic_vector(15 downto 0);
   .D(z80_data),             //: inout std_logic_vector(7 downto 0)
   .R800_mode(1'b0)
);


// Forward reference
wire [1:0] opreg_sel;

// Generate the memory decodes for the ROM, RAM, display, and keyboard.
wire trs_rom_sel = ~z80_mreq_n & (((opreg_sel == 2'b00) & ((z80_addr[15:13] == 3'b000) | (z80_addr[15:12] == 4'b0010) | (z80_addr[15:11] == 5'b00110)))); // 8k+4k+2k=14k @ 0x0000-0x37ff
wire trs_kbd_sel = ~z80_mreq_n & (((opreg_sel == 2'b00) & ((z80_addr[15:10] == 6'b001110))) | // 1k @ 0x3800-0x3bff
                                  ((opreg_sel == 2'b01) & ((z80_addr[15:10] == 6'b001110))) | // 1k @ 0x3800-0x3bff
                                  ((opreg_sel == 2'b10) & ((z80_addr[15:10] == 6'b111101)))); // 1k @ 0xf400-0xf7ff
wire trs_dsp_sel = ~z80_mreq_n & (((opreg_sel == 2'b00) & ((z80_addr[15:10] == 6'b001111))) | // 1k @ 0x3c00-0x3fff
                                  ((opreg_sel == 2'b01) & ((z80_addr[15:10] == 6'b001111))) | // 1k @ 0x3c00-0x3fff
                                  ((opreg_sel == 2'b10) & ((z80_addr[15:11] == 5'b11111))));  // 2k @ 0xf800-0xffff
wire trs_ram_sel = ~z80_mreq_n & (((opreg_sel == 2'b00) & ((z80_addr[15:14] == 2'b01) | (z80_addr[15] == 1'b1))) | // 48k @ 0x4000-0xffff
                                  ((opreg_sel == 2'b01) & ((z80_addr[15:13] == 3'b000) | (z80_addr[15:12] == 4'b0010) | (z80_addr[15:11] == 5'b00110) |
                                                           (z80_addr[15:14] == 2'b01) | (z80_addr[15] == 1'b1))) | // 62k @ 0x0000-0x37ff, 0x4000-0xffff
                                  ((opreg_sel == 2'b10) & ((z80_addr[15] == 1'b0) | (z80_addr[15:14] == 2'b10) | (z80_addr[15:13] == 3'b110) |
                                                           (z80_addr[15:12] == 4'b1110) | (z80_addr[15:10] == 6'b111100))) | // 61k @ 0x0000-0xf3ff
                                  ((opreg_sel == 2'b11))); // 64k @ 0x0000-0xffff


// Instantiate the ROM.
wire [7:0] trs_rom_data;
wire [7:0] rom_dout;
wire wren_romwe;

blk_mem_gen_0 trs_rom (
   .clka(z80_clk), // input
   .cea(trs_rom_sel & (~z80_rd_n | ~z80_wr_n)), // input
   .ada(z80_addr[13:0]), // input [13:0]
   .wrea(wren_romwe & ~z80_wr_n), // input
   .dina(z80_data), // input
   .douta(trs_rom_data), // output [7:0]
   .ocea(1'b1),
   .reseta(1'b0),

   .clkb(clk), // input
   .ceb(rom_ce), // input
   .adb(dsp_rom_ram_addr[13:0]), // input [13:0]
   .wreb(dsp_rom_ram_wre), // input
   .dinb(dsp_rom_ram_din), // input
   .doutb(rom_dout), // output [7:0]
   .oceb(1'b1),
   .resetb(1'b0)
);


// Forward reference
reg [1:0] bnk_addr; // forward reference

// Instantiate the RAM (64k).
wire [7:0] trs_ram_data;
wire [7:0] ram_dout;

blk_mem_gen_1 trs_ram (
   .clka(z80_clk), // input
   .cea(trs_ram_sel & (~z80_rd_n | ~z80_wr_n)), // input
   .ada({bnk_addr[0], z80_addr[14:0]}), // input [15:0]
   .wrea(~z80_wr_n), // input
   .dina(z80_data), // input [7:0]
   .douta(trs_ram_data), // output [7:0]
   .ocea(1'b1),
   .reseta(1'b0),

   .clkb(clk), // input
   .ceb(ram_ce), // input
   .adb(dsp_rom_ram_addr), // input [15:0]
   .wreb(dsp_rom_ram_wre), // input
   .dinb(dsp_rom_ram_din), // input
   .doutb(ram_dout), // output [7:0]
   .oceb(1'b1),
   .resetb(1'b0)
);



// Forward references
reg vga_80_64_n;
wire opreg_page;

// The VGA display is 640x480.
// The pixel clock is divided by two and each row of the TRS-80 display is repeated two times
// for an effective resolution of 640x240 which is slightly larger than the 512x192 native
// resolution of the M3 display resulting in a small border around the M3 display.
// In 64x16 mode the characters are 8x12 or 8x24 when rows are repeated.
// In 80x24 mode the characters are 8x10 or 8x20 when rows are repeated.
// For convenience the VGA X and Y counters are partitioned into high and low parts which
// count the character position and the position within the character respectively.
reg [2:0] vga_xxx;     // 0-7
reg [6:0] vga_XXXXXXX; // 0-79 active, -99 total
reg [4:0] vga_yyyyy;   // 0-23 in 64x16 mode, 0-19 in 80x24 mode
reg [4:0] vga_YYYYY;   // 0-19 active, -21-20/24 total in 64x16 mode, 0-23 active, -26-4/20 total in 80x24 mode
reg vga_Z;
// VGA in active area.
wire vga_act = vga_80_64_n ?
               ((vga_XXXXXXX < 7'd80) & (vga_YYYYY < 5'd24)) :
               ((vga_XXXXXXX < 7'd80) & (vga_YYYYY < 5'd20));

// Instantiate the display RAM.  The display RAM is dual port.
// The A port is connected to the z80.
// The B port is connected to the video logic.
wire [7:0] trs_dsp_data;
wire [7:0] trs_dsp_data_b;

// Center the 64x16 text display in the 640x480 VGA display.
wire [6:0] dsp_XXXXXXX = vga_XXXXXXX - 7'd8;
wire [4:0] dsp_YYYYY   = vga_YYYYY   - 5'd2;
// Display in active area.
wire dsp_act = vga_80_64_n ?
               ((vga_XXXXXXX < 7'd80) & (vga_YYYYY < 5'd24)) :
               ((dsp_XXXXXXX < 7'd64) & (dsp_YYYYY < 5'd16));
// 64/32 or 80/40 column display mode.
// If modsel=1 then in 32/40 column mode.
// in 32/40 column mode only the even columns are active.
wire mod_modsel; // forward reference
wire col_act = (mod_modsel ? ~dsp_XXXXXXX[0] : 1'b1);

wire        dsp_clka  = z80_is_running ? z80_clk                                                     : clk;
wire        dsp_cea   = z80_is_running ? trs_dsp_sel & (~z80_rd_n | ~z80_wr_n)                       : dsp_ce;
wire [10:0] dsp_ada   = z80_is_running ? {(opreg_sel[1] ? z80_addr[10] : opreg_page), z80_addr[9:0]} : dsp_rom_ram_addr[10:0];
wire        dsp_wrea  = z80_is_running ? ~z80_wr_n                                                   : dsp_rom_ram_wre;
wire [7:0]  dsp_dina  = z80_is_running ? z80_data                                                    : dsp_rom_ram_din;

wire [7:0] dsp_dout = trs_dsp_data;

blk_mem_gen_2 trs_dsp (
   .clka(dsp_clka), // input
   .cea(dsp_cea), // input
   .ada(dsp_ada), // input [10:0]
   .wrea(dsp_wrea), // input
   .dina(dsp_dina), // input [7:0]
   .douta(trs_dsp_data), // output [7:0]
   .ocea(1'b1),
   .reseta(1'b0),

   .clkb(vga_clk), // input
   .ceb(dsp_act & col_act & (vga_xxx == 3'b000)), // input
   .adb(vga_80_64_n ?
        ({vga_YYYYY, 6'b000000} + {2'b00, vga_YYYYY, 4'b0000} + vga_XXXXXXX) : // 80*vga_YYYYY + dsp_XXXXXXX
        {1'b0, dsp_YYYYY[3:0], dsp_XXXXXXX[5:0]}), // input [10:0]
   .wreb(1'b0), // input
   .dinb(8'h00), // input [7:0]
   .doutb(trs_dsp_data_b), // output [7:0]
   .oceb(dsp_act & col_act & (vga_xxx == 3'b001)), // input
   .resetb(1'b0)
);


// Instantiate the hires display RAM.  The hires display RAM is dual port.
// The A port is connected to the z80.
// The B port is connected to the video logic.
wire [7:0] _trs_hires_data;
wire [7:0] trs_hires_data_b;
// The z80 address is supplied by the x and y registers
wire trs_hires_data_sel;
reg [7:0] trs_hires_x_reg, trs_hires_y_reg;

// The hires display is the same as the effective 640x240 VGA display.
wire [6:0] hires_XXXXXXX = vga_XXXXXXX; // 0-79 active
wire [7:0] hires_YYYYYYYY = vga_80_64_n ?
                            ((vga_YYYYY << 3) + (vga_YYYYY << 1) + vga_yyyyy[4:1]) :
                            ((vga_YYYYY << 3) + (vga_YYYYY << 2) + vga_yyyyy[4:1]); // 0-239 active
// Hires in active area.
wire hires_act = vga_act;

blk_mem_gen_4 trs_hires (
   .clka(z80_clk), // input
   .cea(trs_hires_data_sel & (~z80_rd_n | ~z80_wr_n)), // input
   .ada({trs_hires_x_reg[6:0], trs_hires_y_reg}), // input [14:0]
   .wrea(~z80_wr_n), // input
   .dina(z80_data), // input [7:0]
   .douta(_trs_hires_data), // output [7:0]
   .ocea(1'b1),
   .reseta(1'b0),

   .clkb(vga_clk), // input
   .ceb(hires_act & (vga_xxx == 3'b010)), // input
   .adb({hires_XXXXXXX, hires_YYYYYYYY}), // input [14:0]
   .wreb(1'b0), // input
   .dinb(8'h00), // input [7:0]
   .doutb(trs_hires_data_b), // output [7:0]
   .oceb(hires_act & (vga_xxx == 3'b011)), // input
   .resetb(1'b0)
);

reg [7:0] trs_hires_data;

always @ (negedge z80_clk)
begin
   if(trs_hires_data_sel & ~z80_rd_n)
      trs_hires_data <= _trs_hires_data;
end


// keyb_matrix is an external input that contains the complete current keyboard matrix.
reg [7:0] trs_kbd_data;

always @ (posedge z80_clk)
begin
   trs_kbd_data <= ({8{z80_addr[0]}} & keyb_matrix[0]) |
                   ({8{z80_addr[1]}} & keyb_matrix[1]) |
                   ({8{z80_addr[2]}} & keyb_matrix[2]) |
                   ({8{z80_addr[3]}} & keyb_matrix[3]) |
                   ({8{z80_addr[4]}} & keyb_matrix[4]) |
                   ({8{z80_addr[5]}} & keyb_matrix[5]) |
                   ({8{z80_addr[6]}} & keyb_matrix[6]) |
                   ({8{z80_addr[7]}} & keyb_matrix[7]);
end


// Generate the io port decodes.
// Output ports
wire trs_opreg_sel      = ~z80_iorq_n & (z80_addr[7:2] == 6'b100001); // 84-87
wire trs_int_mask_sel   = ~z80_iorq_n & (z80_addr[7:2] == 6'b111000); // e0-e3
wire trs_nmi_mask_sel   = ~z80_iorq_n & (z80_addr[7:2] == 6'b111001); // e4-e7
wire trs_rs232_out_sel  = ~z80_iorq_n & (z80_addr[7:2] == 6'b111010); // e8-eb
wire trs_mod_sel        = ~z80_iorq_n & (z80_addr[7:2] == 6'b111011); // ec-ef
wire trs_disk_out_sel   = ~z80_iorq_n & (z80_addr[7:2] == 6'b111100); // f0-f3
wire trs_drv_sel        = ~z80_iorq_n & (z80_addr[7:2] == 6'b111101); // f4-f7
wire trs_lp_out_sel     = ~z80_iorq_n & (z80_addr[7:2] == 6'b111110); // f8-fb
//wire trs_cass_out_sel   = ~z80_iorq_n & (z80_addr[7:2] == 6'b111111); // fc-ff
wire trs_cass_out_sel   = ~z80_iorq_n & (z80_addr[7:1] == 7'b1111111);// fe-ff
wire trs_wren_out_sel   = ~z80_iorq_n & (z80_addr[7:0] == 8'b11111100); // fc - write enables
// Input ports
wire trs_int_stat_sel   = ~z80_iorq_n & (z80_addr[7:2] == 6'b111000); // e0-e3
wire trs_nmi_stat_sel   = ~z80_iorq_n & (z80_addr[7:2] == 6'b111001); // e4-e7
wire trs_rs232_in_sel   = ~z80_iorq_n & (z80_addr[7:2] == 6'b111010); // e8-eb
wire trs_rtc_sel        = ~z80_iorq_n & (z80_addr[7:2] == 6'b111011); // ec-ef
wire trs_disk_in_sel    = ~z80_iorq_n & (z80_addr[7:2] == 6'b111100); // f0-f3
                                                                      // f4-f7
wire trs_lp_in_sel      = ~z80_iorq_n & (z80_addr[7:2] == 6'b111110); // f8-fb
//wire trs_cass_in_sel    = ~z80_iorq_n & (z80_addr[7:2] == 6'b111111); // fc-ff
wire trs_cass_in_sel    = ~z80_iorq_n & (z80_addr[7:1] == 7'b1111111);// fe-ff

// FDC
wire trs_fdc_cmnd_sel  = trs_disk_out_sel & (z80_addr[1:0] == 2'b00); // f0 output
wire trs_fdc_stat_sel  = trs_disk_in_sel  & (z80_addr[1:0] == 2'b00); // f0 input
wire trs_fdc_track_sel = trs_disk_in_sel  & (z80_addr[1:0] == 2'b01); // f1 input/output
wire [7:0] trs_fdc_stat = 8'hff; // no fdc
//wire [7:0] trs_fdc_stat = 8'h34; // seek error
wire [7:0] trs_fdc_track = 8'h00;

// Hi-res board
wire trs_hires_sel      = ~z80_iorq_n & (z80_addr[7:2] == 6'b100000); // 80-83
assign trs_hires_data_sel = trs_hires_sel & (z80_addr[1:0] == 2'b10); // 82

// External expansion bus
wire trs_xio_sel = (~z80_iorq_n & ((z80_addr[7] == 1'b0) | (z80_addr[7:6] == 2'b10) | (z80_addr[7:5] == 3'b110) | // 00-df
                                   (z80_addr[7:2] == 6'b111110 ) |  // f8-fb printer
                                   (z80_addr[7:1] == 7'b1111110)) ) // fc-fd spi flash
                   & ~trs_hires_sel; // minus 80-83

reg [7:0] trs_opreg_reg;    // 84-87
assign opreg_sel      = trs_opreg_reg[1:0];
wire   opreg_80_64_n  = trs_opreg_reg[2];
wire   opreg_invvide  = trs_opreg_reg[3];
wire [1:0] opreg_mbit = trs_opreg_reg[5:4];
wire   opreg_fxupmem  = trs_opreg_reg[6];
assign opreg_page     = trs_opreg_reg[7];
reg [7:0] trs_int_mask_reg; // e0-e3
reg [7:0] trs_nmi_mask_reg; // e4-e7
reg [7:0] trs_mod_reg;      // ec-ef
assign cass_motor_on  = trs_mod_reg[1];
assign mod_modsel     = trs_mod_reg[2];
wire   mod_enaltset   = trs_mod_reg[3];
wire   mod_enextio    = trs_mod_reg[4];
wire   mod_diswait    = trs_mod_reg[5];
wire   mod_cpufast    = trs_mod_reg[6];
reg [7:0] trs_cass_reg;     // fc-ff
wire cass_casout0     = trs_cass_reg[0];
wire cass_casout1     = trs_cass_reg[1];
reg [7:0] trs_wren_reg; // fc
assign wren_romwe   = (trs_wren_reg[1:0] == 2'b10);

always @ (posedge z80_clk)
begin
   if(~z80_reset_n)
   begin
      trs_opreg_reg <= 8'h00;
      trs_int_mask_reg <= 8'h00;
      trs_nmi_mask_reg <= 8'h00;
      trs_mod_reg <= 8'h00;
      trs_cass_reg <= 8'h00;
      trs_wren_reg <= 8'h00;
   end
   else
   begin
      if(trs_opreg_sel & ~z80_wr_n)
         trs_opreg_reg <= z80_data;

      if(trs_int_mask_sel & ~z80_wr_n)
         trs_int_mask_reg <= z80_data;

      if(trs_nmi_mask_sel & ~z80_wr_n)
         trs_nmi_mask_reg <= z80_data;

      if(trs_mod_sel & ~z80_wr_n)
         trs_mod_reg <= z80_data;

      if(trs_cass_out_sel & ~z80_wr_n)
         trs_cass_reg <= z80_data;

      if(trs_wren_out_sel & ~z80_wr_n)
         trs_wren_reg <= z80_data;
   end
end


reg [7:0] trs_hires_options_reg;
wire hires_options_graphics_alpha_n = trs_hires_options_reg[0];
wire hires_options_unused           = trs_hires_options_reg[1];
wire hires_options_x_dec_inc_n      = trs_hires_options_reg[2];
wire hires_options_y_dec_inc_n      = trs_hires_options_reg[3];
wire hires_options_x_read_clk_n     = trs_hires_options_reg[4];
wire hires_options_y_read_clk_n     = trs_hires_options_reg[5];
wire hires_options_x_write_clk_n    = trs_hires_options_reg[6];
wire hires_options_y_write_clk_n    = trs_hires_options_reg[7];
// The x/y post-modify must be performed after the data operation is complete.
// These registers record the data operation while in progress.
reg hires_data_rd, hires_data_wr;

always @ (posedge z80_clk)
begin
   if(~z80_reset_n)
   begin
      trs_hires_options_reg <= 8'hFC;
   end
   else
   begin
      if(trs_hires_sel)
      begin
         case(z80_addr[1:0])
            2'b00: if(~z80_wr_n) trs_hires_x_reg <= z80_data;
            2'b01: if(~z80_wr_n) trs_hires_y_reg <= z80_data;
            2'b10: begin
                      if(~z80_rd_n) hires_data_rd <= 1'b1;
                      if(~z80_wr_n) hires_data_wr <= 1'b1;
                   end
            2'b11: if(~z80_wr_n) trs_hires_options_reg <= z80_data;
         endcase
      end
      else
      begin
         if((hires_data_rd & ~hires_options_x_read_clk_n) | (hires_data_wr & ~hires_options_x_write_clk_n))
            trs_hires_x_reg <= trs_hires_x_reg + (hires_options_x_dec_inc_n ? -8'b1 : 8'b1);
         if((hires_data_rd & ~hires_options_y_read_clk_n) | (hires_data_wr & ~hires_options_y_write_clk_n))
            trs_hires_y_reg <= trs_hires_y_reg + (hires_options_y_dec_inc_n ? -8'b1 : 8'b1);
         hires_data_rd <= 1'b0;
         hires_data_wr <= 1'b0;
      end
   end
end


wire [7:0] trs_int_stat;
wire [7:0] trs_nmi_stat;
wire [7:0] trs_cass_in = {cass_in,  trs_mod_reg[6:1], 1'b0}; // fc-ff - model 4


// Mux the ROM, RAM, display, keyboard, and io data to the z80 read data.
// Invert the data and the final mux'ed result so that the value for an
// undriven bus is 0xff instead of 0x00.
assign z80_data = ~z80_rd_n ?
                  ~((~trs_rom_data & {8{trs_rom_sel}}) | 
                    (~trs_ram_data & {8{trs_ram_sel}}) |
                    (~trs_dsp_data & {8{trs_dsp_sel}}) |
                    (~trs_kbd_data & {8{trs_kbd_sel}}) |

                    (~trs_hires_data & {8{trs_hires_data_sel}}) |
                    (~xio_data_in    & {8{trs_xio_sel & ~xio_sel_n}}) |
                    (~trs_int_stat   & {8{trs_int_stat_sel  }}) |
                    (~trs_nmi_stat   & {8{trs_nmi_stat_sel  }}) |
                    (~trs_fdc_stat   & {8{trs_fdc_stat_sel  }}) |
                    (~trs_fdc_track  & {8{trs_fdc_track_sel }}) |
                    (~trs_cass_in    & {8{trs_cass_in_sel   }})) :
                  8'bzzzzzzzz;


// Instantiate the character generator ROM.
// The character ROM has a latency of 2 clock cycles.
wire [7:0] char_rom_data;
wire [7:0] chr_dout;

blk_mem_gen_3 char_rom (
   .clka(vga_clk), // input
   .cea(dsp_act & col_act & (vga_yyyyy[4] == 1'b0) & (vga_xxx == 3'b010)), // input
   .ada({trs_dsp_data_b[7] & ~opreg_invvide,
         trs_dsp_data_b[6] & ~(trs_dsp_data_b[7] & mod_enaltset),
         trs_dsp_data_b[5:0], vga_yyyyy[3:1]}), // input [11:0]
   .wrea(1'b0),
   .dina(8'b00000000), // input [7:0]
   .douta(char_rom_data), // output [7:0]
   .ocea(dsp_act & col_act & (vga_yyyyy[4] == 1'b0) & (vga_xxx == 3'b011)),
   .reseta(1'b0),

   .clkb(clk), // input
   .ceb(chr_ce), // input
   .adb(dsp_rom_ram_addr[11:0]), // input [11:0]
   .wreb(dsp_rom_ram_wre),
   .dinb(dsp_rom_ram_din), // input [7:0]
   .doutb(chr_dout), // output [7:0]
   .oceb(1'b1),
   .resetb(1'b0)
);


// Latch the character rom address with the same latency as the rom.
// This is the block graphic.
reg [11:0] char_rom_addr, _char_rom_addr;

always @ (posedge vga_clk)
begin
   if(dsp_act & col_act & (vga_xxx == 3'b010))
      _char_rom_addr <= {trs_dsp_data_b, vga_yyyyy[4:1]};
   if(dsp_act & col_act & (vga_xxx == 3'b011))
      char_rom_addr <= _char_rom_addr;
end


// Bump the VGA counters.
always @ (posedge vga_clk)
begin
   if(genlock)
   begin
      vga_xxx <= 3'b000;
      vga_XXXXXXX <= 7'd0;
      vga_yyyyy <= 5'd0;
      vga_YYYYY <= 5'd0;
      vga_Z <= ~vga_Z;
      vga_80_64_n <= opreg_80_64_n;
   end
   else
   begin
      if(vga_xxx == 3'b111)
      begin
         if(vga_XXXXXXX == 7'd99)
         begin
            vga_XXXXXXX <= 7'd0;

            if({vga_YYYYY, vga_yyyyy} == (vga_80_64_n ? {5'd26, 5'd4} : {5'd21, 5'd20}))
            begin
               vga_yyyyy <= 5'd0;
               vga_YYYYY <= 5'd0;
               vga_Z <= ~vga_Z;
               vga_80_64_n <= opreg_80_64_n;
            end
            else if(vga_yyyyy == (vga_80_64_n ? 5'd19 : 5'd23))
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


// Load the hires pixel data into the pixel shift register, or shift current contents.
reg [7:0] hires_pixel_shift_reg;
 
always @ (posedge vga_clk)
begin
   if(hires_act & (vga_xxx == 3'b100))
      hires_pixel_shift_reg <= trs_hires_data_b;
   else
      hires_pixel_shift_reg <= {hires_pixel_shift_reg[6:0], 1'b0};
end


// Synchronize the RTC divider to the z80 clock.
reg [1:0] rtc_div_dly;
reg rtc_int;

always @ (posedge z80_clk)
begin
   if(trs_rtc_sel & ~z80_rd_n)
      rtc_int <= 1'b0;
   else if(rtc_div_dly == 2'b10)
      rtc_int <= 1'b1;

   rtc_div_dly <= {rtc_div_dly[0], (mod_cpufast ? vga_YYYYY[4] : vga_Z)};
end

// Combine all interrupt sources to the z80.
// The individual interrupts are active high, but in the status register 0 means active.
assign trs_int_stat = ~{4'b0000, ~xio_int_n, rtc_int, 2'b00};
assign trs_nmi_stat = ~{8'b00000000};
// The interrupts are enabled by a 1 in the mask register.
assign z80_int_n = ~|(~trs_int_stat & trs_int_mask_reg);
assign z80_nmi_n = ~|(~trs_nmi_stat & trs_nmi_mask_reg);


//assign pixel_data = hires_options_graphics_alpha_n ? hires_pixel_shift_reg[7] : dsp_pixel_shift_reg[7]; // mux graphics and alpha
assign pixel_data = (hires_options_graphics_alpha_n & hires_pixel_shift_reg[7]) ^ dsp_pixel_shift_reg[7]; // xor graphics and alpha


// The RAM bank address is decoded from fxupmem, mbit1, mbit0, and a15.
// If mbit1=0 then banking is disabled, if mbit1=1 then banking is enabled. 

always @ (*)
begin
   case({opreg_fxupmem, opreg_mbit, z80_addr[15]})
      4'b0000: bnk_addr = 2'b00; // map 0
      4'b0001: bnk_addr = 2'b01; // map 0
      4'b0010: bnk_addr = 2'b00;
      4'b0011: bnk_addr = 2'b01;
      4'b1000: bnk_addr = 2'b00;
      4'b1001: bnk_addr = 2'b01;
      4'b1010: bnk_addr = 2'b00;
      4'b1011: bnk_addr = 2'b01;
      4'b0100: bnk_addr = 2'b00; // map 2
      4'b0101: bnk_addr = 2'b10; // map 2
      4'b0110: bnk_addr = 2'b00; // map 3
      4'b0111: bnk_addr = 2'b11; // map 3
      4'b1100: bnk_addr = 2'b10; // map 6
      4'b1101: bnk_addr = 2'b01; // map 6
      4'b1110: bnk_addr = 2'b11; // map 7
      4'b1111: bnk_addr = 2'b01; // map 7
   endcase
end


// Expansion interface
assign xio_mreq_n  = z80_mreq_n;
assign xio_rd_n    = ~(~z80_mreq_n & ~z80_rd_n);
assign xio_wr_n    = ~(~z80_mreq_n & ~z80_wr_n);
assign xio_iorq_n  = z80_iorq_n;
assign xio_in_n    = ~(~z80_iorq_n & ~z80_rd_n);
assign xio_out_n   = ~(~z80_iorq_n & ~z80_wr_n);
assign xio_addr    = z80_addr;
assign xio_data_out= z80_data;
assign xio_m1_n    = z80_m1_n;
assign xio_enab    = mod_enextio;

assign z80_wait_n = ~(trs_xio_sel & ~xio_wait_n);


assign cass_out = {cass_casout1, cass_casout0};
assign cass_out_sel = trs_cass_out_sel & ~z80_wr_n;
assign cpu_fast = mod_cpufast;

assign is_80col = vga_80_64_n;
assign is_doublwide = mod_modsel;
assign is_hires = hires_options_graphics_alpha_n;


always @ (posedge vga_clk)
begin
   if({vga_XXXXXXX, vga_xxx} == {7'd82, 3'b010})
      h_sync <= 1'b1;
   else if({vga_XXXXXXX, vga_xxx} == {7'd94, 3'b010})
      h_sync <= 1'b0;

   if({vga_YYYYY, vga_yyyyy} == (vga_80_64_n ? {5'd24, 5'd9} : {5'd20, 5'd9}))
      v_sync <= 1'b1;
   else if({vga_YYYYY, vga_yyyyy} == (vga_80_64_n ? {5'd24, 5'd11} : {5'd20, 5'd11}))
      v_sync <= 1'b0;
end


// latch the last accessed memory and gate it to the output

reg dsp_ce_reg;
reg rom_ce_reg;
reg ram_ce_reg;
reg chr_ce_reg;

always @ (posedge clk)
begin
   if(dsp_ce | rom_ce | ram_ce | chr_ce)
   begin
      dsp_ce_reg <= dsp_ce;
      rom_ce_reg <= rom_ce;
      ram_ce_reg <= ram_ce;
      chr_ce_reg <= chr_ce;
   end
end

assign dsp_rom_ram_dout = ({8{dsp_ce_reg}} & dsp_dout) |
                          ({8{rom_ce_reg}} & rom_dout) |
                          ({8{ram_ce_reg}} & ram_dout) |
                          ({8{chr_ce_reg}} & chr_dout);

endmodule
