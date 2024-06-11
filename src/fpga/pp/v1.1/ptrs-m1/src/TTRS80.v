`timescale 1ns / 1ps

// Implementation of a TRS-80 Model 1 using the T80 core.
//
// Inputs:
//    z80_clk - Z80 clock input.
//    z80_reset_n - Reset input, active low.
//    keyb_matrix - Keyboard input (basically row (address) and column (data)
//                  data from keyboard matrix).
//    vga_clk - VGA clock input, 40MHz for 800x600@60Hz display.
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
   input vga_clk_en,
   input genlock,

   // Display RAM and ROM/RAM interface
   input clk,
   input dsp_ce,
   input rom_ce,
   input ram_ce,
   input [15:0] dsp_rom_ram_addr,
   input dsp_rom_ram_wre,
   input [7:0] dsp_rom_ram_din,
   output [7:0] dsp_dout,
   output [7:0] rom_dout,
   output [7:0] ram_dout,

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


// Generate the memory decodes for the ROM, RAM, display, and keyboard.
wire trs_rom_sel = ~z80_mreq_n & ((z80_addr[15:13] == 3'b000) | (z80_addr[15:12] == 4'b0010)); // 8k+4k=12k @ 0x0000-0x2fff
wire trs_kbd_sel = ~z80_mreq_n & ((z80_addr[15:10] == 6'b001110)); // 1k @ 0x3800-0x3bff
wire trs_dsp_sel = ~z80_mreq_n & ((z80_addr[15:10] == 6'b001111)); // 1k @ 0x3c00-0x3fff
wire trs_ram_sel = ~z80_mreq_n & ((z80_addr[15:14] == 2'b01) | (z80_addr[15] == 1'b1)); // 48k @ 0x4000-0xffff


// Instantiate the ROM.
wire [7:0] trs_rom_data;

blk_mem_gen_0 trs_rom (
   .clka(z80_clk), // input
   .cea(trs_rom_sel & ~z80_rd_n), // input
   .ada(z80_addr[13:0]), // input [13:0]
   .wrea(1'b0), // input
   .dina(8'b00000000), // input
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


// Instantiate the RAM (48k).
wire [7:0] trs_ram_data;

blk_mem_gen_1 trs_ram (
   .clka(z80_clk), // input
   .cea(trs_ram_sel & (~z80_rd_n | ~z80_wr_n)), // input
   .ada(z80_addr - 16'h4000), // input [15:0]
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


// The VGA display is 800x600.
// The pixel clock is divided by two and each row of the TRS-80 display is repeated three times
// for an effective resolution of 400x200 which slightly larger than the 384x192 native
// resolution of the M1 display resulting in a small border around the M1 display.
// In 64x16 mode the characters are 6x12 or 6x36 when rows are repeated.
// For convenience the VGA X and Y counters are partitioned into high and low parts which
// count the character position and the position within the character respectively.
reg [2:0] vga_xxx;     // 0-5
reg [6:0] vga_XXXXXXX; // 0-66-2/6 active, -87 total
reg [5:0] vga_yyyy_yy; // vga_yyyy_yy[5:2] = 0-11, vga_yyyy_yy[1:0]=0-2 in 64x16 mode
reg [4:0] vga_YYYYY;   // 0-16-23/36 active, -17-15/36 total in 64x16 mode
// VGA in active area.
wire vga_act = ( (vga_XXXXXXX < 7'd67)
               & ((vga_YYYYY < 5'd16) | ((vga_YYYYY == 5'd16) & (vga_yyyy_yy[5:2] < 4'd8))));

// Instantiate the display RAM.  The display RAM is dual port.
// The A port is connected to the z80.
// The B port is connected to the video logic.
wire [7:0] trs_dsp_data;
wire [7:0] trs_dsp_data_b;

// Center the 64x16 text/le18 display in the 800x600 VGA display.
//wire [6:0] dsp_XXXXXXX = vga_XXXXXXX - 7'd1;
wire [6:0] dsp_XXXXXXX = ((|vga_xxx[2:1]) ? (vga_XXXXXXX - 7'd0) : (vga_XXXXXXX - 7'd1));
wire [2:0] dsp_xxx     = ((|vga_xxx[2:1]) ? (vga_xxx     - 3'd2) : (vga_xxx     + 3'd4));
//wire [4:0] dsp_YYYYY   = vga_YYYYY   - 5'd0;
wire [4:0] dsp_YYYYY   =  ((|vga_yyyy_yy[5:4]) ? (vga_YYYYY        - 5'd0) : (vga_YYYYY        - 5'd1));
wire [5:0] dsp_yyyy_yy = {((|vga_yyyy_yy[5:4]) ? (vga_yyyy_yy[5:2] - 4'd4) : (vga_yyyy_yy[5:2] + 4'd8)), vga_yyyy_yy[1:0]};
// Display in active area.
wire dsp_act = ((dsp_XXXXXXX < 7'd64) & (dsp_YYYYY < 5'd16));
// 64/32 column display mode.
// If modsel=1 then in 32 column mode.
// in 32 column mode only the even columns are active.
wire cass_modsel; // forward reference
wire col_act = (cass_modsel ? ~dsp_XXXXXXX[0] : 1'b1);

wire        dsp_clka  = z80_is_running ? z80_clk                               : clk;
wire        dsp_cea   = z80_is_running ? trs_dsp_sel & (~z80_rd_n | ~z80_wr_n) : dsp_ce;
wire [9:0]  dsp_ada   = z80_is_running ? z80_addr[9:0]                         : dsp_rom_ram_addr[9:0];
wire        dsp_wrea  = z80_is_running ? ~z80_wr_n                             : dsp_rom_ram_wre;
wire [7:0]  dsp_dina  = z80_is_running ? z80_data                              : dsp_rom_ram_din;

assign dsp_dout = trs_dsp_data;

blk_mem_gen_2 trs_dsp (
   .clka(dsp_clka), // input
   .cea(dsp_cea), // input
   .ada(dsp_ada), // input [9:0]
   .wrea(dsp_wrea), // input
   .dina(dsp_dina), // input [7:0]
   .douta(trs_dsp_data), // output [7:0]
   .ocea(1'b1),
   .reseta(1'b0),

   .clkb(vga_clk), // input
   .ceb(dsp_act & col_act & (dsp_xxx == 3'b000)), // input
   .adb({dsp_YYYYY[3:0], dsp_XXXXXXX[5:0]}), // input [9:0]
   .wreb(1'b0), // input
   .dinb(8'h00), // input [7:0]
   .doutb(trs_dsp_data_b), // output [7:0]
   .oceb(dsp_act & col_act & (dsp_xxx == 3'b001)), // input
   .resetb(1'b0)
);


// Disable the splash screen and border on first write to text display.
reg splash_en = 1'b1;

always @(posedge z80_clk)
begin
   if(trs_dsp_sel & ~z80_wr_n)
      splash_en <= 1'b0;
end


// Instantiate the le18 display RAM.  The le18 display RAM is dual port.
// The A port is connected to the z80.
// The B port is connected to the video logic.
wire [5:0] _trs_le18_data;
wire [5:0] trs_le18_data_b;
// The z80 address is supplied by the x and y registers
wire trs_le18_data_sel;
reg [5:0] trs_le18_x_reg;
reg [7:0] trs_le18_y_reg;
reg [0:0] trs_le18_options_reg;
wire le18_options_enable = trs_le18_options_reg[0];

// The le18 display is the same as the effective 384x192 text display.
wire [6:0] le18_XXXXXXX = dsp_XXXXXXX; // 0-63 active
wire [7:0] le18_YYYYYYYY = (dsp_YYYYY << 3) + (dsp_YYYYY << 2) + dsp_yyyy_yy[5:2]; // 0-191 active
// Le18 in active area.
wire le18_act = dsp_act;

blk_mem_gen_4 trs_le18 (
   .clka(z80_clk), // input
   .cea(trs_le18_data_sel & (~z80_rd_n | ~z80_wr_n)), // input
   .ada({trs_le18_y_reg, trs_le18_x_reg}), // input [13:0]
   .wrea(~z80_wr_n), // input
   .dina(z80_data[5:0]), // input [5:0]
   .douta(_trs_le18_data), // output [5:0]
   .ocea(1'b1),
   .reseta(1'b0),

   .clkb(vga_clk), // input
   .ceb(dsp_act & (dsp_xxx == 3'b010)), // input
   .adb({le18_YYYYYYYY, le18_XXXXXXX[5:0]}), // input [13:0]
   .wreb(1'b0), // input
   .dinb(6'h00), // input [5:0]
   .doutb(trs_le18_data_b), // output [5:0]
   .oceb(dsp_act & (dsp_xxx == 3'b011)), // input
   .resetb(1'b0)
);

reg [5:0] trs_le18_data;

always @ (negedge z80_clk)
begin
   if(trs_le18_data_sel & ~z80_rd_n)
      trs_le18_data <= _trs_le18_data;
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
wire trs_rs232_out_sel  = ~z80_iorq_n & (z80_addr[7:2] == 6'b111010); // e8-eb
wire trs_drv_sel        = ~z80_mreq_n & (z80_addr[15:2] == 14'b00110111111000); // 37e0-37e3
wire trs_lp_out_sel     = ~z80_mreq_n & (z80_addr[15:2] == 14'b00110111111010); // 37e8-37eb
wire trs_disk_out_sel   = ~z80_mreq_n & (z80_addr[15:2] == 14'b00110111111011); // 37ec-37ef
//wire trs_cass_out_sel   = ~z80_iorq_n & (z80_addr[7:2] == 6'b111111); // fc-ff
wire trs_cass_out_sel   = ~z80_iorq_n & (z80_addr[7:1] == 7'b1111111);// fe-ff
// Input ports
wire trs_int_stat_sel   = ~z80_mreq_n & (z80_addr[15:2] == 14'b00110111111000); // 37e0-37e3
wire trs_rs232_in_sel   = ~z80_iorq_n & (z80_addr[7:2] == 6'b111010); // e8-eb
wire trs_rtc_sel        = ~z80_iorq_n & (z80_addr[7:2] == 6'b111011); // ec-ef
wire trs_lp_in_sel      = ~z80_mreq_n & (z80_addr[15:2] == 14'b00110111111010); // 37e8-37eb
wire trs_disk_in_sel    = ~z80_mreq_n & (z80_addr[15:2] == 14'b00110111111011); // 37ec-37ef

// FDC
wire trs_fdc_cmnd_sel  = trs_disk_out_sel & (z80_addr[1:0] == 2'b00); // 37ec output
wire trs_fdc_stat_sel  = trs_disk_in_sel  & (z80_addr[1:0] == 2'b00); // 37ec input
wire trs_fdc_track_sel = trs_disk_in_sel & (z80_addr[1:0] == 2'b01); // 37ed input/output
//wire [7:0] trs_fdc_stat = 8'hff; // no fdc
wire [7:0] trs_fdc_stat = 8'h34; // seek error
wire [7:0] trs_fdc_track = 8'h00;
wire fdc_int = 1'b0;

// LE18 graphics board
assign trs_le18_data_sel  = ~z80_iorq_n & (z80_addr[7:0] == 8'hEC); // ec
wire trs_le18_x_sel       = ~z80_iorq_n & (z80_addr[7:0] == 8'hED); // ed
wire trs_le18_y_sel       = ~z80_iorq_n & (z80_addr[7:0] == 8'hEE); // ee
wire trs_le18_options_sel = ~z80_iorq_n & (z80_addr[7:0] == 8'hEF); // ef

// External expansion bus
wire trs_xio_sel = (~z80_iorq_n & ((z80_addr[7] == 1'b0) | (z80_addr[7:6] == 2'b10) | (z80_addr[7:5] == 3'b110) | // 00-df
                                   (z80_addr[7:1] == 7'b1111110)) | // fc-fd spi flash
                    ~z80_mreq_n & ((z80_addr[15:2] == (16'h37E8 >> 2))) ); // 37e8-37eb printer

reg [7:0] trs_cass_reg;     // fc-ff
wire   cass_casout0    = trs_cass_reg[0];
wire   cass_casout1    = trs_cass_reg[1];
wire   cass_casmotoron = trs_cass_reg[2];
assign cass_modsel     = trs_cass_reg[3];

always @ (posedge z80_clk)
begin
   if(~z80_reset_n)
   begin
      trs_cass_reg <= 8'h00;
   end
   else
   begin
      if(trs_cass_out_sel & ~z80_wr_n)
         trs_cass_reg <= z80_data;
   end
end


always @ (posedge z80_clk)
begin
   if(~z80_reset_n)
   begin
      trs_le18_options_reg <= 1'b0;
   end
   else
   begin
      if(trs_le18_x_sel & ~z80_wr_n)
         trs_le18_x_reg <= z80_data[5:0];

      if(trs_le18_y_sel & ~z80_wr_n)
         trs_le18_y_reg <= z80_data;

      if(trs_le18_options_sel & ~z80_wr_n)// & ~ splash_en)
         trs_le18_options_reg <= z80_data[0];
   end
end


wire [7:0] trs_int_stat;


// Mux the ROM, RAM, display, keyboard, and io data to the z80 read data.
// Invert the data and the final mux'ed result so that the value for an
// undriven bus is 0xff instead of 0x00.
assign z80_data = ~z80_rd_n ?
                  ~((~trs_rom_data & {8{trs_rom_sel}}) | 
                    (~trs_ram_data & {8{trs_ram_sel}}) |
                    (~trs_dsp_data & {8{trs_dsp_sel}}) |
                    (~trs_kbd_data & {8{trs_kbd_sel}}) |

                    (~trs_le18_data  & {8{trs_le18_data_sel }}) |
                    (~xio_data_in    & {8{trs_xio_sel & ~xio_sel_n}}) |
                    (~trs_int_stat   & {8{trs_int_stat_sel  }}) |
                    (~trs_fdc_stat   & {8{trs_fdc_stat_sel  }}) |
                    (~trs_fdc_track  & {8{trs_fdc_track_sel }}) ) :
                  8'bzzzzzzzz;


// Instantiate the character generator ROM.
// The character ROM has a latency of 2 clock cycles.
wire [5:0] char_rom_data;

blk_mem_gen_3 char_rom (
   .clk(vga_clk), // input
   .ad({trs_dsp_data_b[6:0], dsp_yyyy_yy[4:2]}), // input [9:0]
   .dout(char_rom_data), // output [5:0]
   .ce(dsp_act & col_act & (dsp_yyyy_yy[5] == 1'b0) & (dsp_xxx == 3'b010)),
   .oce(dsp_act & col_act & (dsp_yyyy_yy[5] == 1'b0) & (dsp_xxx == 3'b011)),
   .reset(1'b0)
);


// Latch the character rom address with the same latency as the rom.
// This is the block graphic.
reg [11:0] char_rom_addr, _char_rom_addr;

always @ (posedge vga_clk)
begin
   if(vga_clk_en)
   begin
      if(dsp_act & col_act & (dsp_xxx == 3'b010))
         _char_rom_addr <= {trs_dsp_data_b, dsp_yyyy_yy[5:2]};
      if(dsp_act & col_act & (dsp_xxx == 3'b011))
         char_rom_addr <= _char_rom_addr;
   end
end


// Bump the VGA counters.
always @ (posedge vga_clk)
begin
   if(vga_clk_en)
   begin
      if(genlock)
      begin
         vga_xxx <= 3'b000;
         vga_XXXXXXX <= 7'd0;
         vga_yyyy_yy <= {4'd0, 2'b00};
         vga_YYYYY <= 5'd0;
      end
      else
      if(vga_xxx == 3'b101)
      begin
         vga_xxx <= 3'b000;
         if(vga_XXXXXXX == 7'd87)
         begin
            vga_XXXXXXX <= 7'd0;

            if({vga_YYYYY, vga_yyyy_yy} == {5'd17, {4'd5, 2'b00}})
            begin
               vga_yyyy_yy <= {4'd0, 2'b00};
               vga_YYYYY <= 5'd0;
            end
            else if(vga_yyyy_yy[1:0] == 2'b10)
            begin
               vga_yyyy_yy[1:0] = 2'b00;
               if(vga_yyyy_yy[5:2] == 5'd11)
               begin
                  vga_yyyy_yy[5:2] <= 4'd0;
                  vga_YYYYY <= vga_YYYYY + 5'd1;
               end
               else
                  vga_yyyy_yy[5:2] <= vga_yyyy_yy[5:2] + 4'd1;
            end
            else
               vga_yyyy_yy[1:0] <= vga_yyyy_yy[1:0] + 2'b1;
         end
         else
            vga_XXXXXXX <= vga_XXXXXXX + 7'd1;
      end
      else
         vga_xxx <= vga_xxx + 3'b1;
   end
end


// Load the display pixel data into the pixel shift register, or shift current contents.
reg [5:0] dsp_pixel_shift_reg;

always @ (posedge vga_clk)
begin
   if(vga_clk_en)
   begin
      // If the msb is 1 then it's block graphic.
      // Otherwise it's character data from the character rom.
      if(dsp_act & col_act & (dsp_xxx == 3'b100))
      begin
         if(char_rom_addr[11] == 1'b0)
            dsp_pixel_shift_reg <= (char_rom_addr[3] ? 6'h00 : char_rom_data);
         else
         begin
            // The character is 12 rows.
            case(char_rom_addr[3:2])
               2'b00: dsp_pixel_shift_reg <= {{3{char_rom_addr[4]}}, {3{char_rom_addr[5]}}};
               2'b01: dsp_pixel_shift_reg <= {{3{char_rom_addr[6]}}, {3{char_rom_addr[7]}}};
               2'b10: dsp_pixel_shift_reg <= {{3{char_rom_addr[8]}}, {3{char_rom_addr[9]}}};
               2'b11: dsp_pixel_shift_reg <= 6'h00; // should never happen
            endcase
         end
      end
      else
      begin
         // If 32 column mode then shift only every other clock.
         // Note the vga_xxx[0] value here (0 or 1) must be the same as the lsb used above
         // so that the load cycle would also be a shift cycle.
         if(cass_modsel ? (vga_xxx[0] == 1'b0) : 1'b1)
            dsp_pixel_shift_reg <= {dsp_pixel_shift_reg[4:0], 1'b0};
      end
   end
end


// Load the le18 pixel data into the pixel shift register, or shift current contents.
reg [5:0] le18_pixel_shift_reg;
 
always @ (posedge vga_clk)
begin
   if(vga_clk_en)
   begin
      if(le18_act & (dsp_xxx == 3'b100))
         // For le18 the leftmost bit is bit 0 so load the shift register in bit reversed order.
         le18_pixel_shift_reg <= {trs_le18_data_b[0], trs_le18_data_b[1], trs_le18_data_b[2],
                                  trs_le18_data_b[3], trs_le18_data_b[4], trs_le18_data_b[5]};
      else
         le18_pixel_shift_reg <= {le18_pixel_shift_reg[4:0], 1'b0};
   end
end


// Divide 40MHz vga clock to 40Hz RTC clock.
reg [18:0] rtc_div;
wire rtc_clk = ~rtc_div[18];

always @ (posedge vga_clk)
begin
   if(vga_clk_en)
      rtc_div <= (rtc_div == 19'd249999) ? -19'd250000 : (rtc_div + 19'd1);
end

// Synchronize the RTC divider to the z80 clock.
reg [1:0] rtc_div_dly;
reg rtc_int;
// The interrupt clear is performed after the read operation is complete.
// This register records the read operation while in progress.
reg rtc_int_rd;

always @ (posedge z80_clk)
begin
   if(trs_int_stat_sel & ~z80_rd_n)
      rtc_int_rd <= 1'b1;
   else
   begin
      if(rtc_div_dly == 2'b01)
         rtc_int <= 1'b1;
      else if(rtc_int_rd)
         rtc_int <= 1'b0;
      rtc_int_rd <= 1'b0;
   end 

   rtc_div_dly <= {rtc_div_dly[0], rtc_clk};
end

// Combine all interrupt sources to the z80.
// The individual interrupts are active high, and in the status register 1 means active.
assign trs_int_stat = {rtc_int, fdc_int, 6'b000000};

assign z80_int_n = ~(rtc_int | fdc_int | ~xio_int_n);
assign z80_nmi_n = 1'b1;


//assign pixel_data = le18_options_enable ? le18_pixel_shift_reg[5] : dsp_pixel_shift_reg[5]; // mux graphics and alpha
assign pixel_data = ((le18_options_enable | splash_en) & le18_pixel_shift_reg[5]) ^ dsp_pixel_shift_reg[5]; // xor graphics and alpha


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
assign xio_enab    = 1'b1;

assign z80_wait_n = ~(trs_xio_sel & ~xio_wait_n);


assign cass_out = {cass_casout1, cass_casout0};
assign cass_out_sel = trs_cass_out_sel & ~z80_wr_n;
assign cpu_fast = 1'b0;

assign is_80col = 1'b0;
assign is_doublwide = cass_modsel;
assign is_hires = le18_options_enable;


always @ (posedge vga_clk)
begin
   if(vga_clk_en)
   begin
      if({vga_XXXXXXX, vga_xxx} == {7'd69, 3'b101})
         h_sync <= 1'b1;
      else if({vga_XXXXXXX, vga_xxx} == {7'd80, 3'b011})
         h_sync <= 1'b0;

      if({vga_YYYYY, vga_yyyy_yy} == {5'd16, {4'd8, 2'b00}})
         v_sync <= 1'b1;
      else if({vga_YYYYY, vga_yyyy_yy} == {5'd16, {4'd9, 2'b01}})
         v_sync <= 1'b0;
   end
end

endmodule
