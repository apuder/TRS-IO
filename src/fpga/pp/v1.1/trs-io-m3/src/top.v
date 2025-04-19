`timescale 1ns / 1ps

module top(
  input clk_in,
  input _RESET_N,

  output CTRL_DIR,
  output CTRL_EN,
  output CTRL1_EN,
  input _RD_N,
  input _WR_N,
  input _IN_N,
  input _OUT_N,
  input _RAS_N,
  input _IOREQ_N,
  input _M1_N,
  output ABUS_DIR_N,
  output ABUS_DIR,
  output ABUS_EN,
  input [15:0] _A,
  output DBUS_DIR,
  output DBUS_EN,
  inout [7:0] _D,

  input CS_FPGA,
  input SCK,
  input MOSI,
  output MISO,
  output [3:0] ESP_S,
  output ESP_REQ,
  input ESP_DONE,

  output INT,
  input INT_IN_N,
  output WAIT,
  input WAIT_IN_N,
  output EXTIOSEL,
  input EXTIOSEL_IN_N,
  output CASS_OUT_L,
  output CASS_OUT_R,

  input UART_RX,
  output UART_TX,

  input [3:0] CONF,
  output [3:0] LED,
  output reg LED_GREEN,
  output reg LED_RED,
  output reg LED_BLUE,
  inout [7:0] PMOD,

  // HDMI
  output [2:0] HDMI_TX_P,
  output [2:0] HDMI_TX_N,
  output HDMI_TXC_P,
  output HDMI_TXC_N,

  // Configuration Flash
  output FLASH_SPI_CS_N,
  output FLASH_SPI_CLK,
  output FLASH_SPI_SI,
  input FLASH_SPI_SO
);

//-------Mode--------------------------------------------------------------------

localparam [3:0]
  mode_trs_io_m1   = 4'b0000,
  mode_trs_io_m3   = 4'b1000,
  mode_ptrs_m1     = 4'b0001,
  mode_ptrs_m3     = 4'b0011,
  mode_ptrs_m4     = 4'b0100,
  mode_ptrs_m4p    = 4'b0101;

localparam add_dip_4 = 1'b0;
wire [3:0] this_mode = mode_trs_io_m3;


wire[7:0] TRS_A = _A[7:0];
wire[7:0] TRS_D = _D;

wire TRS_OE;
assign DBUS_EN = TRS_OE;
wire TRS_DIR;
assign DBUS_DIR = TRS_DIR;

wire CS = CS_FPGA;

assign ABUS_DIR = 1'b1;
assign ABUS_DIR_N = ~ABUS_DIR;
assign ABUS_EN = 1'b0;

assign CTRL_DIR = 1'b1;
assign CTRL_EN = 1'b0;
assign CTRL1_EN = 1'b1;


localparam [2:0] VERSION_MAJOR = 3'd0;
localparam [4:0] VERSION_MINOR = 5'd3;

localparam [7:0] COOKIE = 8'hAF;

wire clk;

/*
 * Clocking Wizard
 * Clock primary: 27 MHz
 * clk_out1 frequency: 84 MHz
 */

Gowin_rPLL clk_wiz_0(
   .clkout(clk), //output
   .clkin(clk_in) //input
);


reg rst = 1'b0;

always @ (posedge clk)
begin
   rst <= ~_RESET_N;
end


//----Address Decoder------------------------------------------------------------

wire TRS_RD = 1'b1;
wire TRS_WR = 1'b1;

wire TRS_IN  = _IN_N  | _IOREQ_N;
wire TRS_OUT = _OUT_N | _IOREQ_N;

wire io_access_raw = ~TRS_RD | ~TRS_WR | ~TRS_IN | ~TRS_OUT;

wire io_access;

filter io(
  .clk(clk),
  .in(io_access_raw),
  .out(),
  .rising_edge(io_access),
  .falling_edge()
);

//----TRS-IO---------------------------------------------------------------------

// Virtual printer enable flag
reg vprinter_en = 1'b1;

// m3: printer @ F8h-F9h (io)
wire printer_sel_m3 = vprinter_en & (TRS_A[7:2]  == (8'hF8 >> 2));
wire printer_sel_m3_in  = printer_sel_m3 & ~TRS_IN;
wire printer_sel_m3_out = printer_sel_m3 & ~TRS_OUT;
wire printer_sel_rd = printer_sel_m3_in;
wire printer_sel_wr = printer_sel_m3_out;

// trs-io @ 1Fh
wire trs_io_sel_in  = (TRS_A == 8'd31) & ~TRS_IN;
wire trs_io_sel_out = (TRS_A == 8'd31) & ~TRS_OUT;
wire trs_io_sel = trs_io_sel_in | trs_io_sel_out;

// frehd @ C0h-CFh
wire frehd_sel_in  = (TRS_A[7:4] == 4'hC) & ~TRS_IN;
wire frehd_sel_out = (TRS_A[7:4] == 4'hC) & ~TRS_OUT;

// cassette @ FFh
wire cass_sel_out = (TRS_A == 8'hFF) & ~TRS_OUT;

// m3: hires graphics @ 80h-83h
wire hires_sel_in = (TRS_A[7:2] == (8'h80 >> 2)) & ~TRS_IN;

// m3/4: orchestra-90 @ 79h,75h
wire orch90l_sel_out = (TRS_A == 8'h75) & ~TRS_OUT;
wire orch90r_sel_out = (TRS_A == 8'h79) & ~TRS_OUT;

// fpga flash spi @ FCh-FDh
wire spi_ctrl_sel_out = (TRS_A == 8'hFC) & ~TRS_OUT;
wire spi_data_sel_in  = (TRS_A == 8'hFD) & ~TRS_IN;
wire spi_data_sel_out = (TRS_A == 8'hFD) & ~TRS_OUT;


wire esp_sel_in  = trs_io_sel_in  | frehd_sel_in  | printer_sel_rd;
wire esp_sel_out = trs_io_sel_out | frehd_sel_out | printer_sel_wr;
wire esp_sel = esp_sel_in | esp_sel_out;

assign EXTIOSEL = esp_sel_in | hires_sel_in | spi_data_sel_in;

wire esp_sel_risingedge = io_access & esp_sel;

reg [2:0] esp_done_raw; always @(posedge clk) esp_done_raw <= {esp_done_raw[1:0], ESP_DONE};
wire esp_done_risingedge = esp_done_raw[2:1] == 2'b01;

reg [6:0] esp_req_count = 6'd1;
reg trs_io_wait = 1'b0;

assign WAIT = trs_io_wait;

always @(posedge clk) begin
  if (esp_sel_risingedge) begin
    // ESP needs to do something
    esp_req_count <= -7'd50;
  end
  if (esp_sel_risingedge) begin
    // Assert WAIT
    trs_io_wait <= 1'b1;
  end
  else if (esp_done_risingedge) begin
    // When ESP is done, de-assert WAIT
    trs_io_wait <= 1'b0;
  end
  if (esp_req_count != 7'd0) begin
    esp_req_count <= esp_req_count + 7'd1;
  end
end

assign ESP_REQ = esp_req_count[6];


localparam [3:0]
  esp_trs_io_in  = 4'd0,
  esp_trs_io_out = 4'd1,
  esp_frehd_in   = 4'd2,
  esp_frehd_out  = 4'd3,
  esp_printer_rd = 4'd4,
  esp_printer_wr = 4'd5,
  esp_xray       = 4'd6;


assign ESP_S = ~( (~esp_trs_io_in  & {4{trs_io_sel_in }}) |
                  (~esp_trs_io_out & {4{trs_io_sel_out}}) |
                  (~esp_frehd_in   & {4{frehd_sel_in  }}) |
                  (~esp_frehd_out  & {4{frehd_sel_out }}) |
                  (~esp_printer_rd & {4{printer_sel_rd}}) |
                  (~esp_printer_wr & {4{printer_sel_wr}}) );


//---main-------------------------------------------------------------------------

localparam [2:0]
  idle       = 3'b000,
  read_bytes = 3'b001,
  execute    = 3'b010;

reg [2:0] state = idle;

wire start_msg = 1'b0;

localparam [7:0]
  get_cookie          = 8'b0,
  bram_poke           = 8'd1,
  bram_peek           = 8'd2,
  dbus_read           = 8'd3,
  dbus_write          = 8'd4,
  data_ready          = 8'd5,
  set_breakpoint      = 8'd6,
  clear_breakpoint    = 8'd7,
  xray_code_poke      = 8'd8,
  xray_data_poke      = 8'd9,
  xray_data_peek      = 8'd10,
  enable_breakpoints  = 8'd11,
  disable_breakpoints = 8'd12,
  xray_resume         = 8'd13,
  set_full_addr       = 8'd14,
  get_version         = 8'd15,
  get_mode            = 8'd16,
  set_screen_color    = 8'd17,
  abus_read           = 8'd18,
  send_keyb           = 8'd19,
  set_led             = 8'd26,
  get_config          = 8'd27,
  set_spi_ctrl_reg    = 8'd29,
  set_spi_data        = 8'd30,
  get_spi_data        = 8'd31,
  set_esp_status      = 8'd32,
  set_vprinter_en     = 8'd33;


reg [7:0] byte_in, byte_out;
reg byte_received = 1'b0;

reg [7:0] params[0:4];
reg [2:0] bytes_to_read;
reg [2:0] idx;
reg [7:0] cmd;
reg trs_io_data_ready = 1'b0;

assign INT = trs_io_data_ready;

reg trigger_action = 1'b0;
reg spi_error = 1'b0;

always @(posedge clk) begin
  trigger_action <= 1'b0;

  if (io_access && trs_io_sel) trs_io_data_ready <= 1'b0;

  if (start_msg)
    state <= idle;
  else if (byte_received) begin
    case (state)
    idle:
      begin
        trigger_action <= 1'b0;
        cmd <= byte_in;
        state <= read_bytes;
        idx <= 3'b000;
        case (byte_in)
          get_cookie: begin
            trigger_action <= 1'b1;
            state <= idle;
          end
          get_version: begin
            trigger_action <= 1'b1;
            state <= idle;
          end
          bram_poke: begin
            bytes_to_read <= 3'd3;
          end
          bram_peek: begin
            bytes_to_read <= 3'd2;
          end
          dbus_read: begin
            trigger_action <= 1'b1;
            state <= idle;
          end
          dbus_write: begin
            bytes_to_read <= 3'd1;
          end
          abus_read: begin
            trigger_action <= 1'b1;
            state <= idle;
          end
          data_ready: begin
            trs_io_data_ready <= 1'b1;
            state <= idle;
          end
          set_breakpoint: begin
            bytes_to_read <= 3'd3;
          end
          clear_breakpoint: begin
            bytes_to_read <= 3'd1;
          end
          xray_code_poke: begin
            bytes_to_read <= 3'd2;
          end
          xray_data_poke: begin
            bytes_to_read <= 3'd2;
          end
          xray_data_peek: begin
            bytes_to_read <= 3'd1;
          end
          xray_resume: begin
            trigger_action <= 1'b1;
            state <= idle;
          end
          set_full_addr: begin
            bytes_to_read <= 3'd1;
          end
          get_mode: begin
            trigger_action <= 1'b1;
            state <= idle;
          end
          set_screen_color: begin
            bytes_to_read <= 3'd3;
          end
          send_keyb: begin
            bytes_to_read <= 3'd2;
          end
          set_led: begin
            bytes_to_read <= 3'd1;
          end
          get_config: begin
            trigger_action <= 1'b1;
            state <= idle;
          end
          set_spi_ctrl_reg: begin
            bytes_to_read <= 3'd1;
          end
          set_spi_data: begin
            bytes_to_read <= 3'd1;
          end
          get_spi_data: begin
            trigger_action <= 1'b1;
            state <= idle;
          end
          set_esp_status: begin
            bytes_to_read <= 3'd1;
          end
          set_vprinter_en: begin
            bytes_to_read <= 3'd1;
          end
          default:
            begin
              state <= idle;
              spi_error <= 1'b1;
            end
        endcase
      end
    read_bytes:
      begin
        params[idx] <= byte_in;
        idx <= idx + 3'b001;
        
        if (bytes_to_read == 3'd1)
          begin
            trigger_action <= 1'b1;
            state <= idle;
          end
        else
          bytes_to_read <= bytes_to_read - 3'd1;
      end
    default:
      state <= idle;
      endcase
  end
end


reg [7:0] trs_data;

always @(posedge clk) begin
  if (trigger_action && cmd == dbus_write)
    trs_data <= params[0];
end


//---SPI---------------------------------------------------------

reg [2:0] SCKr;  always @(posedge clk) SCKr <= {SCKr[1:0], SCK};
wire SCK_rising_edge = (SCKr[2:1] == 2'b01);
wire SCK_falling_edge = (SCKr[2:1] == 2'b10);

reg [2:0] CSr;  always @(posedge clk) CSr <= {CSr[1:0], CS};
wire CS_active = ~CSr[1];
//wire CS_startmessage = (CSr[2:1]==2'b10);
//wire CS_endmessage = (CSr[2:1]==2'b01);

//assign start_msg = CS_startmessage;
//wire end_msg = CS_endmessage;

reg [1:0] MOSIr;  always @(posedge clk) MOSIr <= {MOSIr[0], MOSI};
wire MOSI_data = MOSIr[1];

reg [2:0] bitcnt = 3'b000;
reg [7:0] byte_data_sent;

always @(posedge clk) begin
  byte_received <= 1'b0;

  if(~CS_active)
    bitcnt <= 3'b000;
  else begin
    if(SCK_rising_edge) begin
      bitcnt <= bitcnt + 3'b001;
      byte_in <= {byte_in[6:0], MOSI_data};
      if(bitcnt == 3'b111)
         byte_received <= 1'b1;
    end

    if(SCK_falling_edge) begin
      if(bitcnt == 3'b001)
        byte_data_sent <= byte_out;
      else
        byte_data_sent <= {byte_data_sent[6:0], 1'b0};
    end
  end
end

assign MISO = CS_active ? byte_data_sent[7] : 1'bz;


//---ESP Status----------------------------------------------------------------------------

reg [7:0] esp_status = 0;

wire esp_status_esp_ready   = esp_status[0];
wire esp_status_wifi_up     = esp_status[1];
wire esp_status_smb_mounted = esp_status[2];
wire esp_status_sd_mounted  = esp_status[3];

always @(posedge clk) begin
  if (trigger_action && cmd == set_esp_status)
    esp_status <= params[0];
end


//---Printer enable----------------------------------------------------------------------------

always @(posedge clk) begin
  if (trigger_action && cmd == set_vprinter_en)
    vprinter_en <= params[0][0];
end


//---LED-----------------------------------------------------------------------------------

always @(posedge clk) begin
  if (trigger_action && cmd == set_led) begin
    LED_RED   <= params[0][0];
    LED_GREEN <= params[0][1];
    LED_BLUE  <= params[0][2];
  end
end


// forward references 
wire [7:0] spi_data_in;
 
always @(posedge clk)
begin
  if (trigger_action)
    case (cmd)
      dbus_read:   byte_out <= TRS_D;
      abus_read:   byte_out <= TRS_A;
      get_cookie:  byte_out <= COOKIE;
      get_version: byte_out <= {VERSION_MAJOR, VERSION_MINOR};
      get_config:  byte_out <= {4'b0000, ~CONF};
      get_spi_data:byte_out <= spi_data_in;
      get_mode:    byte_out <= {4'b0000, this_mode | ((add_dip_4 & ~CONF[3]) << 3)};
    endcase
end


//---BUS INTERFACE----------------------------------------------------------------

assign TRS_DIR = TRS_RD & TRS_IN;

assign TRS_OE = ~(~TRS_WR | ~TRS_OUT |
                   esp_sel_in        |
                   hires_sel_in      |
                   spi_data_sel_in   );


wire [7:0] hires_dout;

assign _D = (~TRS_RD | ~TRS_IN)
             ? ( ({8{esp_sel_in      }} & trs_data   ) |
                 ({8{hires_sel_in    }} & hires_dout ) |
                 ({8{spi_data_sel_in }} & spi_data_in) )
             : 8'hzz;


//-----HDMI------------------------------------------------------------------------

logic [23:0] rgb_screen_color = 24'hFFFFFF;

always @(posedge clk) begin
  if (trigger_action && cmd == set_screen_color)
    rgb_screen_color <= {params[0], params[1], params[2]};
end


logic [8:0] audio_cnt;
logic clk_audio;

always @(posedge clk_in) audio_cnt <= (audio_cnt == 9'd280) ? 9'd0 : audio_cnt + 9'd1;
always @(posedge clk_in) if (audio_cnt == 9'd0) clk_audio <= ~clk_audio;

logic [15:0] audio_sample_word [1:0] = '{16'd0, 16'd0};


//-----HDMI3-----------------------------------------------------------------------

wire clk3_pixel;
wire clk3_pixel_x5;

// 125.875 MHz (126 MHz actual)
Gowin_rPLL3 pll3(
  .clkout(clk3_pixel_x5), //output
  .clkin(clk_in) //input
);

// 25.175 MHz (25.2 MHz actual)
Gowin_CLKDIV0 clk3div0(
  .clkout(clk3_pixel), //output
  .hclkin(clk3_pixel_x5), //input
  .resetn(1'b1) //input
);

reg [23:0] rgb3 = 24'h0;
wire dsp_vid, dsp_present;
wire vga3_vid;
wire hires_enable;

always @(posedge clk3_pixel)
begin
  rgb3 <= ((!dsp_present || hires_enable) ? vga3_vid : dsp_vid) ? rgb_screen_color : 24'h0;
end

logic [9:0] cx3, frame_width3, screen_width3;
logic [9:0] cy3, frame_height3, screen_height3;
wire [2:0] tmds_3;
wire tmds_clock_3;

// 640x480 @ 60Hz
hdmi #(.VIDEO_ID_CODE(1), .VIDEO_REFRESH_RATE(60), .AUDIO_RATE(48000), .AUDIO_BIT_WIDTH(16)) hdmi3(
  .clk_pixel_x5(clk3_pixel_x5),
  .clk_pixel(clk3_pixel),
  .clk_audio(clk_audio),
  .reset(1'b0),
  .rgb(rgb3),
  .audio_sample_word(audio_sample_word),
  .tmds(tmds_3),
  .tmds_clock(tmds_clock_3),
  .cx(cx3),
  .cy(cy3),
  .frame_width(frame_width3),
  .frame_height(frame_height3),
  .screen_width(screen_width3),
  .screen_height(screen_height3)
);

TLVDS_OBUF tmds [2:0] (
  .O(HDMI_TX_P),
  .OB(HDMI_TX_N),
  .I(tmds_3)
);

TLVDS_OBUF tmds_clock(
  .O(HDMI_TXC_P),
  .OB(HDMI_TXC_N),
  .I(tmds_clock_3)
);


//-----VGA3------------------------------------------------------------------------------

wire clk3_vga = clk3_pixel;
wire crt_vid, crt_hsync, crt_vsync;
wire hertz50;

reg sync3;

vga3 vga3(
  .clk(clk), // input
  .srst(rst), // input
  .vga_clk(clk3_vga), // 25.2 MHz
  .TRS_A(TRS_A), // input [7:0]
  .TRS_D(TRS_D), // input [7:0]
  .TRS_OUT(TRS_OUT), // input
  .TRS_IN(TRS_IN), // input
  .io_access(io_access), // input
  .hires_dout(hires_dout), // output [7:0]
  .hires_dout_rdy(), // output
  .hires_enable(hires_enable), // output
  .VGA_VID(vga3_vid), // output
  .VGA_HSYNC(vga_hsync), // output
  .VGA_VSYNC(vga_vsync), // output
  .CRT_VID(crt_vid), // output
  .CRT_HSYNC(crt_hsync), // output
  .CRT_VSYNC(crt_vsync), // output
  .HZ50(hertz50), // input
  .genlock(sync3) // input
);

always @(posedge clk3_pixel)
begin
  sync3 <= (cx3 == frame_width3 - 10) && (cy3 == frame_height3 - 1);
end


wire HSYNC_I = PMOD[7];
wire VSYNC_I = PMOD[6];
wire VIDEO_I = PMOD[5];

wire HSYNC_O = hires_enable ?  crt_hsync : HSYNC_I;
wire VSYNC_O = hires_enable ? ~crt_vsync : VSYNC_I;
wire VIDEO_O = hires_enable ?  crt_vid   : VIDEO_I;

assign PMOD[3] = HSYNC_O;
assign PMOD[2] = VSYNC_O;
assign PMOD[1] = VIDEO_O;

//---------------------------------------------------------------------------------

// Detect if video input is present and if it's 60 or 60 Hz.

videodetector videodetector(
  .vgaclk(clk3_pixel), // 25.2MHz clock
  .hsync_in(HSYNC_I), // input
  .vsync_in(VSYNC_I), // input

  .present(dsp_present), // output
  .hertz50(hertz50) // output 
);


//-----Framegrabber----------------------------------------------------------------

wire lock;

framegrabber framegrabber(
  .vgaclk_x5(clk3_pixel_x5), // 126MHz clock
  .vgaclk(clk3_pixel), // 25.2MHz clock
  .hsync_in(HSYNC_I), // input
  .vsync_in(VSYNC_I), // input
  .pixel_in(VIDEO_I), // input
  .HZ50(hertz50), // input

  .cx(cx3), // input [9:0]
  .cy(cy3), // input [9:0]

  .vga_rgb(dsp_vid), // output

  .dpll_lock(lock), // output
  .dpll_hcheck(), // output
  .dpll_vcheck() // output
);


//-----ORCH90----------------------------------------------------------------------

// orchestra-90 output registers
reg [7:0] orch90l_reg;
reg [7:0] orch90r_reg;

always @ (posedge clk)
begin
   if(io_access & orch90l_sel_out)
      orch90l_reg <= TRS_D;

   if(io_access & orch90r_sel_out)
      orch90r_reg <= TRS_D;
end


//-----Cassette out----------------------------------------------------------------

// raw 2-bit cassette output
reg [1:0] cass_reg = 2'b00;

always @(posedge clk)
begin
   if (io_access && cass_sel_out)
      cass_reg <= TRS_D[1:0];
end

// bit1 is inverted and added to bit0 for the analog output
wire [1:0] cass_outx = {~cass_reg[1], cass_reg[0]};
// the sum is 0, 1, or 2
wire [1:0] cass_outy = {1'b0, cass_outx[1]} + {1'b0, cass_outx[0]} - 2'b01;

reg [8:0] cass_outl_reg;
reg [8:0] cass_outr_reg;

always @ (posedge clk)
begin
   cass_outl_reg <= {orch90l_reg[7], orch90l_reg} + {cass_outy, 7'b0000000};
   cass_outr_reg <= {orch90r_reg[7], orch90r_reg} + {cass_outy, 7'b0000000};
end

reg [9:0] cass_pdml_reg;
reg [9:0] cass_pdmr_reg;

always @ (posedge clk)
begin
   cass_pdml_reg <= {1'b0, cass_pdml_reg[8:0]} + {1'b0, ~cass_outl_reg[8], cass_outl_reg[7:0]};
   cass_pdmr_reg <= {1'b0, cass_pdmr_reg[8:0]} + {1'b0, ~cass_outr_reg[8], cass_outr_reg[7:0]};
end

always @(posedge clk_audio)
begin
   audio_sample_word <= '{{cass_outr_reg, 7'b0000000},
                          {cass_outl_reg, 7'b0000000}};
end


assign CASS_OUT_L = cass_pdml_reg[9];
assign CASS_OUT_R = cass_pdmr_reg[9];


//----XFLASH---------------------------------------------------------------------

// SPI Flash control register
// bit7 is CS  (active high)
// bit6 is WPN (active low)
reg [7:0] spi_ctrl_reg = 8'h00;

always @(posedge clk)
begin
   if(io_access & spi_ctrl_sel_out)
      spi_ctrl_reg <= TRS_D;
   else if(trigger_action && cmd == set_spi_ctrl_reg)
      spi_ctrl_reg <= params[0];
end

// The SPI shift register is by design faster than the z80 can read and write.
// Therefore a status bit isn't necessary.  The z80 can read or write and then
// immediately read or write again on the next instruction.
reg [7:0] spi_shift_reg;
reg spi_sdo;
reg [7:0] spi_counter = 8'b0;

always @(posedge clk)
begin
   if(spi_counter[7])
   begin
      spi_counter <= spi_counter + 8'b1;
      if(spi_counter[2:0] == 3'b000)
      begin
         if(spi_counter[3] == 1'b0)
            spi_sdo <= spi_shift_reg[7];
         else
            spi_shift_reg <= {spi_shift_reg[6:0], FLASH_SPI_SO};
      end
   end
   else if(io_access & spi_data_sel_out)
   begin
      spi_shift_reg <= TRS_D;
      spi_counter <= 8'b10000000;
   end
   else if(trigger_action && cmd == set_spi_data)
   begin
      spi_shift_reg <= params[0];
      spi_counter <= 8'b10000000;
   end
end

assign spi_data_in = spi_shift_reg;

assign FLASH_SPI_CS_N = ~spi_ctrl_reg[7];
assign FLASH_SPI_CLK  = spi_counter[3];
assign FLASH_SPI_SI   = spi_sdo;


//-----LED------------------------------------------------------------------------------------

reg [25:0] heartbeat;

always @ (posedge clk)
   heartbeat <= heartbeat + 26'b1;


assign LED[0] = esp_sel;
assign LED[1] = WAIT;
assign LED[2] = INT;
assign LED[3] = heartbeat[25] | spi_error;
endmodule
