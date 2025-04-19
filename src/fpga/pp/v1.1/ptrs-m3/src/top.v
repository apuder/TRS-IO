`timescale 1ns / 1ps

module top(
  input clk_in,
  output _RESET_N,

  output CTRL_DIR,
  output CTRL_EN,
  output CTRL1_EN,
  output _RD_N,
  output _WR_N,
  output _IN_N,
  output _OUT_N,
  output _RAS_N,
  output _IOREQ_N,
  output _M1_N,
  output ABUS_DIR_N,
  output ABUS_DIR,
  output ABUS_EN,
  output [15:0] _A,
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

localparam add_dip_4 = 1'b1;
wire [3:0] this_mode = mode_ptrs_m3;


wire[7:0] TRS_A, TRS_AH;
wire[7:0] TRS_D;

wire TRS_OE;
assign DBUS_EN = TRS_OE;
wire TRS_DIR;
assign DBUS_DIR = TRS_DIR;

wire CS = CS_FPGA;

assign ABUS_DIR = 1'b0;
assign ABUS_DIR_N = ~ABUS_DIR;
assign ABUS_EN = 1'b0;

assign CTRL_DIR = 1'b0;
assign CTRL_EN = 1'b0;
assign CTRL1_EN = 1'b0;


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

//-------Configuration-----------------------------------------------------------

// conf[3] switch 'off' enables the internal trs-io
wire use_internal_trs_io = CONF[3];

// external expansion connector enable
wire xio_enab;

// Virtual printer
reg vprinter_en = 1'b1;


//----Address Decoder------------------------------------------------------------

wire TRS_RD;
wire TRS_WR;

wire TRS_IN;
wire TRS_OUT;

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

// m3: printer @ F8h-F9h (io)
wire printer_sel_m3 = vprinter_en & (TRS_A[7:2]  == (8'hF8 >> 2));
wire printer_sel_m3_in  = printer_sel_m3 & ~TRS_IN;
wire printer_sel_m3_out = printer_sel_m3 & ~TRS_OUT;
wire printer_sel_rd = printer_sel_m3_in;
wire printer_sel_wr = printer_sel_m3_out;

// trs-io @ 1Fh
wire trs_io_sel_in  = use_internal_trs_io & (TRS_A == 8'd31) & ~TRS_IN;
wire trs_io_sel_out = use_internal_trs_io & (TRS_A == 8'd31) & ~TRS_OUT;
wire trs_io_sel = trs_io_sel_in | trs_io_sel_out;

// frehd @ C0h-CFh
wire frehd_sel_in  = use_internal_trs_io & (TRS_A[7:4] == 4'hC) & ~TRS_IN;
wire frehd_sel_out = use_internal_trs_io & (TRS_A[7:4] == 4'hC) & ~TRS_OUT;

// m3/4: orchestra-90 @ 79h,75h
wire orch90l_sel_out = (TRS_A == 8'h75) & ~TRS_OUT;
wire orch90r_sel_out = (TRS_A == 8'h79) & ~TRS_OUT;

// fpga flash spi @ FCh-FDh
wire spi_ctrl_sel_out = (TRS_A == 8'hFC) & ~TRS_OUT;
wire spi_data_sel_in  = (TRS_A == 8'hFD) & ~TRS_IN;
wire spi_data_sel_out = (TRS_A == 8'hFD) & ~TRS_OUT;

// External expansion bus
wire trs_xio_sel = (~TRS_IOREQ & ~((use_internal_trs_io & (TRS_A == 8'd31)            ) | // 1f     trs-io
                                   (use_internal_trs_io & (TRS_A[7:4] == 4'hC)        ) | // c0-cf  frehd
                                   (~vprinter_en        & (TRS_A[7:2] == (8'hF8 >> 2))) | // f8-fb  printer
                                                          (TRS_A[7:1] == (8'hFC >> 1))) );// fc-fd  flash spi


wire esp_sel_in  = trs_io_sel_in  | frehd_sel_in  | printer_sel_rd;
wire esp_sel_out = trs_io_sel_out | frehd_sel_out | printer_sel_wr;
wire esp_sel = esp_sel_in | esp_sel_out;

wire extiosel = esp_sel_in | spi_data_sel_in;

wire esp_sel_risingedge = io_access & esp_sel;

reg [2:0] esp_done_raw; always @(posedge clk) esp_done_raw <= {esp_done_raw[1:0], ESP_DONE};
wire esp_done_risingedge = esp_done_raw[2:1] == 2'b01;

reg [6:0] esp_req_count = 6'd1;
reg trs_io_wait = 1'b0;

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
  esp_xray       = 4'd6,
  esp_cass_motor_on  = 4'd13,
  esp_cass_motor_off = 4'd14;

reg cass_motor_on_sel  = 1'b0;
reg cass_motor_off_sel = 1'b0;
wire cass_motor_sel = cass_motor_on_sel | cass_motor_off_sel;

assign ESP_S = ~( (~esp_trs_io_in  & {4{trs_io_sel_in }}) |
                  (~esp_trs_io_out & {4{trs_io_sel_out}}) |
                  (~esp_frehd_in   & {4{frehd_sel_in  }}) |
                  (~esp_frehd_out  & {4{frehd_sel_out }}) |
                  (~esp_printer_rd & {4{printer_sel_rd}}) |
                  (~esp_printer_wr & {4{printer_sel_wr}}) |
                  (~esp_cass_motor_on  & {4{cass_motor_on_sel }}) |
                  (~esp_cass_motor_off & {4{cass_motor_off_sel}}) );


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
  ptrs_rst            = 8'd20,
  z80_pause           = 8'd21,
  z80_resume          = 8'd22,
  z80_dsp_set_addr    = 8'd23,
  z80_dsp_poke        = 8'd24,
  z80_dsp_peek        = 8'd25,
  set_led             = 8'd26,
  get_config          = 8'd27,
  set_cass_in         = 8'd28,
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
          ptrs_rst: begin
            trigger_action <= 1'b1;
            state <= idle;
          end
          z80_pause: begin
            trigger_action <= 1'b1;
            state <= idle;
          end
          z80_resume: begin
            trigger_action <= 1'b1;
            state <= idle;
          end
          z80_dsp_set_addr: begin
            bytes_to_read <= 3'd2;
          end
          z80_dsp_poke: begin
            bytes_to_read <= 3'd1;
          end
          z80_dsp_peek: begin
            trigger_action <= 1'b1;
            state <= idle;
          end
          set_cass_in: begin
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


//---Keyboard-----------------------------------------------------------------------------

reg [7:0] keyb_matrix[0:7];

always @(posedge clk) begin
  if (trigger_action && cmd == send_keyb) begin
    keyb_matrix[params[0]] <= params[1];
  end
end

wire keyb_matrix_pressed = |(keyb_matrix[7] | keyb_matrix[6] | keyb_matrix[5] | keyb_matrix[4] |
                             keyb_matrix[3] | keyb_matrix[2] | keyb_matrix[1] | keyb_matrix[0]);


//---LED-----------------------------------------------------------------------------------

always @(posedge clk) begin
  if (trigger_action && cmd == set_led) begin
    LED_RED   <= params[0][0];
    LED_GREEN <= params[0][1];
    LED_BLUE  <= params[0][2];
  end
end


//------Video RAM peek/poke interface-----------------------------------------

wire do_dsp_poke = trigger_action && cmd == z80_dsp_poke;
wire do_dsp_peek = trigger_action && cmd == z80_dsp_peek;
wire dsp_ce = do_dsp_peek || do_dsp_poke;
reg [15:0] dsp_addr;
wire dsp_wre = do_dsp_poke;
wire [7:0] dsp_din = params[0];
wire [7:0] _dsp_dout;
reg [7:0] dsp_dout;
wire dsp_ram_data_read;
wire dsp_ram_data_ready;
wire dsp_incr_addr;

trigger dsp_ram_read_trigger(
  .clk(clk),
  .cond(do_dsp_peek),
  .one(),
  .two(dsp_ram_data_read),
  .three(dsp_ram_data_ready)
);

trigger dsp_incr_trigger(
  .clk(clk),
  .cond(dsp_ce),
  .one(),
  .two(),
  .three(dsp_incr_addr)
);

always @(posedge clk) begin
  if (trigger_action && cmd == z80_dsp_set_addr) dsp_addr <= {params[1], params[0]};
  if (dsp_ram_data_read) dsp_dout <= _dsp_dout;
  if (dsp_incr_addr) dsp_addr <= dsp_addr + 16'd1;
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
      get_config:  byte_out <= {1'b0, is_80cols, is_doublwide, is_hires, ~CONF};
      get_spi_data:byte_out <= spi_data_in;
      get_mode:    byte_out <= {4'b0000, this_mode | ((add_dip_4 & ~CONF[3]) << 3)};
    endcase
  else if (dsp_ram_data_ready)
    byte_out <= dsp_dout;
end


//---BUS INTERFACE----------------------------------------------------------------

assign TRS_DIR = xio_enab
                ? ~(TRS_RD & TRS_IN)
                : 1'b1;

assign TRS_OE = xio_enab
               ? 1'b0
               : 1'b1;

assign _D = xio_enab
           ? ((TRS_RD & TRS_IN) ? TRS_D : 8'hzz)
           : 8'h00; 

wire [7:0] TRS_DI = ~( ({8{xio_enab & trs_xio_sel & ~EXTIOSEL_IN_N}} & ~_D)
                     | ({8{esp_sel_in     }} & ~trs_data   )
                     | ({8{spi_data_sel_in}} & ~spi_data_in) );


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


wire clk_pixel;
wire clk_pixel_x5;

// 125.875 MHz (126 MHz actual)
Gowin_rPLL0 pll0(
  .clkout(clk_pixel_x5), //output
  .clkin(clk_in) //input
);

// 25.175 MHz (25.2 MHz actual)
Gowin_CLKDIV0 clkdiv0(
  .clkout(clk_pixel), //output
  .hclkin(clk_pixel_x5), //input
  .resetn(1'b1) //input
);

reg [23:0] rgb = 24'h0;
wire vga3_vid;

always @(posedge clk_pixel)
begin
  rgb <= vga_vid ? rgb_screen_color : 24'h0;
end

logic [9:0] cx, frame_width, screen_width;
logic [9:0] cy, frame_height, screen_height;
wire [2:0] tmds_x;
wire tmds_clock_x;

// 640x480 @ 60Hz
hdmi #(.VIDEO_ID_CODE(1), .VIDEO_REFRESH_RATE(60), .AUDIO_RATE(48000), .AUDIO_BIT_WIDTH(16)) hdmi(
  .clk_pixel_x5(clk_pixel_x5),
  .clk_pixel(clk_pixel),
  .clk_audio(clk_audio),
  .reset(1'b0),
  .rgb(rgb),
  .audio_sample_word(audio_sample_word),
  .tmds(tmds_x),
  .tmds_clock(tmds_clock_x),
  .cx(cx),
  .cy(cy),
  .frame_width(frame_width),
  .frame_height(frame_height),
  .screen_width(screen_width),
  .screen_height(screen_height)
);

TLVDS_OBUF tmds [2:0] (
  .O(HDMI_TX_P),
  .OB(HDMI_TX_N),
  .I(tmds_x)
);

TLVDS_OBUF tmds_clock(
  .O(HDMI_TXC_P),
  .OB(HDMI_TXC_N),
  .I(tmds_clock_x)
);


//-----VGA-------------------------------------------------------------------------------

reg sync;


always @(posedge clk_pixel)
begin
  sync <= (cx == frame_width - 10) && (cy == frame_height - 1);
end


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
wire [1:0] cass_reg;

// bit1 is inverted and added to bit0 for the analog output
wire [1:0] cass_outx = {~cass_reg[1], cass_reg[0]};
// the sum is 0, 1, or 2
wire [1:0] cass_outy = {1'b0, cass_outx[1]} + {1'b0, cass_outx[0]} - 2'b01;

reg [8:0] cass_outl_reg;
reg [8:0] cass_outr_reg;

always @ (posedge z80_clk)
begin
   cass_outl_reg <= {orch90l_reg[7], orch90l_reg} + {cass_outy, 7'b0000000};
   cass_outr_reg <= {orch90r_reg[7], orch90r_reg} + {cass_outy, 7'b0000000};
end

reg [9:0] cass_pdml_reg;
reg [9:0] cass_pdmr_reg;

always @ (posedge z80_clk)
begin
   cass_pdml_reg <= {1'b0, cass_pdml_reg[8:0]} + {1'b0, ~cass_outl_reg[8], cass_outl_reg[7:0]};
   cass_pdmr_reg <= {1'b0, cass_pdmr_reg[8:0]} + {1'b0, ~cass_outr_reg[8], cass_outr_reg[7:0]};
end

reg [15:0] cass_outz;

always @ (posedge z80_clk)
begin
   cass_outz <= cass_outz - {{8{cass_outz[15]}}, cass_outz[15:8]} + {{8{cass_outy[1]}}, cass_outy, 6'b0};
end

wire cass_filt_en = 1'b1;

reg [8:0] cass_audl_reg;
reg [8:0] cass_audr_reg;

always @ (posedge z80_clk)
begin
   cass_audl_reg <= {orch90l_reg[7], orch90l_reg} + (cass_filt_en ? cass_outz[15:7] : {cass_outy, 7'b0000000});
   cass_audr_reg <= {orch90r_reg[7], orch90r_reg} + (cass_filt_en ? cass_outz[15:7] : {cass_outy, 7'b0000000});
end

always @(posedge clk_audio)
begin
   audio_sample_word <= '{{cass_audr_reg, 7'b0000000},
                          {cass_audl_reg, 7'b0000000}};
end


assign CASS_OUT_L = cass_pdml_reg[9];
assign CASS_OUT_R = cass_pdmr_reg[9];


//------------LiteBrite-80---------------------------------------------------------------

wire lb80_update = io_access & (~TRS_RD | ~TRS_WR);

reg [7:0] pmod_a;

always @ (posedge clk)
begin
   if(lb80_update)
      pmod_a <= TRS_AH;
end

assign PMOD = {~pmod_a[7:5], ~pmod_a[1], pmod_a[4:2], pmod_a[0]};
//assign PMOD = {pmod_a[4:2], pmod_a[0], pmod_a[7:5], pmod_a[1]};


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


//------PocketTRS-------------------------------------------------------------

wire z80_clk1, z80_clkH, z80_clkL;

// 84/5 = 16.8
Gowin_CLKDIV0 z80_clkdiv0(
  .clkout(z80_clk1), //output
  .hclkin(clk), //input
  .resetn(1'b1) //input
);

// 16.8/4 = 4.2
Gowin_CLKDIV1 z80_clkdiv1(
  .clkout(z80_clkH), //output
  .hclkin(z80_clk1), //input
  .resetn(1'b1) //input
);

// 4.2/2 = 2.1
Gowin_CLKDIV2 z80_clkdiv2(
  .clkout(z80_clkL), //output
  .hclkin(z80_clkH), //input
  .resetn(1'b1) //input
);


wire z80_clk = z80_clkL;


reg [3:0] z80_rst_cnt = 4'b0;

always @ (posedge clk) begin
  if (trigger_action && cmd == ptrs_rst) z80_rst_cnt <= 4'b1111;
  else if (z80_rst_cnt != 0) z80_rst_cnt <= z80_rst_cnt - 1;
end

wire z80_rst = z80_rst_cnt != 0;


reg cass_in;
wire cass_out_sel;
wire cass_out_sel_n = ~cass_out_sel;
wire wait_in_n;
wire int_in_n;
wire extiosel_in_n;

always@(posedge clk or negedge cass_out_sel_n) begin
  if(!cass_out_sel_n)
    cass_in <= 0;
  else if (trigger_action && cmd == set_cass_in)
    cass_in <= 1'b1;
  else
    cass_in <= cass_in;
end

wire cass_motor_on;
wire cass_motor_on_trigger;
wire cass_motor_off_trigger;

filter cass_motor(
  .clk(clk),
  .in(cass_motor_on),
  .out(),
  .rising_edge(cass_motor_on_trigger),
  .falling_edge(cass_motor_off_trigger)
);

always @(posedge clk) begin
  if (cass_motor_on_trigger) begin
    cass_motor_on_sel <= 1'b1;
    cass_motor_off_sel <= 1'b0;
  end
  if (cass_motor_off_trigger) begin
    cass_motor_on_sel <= 1'b0;
    cass_motor_off_sel <= 1'b1;
  end
  if (esp_done_risingedge) begin
    cass_motor_on_sel <= 1'b0;
    cass_motor_off_sel <= 1'b0;
  end
end


reg z80_is_paused = 1'b1;

always @(posedge clk) begin
  if (trigger_action && cmd == z80_pause) z80_is_paused <= 1'b1;
  if (trigger_action && ((cmd == z80_resume) || (cmd == ptrs_rst))) z80_is_paused <= 1'b0;
end

wire ttrs80_poke_rom = trigger_action && cmd == bram_poke;

wire [15:0] ttrs80_addr = ttrs80_poke_rom ? {params[1], params[0]}
                                          : dsp_addr;

wire [7:0] ttrs80_din = ttrs80_poke_rom ? params[2]
                                        : dsp_din;

TTRS80 TTRS80 (
   // Inputs
   .z80_clk(z80_clk),
   .z80_reset_n(~z80_rst),
   .z80_pause(z80_is_paused),
   .keyb_matrix(keyb_matrix),
   .vga_clk(clk_pixel),
   .genlock(sync),

   // Display RAM and ROM/RAM interface
   .clk(clk),
   .dsp_ce(dsp_ce),
   .rom_ce(ttrs80_poke_rom),
   .ram_ce(1'b0),
   .chr_ce(1'b0),
   .dsp_rom_ram_addr(ttrs80_addr),
   .dsp_rom_ram_wre(dsp_wre | ttrs80_poke_rom),
   .dsp_rom_ram_din(ttrs80_din),
   .dsp_rom_ram_dout(_dsp_dout),

   // Outputs
   .cpu_fast(),
   .pixel_data(vga_vid),
   .h_sync(),
   .v_sync(),
   .cass_motor_on(cass_motor_on),
   .cass_out(cass_reg),
   .cass_out_sel(cass_out_sel),
   .cass_in(cass_in),
   .is_80col(is_80cols),
   .is_doublwide(is_doublwide),
   .is_hires(is_hires),

   // Expansion connector
   // Inputs
   .xio_int_n(int_in_n),
   .xio_wait_n(wait_in_n),
   .xio_sel_n(extiosel_in_n),
   // Outputs
   .xio_mreq_n(TRS_MREQ),
   .xio_rd_n(TRS_RD),
   .xio_wr_n(TRS_WR),
   .xio_iorq_n(TRS_IOREQ),
   .xio_in_n(TRS_IN),
   .xio_out_n(TRS_OUT),
   .xio_addr({TRS_AH, TRS_A}),
   .xio_m1_n(TRS_M1),
   .xio_enab(xio_enab),
   // Inputs/Outputs
   .xio_data_in(TRS_DI),
   .xio_data_out(TRS_D)
);

assign wait_in_n     = ~((use_internal_trs_io & trs_io_wait      ) | (xio_enab & trs_xio_sel & ~WAIT_IN_N    ));
assign int_in_n      = ~((use_internal_trs_io & trs_io_data_ready) | (xio_enab &               ~INT_IN_N     ));
assign extiosel_in_n = ~((                      extiosel         ) | (xio_enab & trs_xio_sel & ~EXTIOSEL_IN_N));
assign WAIT          = 1'b0;
assign INT           = 1'b0;
assign EXTIOSEL      = 1'b0;


assign _RAS_N   = xio_enab ? TRS_MREQ  : 1'b1;
assign _RD_N    = xio_enab ? TRS_RD    : 1'b1;
assign _WR_N    = xio_enab ? TRS_WR    : 1'b1;
assign _IOREQ_N = xio_enab ? TRS_IOREQ : 1'b1;
assign _IN_N    = xio_enab ? TRS_IN    : 1'b1;
assign _OUT_N   = xio_enab ? TRS_OUT   : 1'b1;
assign _M1_N    = xio_enab ? TRS_M1    : 1'b1;
assign _A       = xio_enab ? {TRS_AH, TRS_A} : 16'h0000;


//-----LED------------------------------------------------------------------------------------

reg [25:0] heartbeat;

always @ (posedge clk)
   heartbeat <= heartbeat + 26'b1;


//assign LED[0] = z80_rst;

assign LED[0] = ~extiosel_in_n;
assign LED[1] = ~wait_in_n;
assign LED[2] = ~int_in_n;
//assign LED[0] = keyb_matrix_pressed;
//assign LED[1] = xio_enab;
assign LED[3] = heartbeat[25] | spi_error;

endmodule
