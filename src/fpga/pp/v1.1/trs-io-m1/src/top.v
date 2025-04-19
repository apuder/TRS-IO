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
wire [3:0] this_mode = mode_trs_io_m1;


reg TRS_INT;
wire[15:0] TRS_A = _A;
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

wire TRS_RD = _RD_N;
wire TRS_WR = _WR_N;

wire TRS_IN  = _IN_N;
wire TRS_OUT = _OUT_N;

wire io_access_raw = ~TRS_RD | ~TRS_WR | ~TRS_IN | ~TRS_OUT;

wire io_access;

filter io(
  .clk(clk),
  .in(io_access_raw),
  .out(),
  .rising_edge(io_access),
  .falling_edge()
);

wire ras_access;

filter ras(
  .clk(clk),
  .in(~_RAS_N),
  .out(),
  .rising_edge(ras_access),
  .falling_edge()
);

//----TRS-IO---------------------------------------------------------------------

reg full_addr = 1'b0;

// m1 extension rom (1.96875k @ 3000h-37DFh)
wire trs_extrom_sel = ((TRS_A[15:11] == 5'b00110) &         // 2k @ 3000h-37FFh
                       ~(TRS_A[15:5] == 11'b00110111111));  // - 32 @ 37E0h-37FFh
wire trs_extrom_sel_rd = trs_extrom_sel & ~TRS_RD;
wire trs_extrom_sel_wr = trs_extrom_sel & ~TRS_WR;

// m1 ram (32k @ 8000h-FFFFh)
wire trs_ram_sel = (TRS_A[15] == 1'b1);  // upper 32k
wire trs_ram_sel_rd = trs_ram_sel & ~TRS_RD;
wire trs_ram_sel_wr = trs_ram_sel & ~TRS_WR;

// m1 fdc irq status @ 37E0h-37E3h
wire fdc_37e0_sel_rd = (TRS_A[15:2] == (16'h37E0 >> 2)) & ~TRS_RD; // 37E0h-37E3h
// m1 fdc @ 37ECh-37EFh
wire fdc_37ec_sel_rd = (TRS_A[15:2] == (16'h37EC >> 2)) & ~TRS_RD; // 37ECh-37EFh
wire fdc_37ec_sel_wr = (TRS_A[15:2] == (16'h37EC >> 2)) & ~TRS_WR; // 37ECh-37EFh

// m1: printer @ 37E8h-37EBh (mem)
reg vprinter_en = 1'b1;
wire printer_sel_m1 = vprinter_en & (TRS_A[15:2] == (16'h37E8 >> 2));
wire printer_sel_m1_rd  = printer_sel_m1 & ~TRS_RD;
wire printer_sel_m1_wr  = printer_sel_m1 & ~TRS_WR;
wire printer_mem_trigger = printer_sel_m1 & ras_access;
wire printer_sel_rd = printer_sel_m1_rd;
wire printer_sel_wr = printer_sel_m1_wr;

// trs-io @ 1Fh
wire trs_io_sel_in  = (TRS_A[7:0] == 8'd31) & ~TRS_IN;
wire trs_io_sel_out = (TRS_A[7:0] == 8'd31) & ~TRS_OUT;
wire trs_io_sel = trs_io_sel_in | trs_io_sel_out;

// frehd @ C0h-CFh
wire frehd_sel_in  = (TRS_A[7:4] == 4'hC) & ~TRS_IN;
wire frehd_sel_out = (TRS_A[7:4] == 4'hC) & ~TRS_OUT;

// cassette @ FFh
wire cass_sel_out = (TRS_A[7:0] == 8'hFF) & ~TRS_OUT;

// m1: le18 graphics @ ECh-EFh
// m3: hires graphics @ 80h-83h
wire le18_data_sel_in = (TRS_A[7:0] == 8'hEC) & ~TRS_IN;
wire hires_data_sel_in = (TRS_A[7:0] == 8'h82) & ~TRS_IN;

// m1: orchestra-85 @ B9h,B5h
wire orch85l_sel_out = (TRS_A[7:0] == 8'hB5) & ~TRS_OUT;
wire orch85r_sel_out = (TRS_A[7:0] == 8'hB9) & ~TRS_OUT;

// fpga flash spi @ FCh-FDh
wire spi_ctrl_sel_out = (TRS_A[7:0] == 8'hFC) & ~TRS_OUT;
wire spi_data_sel_in  = (TRS_A[7:0] == 8'hFD) & ~TRS_IN;
wire spi_data_sel_out = (TRS_A[7:0] == 8'hFD) & ~TRS_OUT;


wire esp_sel_in  = trs_io_sel_in  | frehd_sel_in  | printer_sel_rd;
wire esp_sel_out = trs_io_sel_out | frehd_sel_out | printer_sel_wr;
wire esp_sel = esp_sel_in | esp_sel_out;

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
  if (esp_sel_risingedge || printer_mem_trigger) begin
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

assign INT = TRS_INT;

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


//---Full Address--------------------------------------------------------------------------

always @(posedge clk) begin
  if (trigger_action && cmd == set_full_addr) begin
    full_addr <= (params[0] != 0);
  end
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


//--------IRQ--------------------------------------------------------------------------

reg [7:0] irq_data;
reg [21:0] counter_25ms = 22'd0;

always @(posedge clk)
begin
   // 0.025*84000000 = 2100000
   if (counter_25ms == (22'd2100000 -22'd1))
   begin
      counter_25ms <= 22'd0;
      TRS_INT <= 1'b1;
   end
   else
   begin
      counter_25ms <= counter_25ms + 22'd1;
   end

   if (io_access & fdc_37e0_sel_rd)
   begin
      irq_data <= {TRS_INT, 1'b0, ~trs_io_data_ready, 5'b00000};
      TRS_INT <= 1'b0;
   end
end


//--------FDC--------------------------------------------------------------------------

/*
  ; Assembly of the autoboot. This will be returned when the M1 ROM reads in the
  ; boot sector from the FDC.
    org 4200h
    ld a,1
    out (197),a
    in a,(196)
    cp 254
    jp nz,0075h
    ld bc,196
    ld hl,20480
    inir
    jp 20480
*/
localparam [7:0] frehd_loader [0:22-1] =
  {8'h3E, 8'h01, 8'hD3, 8'hC5, 8'hDB, 8'hC4, 8'hFE, 8'hFE, 8'hC2, 8'h75, 8'h00, 8'h01,
   8'hC4, 8'h00, 8'h21, 8'h00, 8'h50, 8'hED, 8'hB2, 8'hC3, 8'h00, 8'h50};

reg [7:0] fdc_sector_idx = 8'd0;
reg [7:0] fdc_data;

always @ (posedge clk)
begin
   if(io_access & fdc_37ec_sel_rd)
      case(TRS_A[1:0])
      2'b00: // fdc status
         fdc_data <= 8'h02;
      2'b11: // fdc data
         begin
            fdc_data <= (fdc_sector_idx < 8'd22) ? frehd_loader[fdc_sector_idx] : 8'h00;
            fdc_sector_idx <= fdc_sector_idx + 8'd1;
         end
      endcase

   if(io_access & fdc_37ec_sel_wr)
      case(TRS_A[1:0])
      2'b00: // fdc command
         fdc_sector_idx <= 8'h00;
      endcase
end


//--------BRAM-------------------------------------------------------------------------

wire ram_rde, ram_oce;

trigger rama_read_trigger(
  .clk(clk),
  .cond(io_access & trs_ram_sel_rd),
  .one(ram_rde),
  .two(ram_oce),
  .three()
);

wire ram_wre;

trigger rama_write_trigger(
  .clk(clk),
  .cond(io_access & trs_ram_sel_wr),
  .one(),
  .two(ram_wre),
  .three()
);

wire [7:0] ram_dout;

wire ram_ceb;
wire ram_oceb;
wire ram_web;
wire [15:0] addrb;
wire [7:0] dinb;
wire [7:0] ram_doutb;


Gowin_DPB0 bram(
  .clka(clk), // input
  .cea(ram_rde | ram_wre), // input
  .ada(TRS_A[14:0]), // input [14:0]
  .wrea(~TRS_WR), // input
  .dina(TRS_D), // input [7:0]
  .douta(ram_dout), // output [7:0]
  .ocea(ram_oce), // input
  .reseta(1'b0), // input

  .clkb(clk), // input
  .ceb(ram_ceb), // input
  .adb(addrb[14:0]), // input [14:0]
  .wreb(ram_web), // input
  .dinb(dinb), // input [7:0]
  .doutb(ram_doutb), // output [7:0]
  .oceb(ram_oceb), // input
  .resetb(1'b0) // input
);


//---EXTENSION ROM----------------------------------------------------------------

wire extrom_rde, extrom_oce;

trigger extrom_rd_trigger(
  .clk(clk),
  .cond(io_access & trs_extrom_sel_rd),
  .one(extrom_rde),
  .two(extrom_oce),
  .three()
);

wire extrom_wre;

trigger extrom_write_trigger(
  .clk(clk),
  .cond(io_access & trs_extrom_sel_wr),
  .one(),
  .two(extrom_wre),
  .three()
);

wire [7:0] extrom_dout;
wire [7:0] extrom_doutb;
wire extrom_wr_en;

Gowin_DPB2 extrom (
   .clka(clk), // input
   .cea(extrom_rde | extrom_wre), // input
   .ada(TRS_A[10:0]), // input [10:0]
   .wrea(extrom_wr_en & ~TRS_WR), // input
   .dina(TRS_D), // input [7:0]
   .douta(extrom_dout), // output [7:0]
   .ocea(extrom_oce),
   .reseta(1'b0),
 
   .clkb(clk), // input
   .ceb(1'b0), // input
   .adb(addrb[10:0]), // input [10:0]
   .wreb(1'b0), // input
   .dinb(dinb), // input [7:0]
   .doutb(extrom_doutb), // output [7:0]
   .oceb(1'b0), // input
   .resetb(1'b0)
);


//--------BRAM-------------------------------------------------------------------------

assign addrb = {params[1], params[0]};
assign dinb = params[2];

wire ceb_peek, ceb_poke;
assign ram_ceb = ceb_peek | ceb_poke;
assign ram_web = (cmd == bram_poke);
wire bram_peek_done;

trigger bram_poke_trigger(
  .clk(clk),
  .cond(trigger_action & (cmd == bram_poke)),
  .one(ceb_poke),
  .two(),
  .three());

trigger bram_peek_trigger(
  .clk(clk),
  .cond(trigger_action & (cmd == bram_peek)),
  .one(ceb_peek),
  .two(ram_oceb),
  .three(bram_peek_done));


// forward references 
wire [7:0] spi_data_in;

always @(posedge clk)
begin
  if (trigger_action)
    case (cmd)
      dbus_read:   byte_out <= TRS_D;
      abus_read:   byte_out <= TRS_A[7:0];
      get_cookie:  byte_out <= COOKIE;
      get_version: byte_out <= {VERSION_MAJOR, VERSION_MINOR};
      get_config:  byte_out <= {4'b0000, ~CONF};
      get_spi_data:byte_out <= spi_data_in;
      get_mode:    byte_out <= {4'b0000, this_mode | ((add_dip_4 & ~CONF[3]) << 3)};
    endcase
  else if (bram_peek_done)
    byte_out <= ram_doutb;
end


//---BUS INTERFACE----------------------------------------------------------------

assign TRS_DIR = TRS_RD & TRS_IN;

assign TRS_OE = ~(~TRS_WR | ~TRS_OUT |
                   trs_extrom_sel_rd |
                   trs_ram_sel_rd    |
                   esp_sel_in        |
                   fdc_37e0_sel_rd   |
                   fdc_37ec_sel_rd   |
                   le18_data_sel_in  |
                   hires_data_sel_in |
                   spi_data_sel_in   );


wire [7:0] hires_dout;
wire [7:0] le18_dout;

assign _D = (~TRS_RD | ~TRS_IN)
             ? ( ({8{trs_extrom_sel_rd}} & extrom_dout) |
                 ({8{trs_ram_sel_rd   }} & ram_dout   ) |
                 ({8{esp_sel_in       }} & trs_data   ) |
                 ({8{fdc_37e0_sel_rd  }} & irq_data   ) |
                 ({8{fdc_37ec_sel_rd  }} & fdc_data   ) |
                 ({8{le18_data_sel_in }} & le18_dout  ) |
                 ({8{hires_data_sel_in}} & hires_dout ) |
                 ({8{spi_data_sel_in  }} & spi_data_in) )
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


//-----HDMI------------------------------------------------------------------------

wire hdmi_sel = ~CONF[1]; // for now for development

wire clk_pixel;
wire clk_pixel_x5;

// 200 MHz (200.571 MHz actual)
// 125.875 MHz (126 MHz actual)
Gowin_rPLLx pllx(
  .clkout(clk_pixel_x5), //output clkout
  .clkin(clk_in), //input clkin
  .fbdsel(hdmi_sel ? -6'd14 : -6'd37), //input [5:0]
  .idsel( hdmi_sel ? -6'd3  : -6'd5 ) //input [5:0]
);

// 40 MHz (40.114 MHz actual)
// 25.175 MHz (25.2 MHz actual)
Gowin_CLKDIV0 clkxdiv0(
  .clkout(clk_pixel), //output clkout
  .hclkin(clk_pixel_x5), //input hclkin
  .resetn(1'b1) //input resetn
);

reg [23:0] rgb = 24'h0;
wire vga1_vid;
wire vga3_vid;

always @(posedge clk1_pixel)
begin
  rgb <= (hdmi_sel ? vga3_vid : vga1_vid) ? rgb_screen_color : 24'h0;
end

//-----HDMI1-----------------------------------------------------------------------

wire clk1_pixel = clk_pixel;
wire clk1_pixel_x5 = clk_pixel_x5;

logic [10:0] cx1, frame_width1, screen_width1;
logic [9:0] cy1, frame_height1, screen_height1;
logic [9:0] tmds_1 [3-1:0];

// 800x600 @ 60Hz
hdmi #(.VIDEO_ID_CODE(5), .VIDEO_REFRESH_RATE(60), .AUDIO_RATE(48000), .AUDIO_BIT_WIDTH(16)) hdmi1(
  .clk_pixel_x5(clk1_pixel_x5),
  .clk_pixel(clk1_pixel),
  .clk_audio(clk_audio),
  .reset(1'b0),
  .rgb(rgb),
  .audio_sample_word(audio_sample_word),
  .tmds(),
  .tmds_clock(),
  .cx(cx1),
  .cy(cy1),
  .frame_width(frame_width1),
  .frame_height(frame_height1),
  .screen_width(screen_width1),
  .screen_height(screen_height1),
  .tmds_internal(tmds_1)
);


//-----HDMI3-----------------------------------------------------------------------

wire clk3_pixel = clk_pixel;
wire clk3_pixel_x5 = clk_pixel_x5;

logic [9:0] cx3, frame_width3, screen_width3;
logic [9:0] cy3, frame_height3, screen_height3;
logic [9:0] tmds_3 [3-1:0];

// 640x480 @ 60Hz
hdmi #(.VIDEO_ID_CODE(1), .VIDEO_REFRESH_RATE(60), .AUDIO_RATE(48000), .AUDIO_BIT_WIDTH(16)) hdmi3(
  .clk_pixel_x5(clk3_pixel_x5),
  .clk_pixel(clk3_pixel),
  .clk_audio(clk_audio),
  .reset(1'b0),
  .rgb(rgb),
  .audio_sample_word(audio_sample_word),
  .tmds(),
  .tmds_clock(),
  .cx(cx3),
  .cy(cy3),
  .frame_width(frame_width3),
  .frame_height(frame_height3),
  .screen_width(screen_width3),
  .screen_height(screen_height3),
  .tmds_internal(tmds_3)
);


//-----HDMI1/HDMI3 Selection-------------------------------------------------------

wire [2:0] tmds_x;
wire tmds_clock_x;

serializer #(.NUM_CHANNELS(3), .VIDEO_RATE(0)) serializer(.clk_pixel(clk_pixel), .clk_pixel_x5(clk_pixel_x5), .reset(1'b0), .tmds_internal(hdmi_sel ? tmds_3 : tmds_1), .tmds(tmds_x), .tmds_clock(tmds_clock_x));


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


//-----VGA1------------------------------------------------------------------------------

reg sync1;

vga1 vga1(
  .clk(clk),
  .srst(1'b0),
  .vga_clk(clk1_pixel), // 40 MHz
  .vga_clk_en(cx1[0]),
  .TRS_A(TRS_A),
  .TRS_D(TRS_D),
  .TRS_WR(TRS_WR),
  .TRS_OUT(TRS_OUT),
  .TRS_IN(TRS_IN),
  .io_access(io_access),
  .le18_dout(le18_dout),
  .le18_dout_rdy(),
  .le18_enable(),
  .VGA_VID(vga1_vid),
  .VGA_HSYNC(),
  .VGA_VSYNC(),
  .genlock(sync1)
);

always @(posedge clk1_pixel)
begin
  sync1 <= (cx1 == frame_width1 - 10) && (cy1 == frame_height1 - 1);
end


//-----VGA3------------------------------------------------------------------------------

wire clk3_vga = clk3_pixel;

reg sync3;

vga3 vga3(
  .clk(clk),
  .srst(1'b0),
  .vga_clk(clk3_vga), // 25 MHz
  .TRS_A(TRS_A),
  .TRS_D(TRS_D),
  .TRS_WR(TRS_WR),
  .TRS_OUT(TRS_OUT),
  .TRS_IN(TRS_IN),
  .io_access(io_access),
  .hires_dout(hires_dout),
  .hires_dout_rdy(),
  .hires_enable(),
  .VGA_VID(vga3_vid),
  .VGA_HSYNC(),
  .VGA_VSYNC(),
  .genlock(sync3)
);

always @(posedge clk3_pixel)
begin
  sync3 <= (cx3 == frame_width3 - 10) && (cy3 == frame_height3 - 1);
end


//-----ORCH85----------------------------------------------------------------------

// orchestra-85 output registers
reg [7:0] orch85l_reg;
reg [7:0] orch85r_reg;

always @ (posedge clk)
begin
   if(io_access & orch85l_sel_out)
      orch85l_reg <= TRS_D;

   if(io_access & orch85r_sel_out)
      orch85r_reg <= TRS_D;
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
   cass_outl_reg <= {orch85l_reg[7], orch85l_reg} + {cass_outy, 7'b0000000};
   cass_outr_reg <= {orch85r_reg[7], orch85r_reg} + {cass_outy, 7'b0000000};
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


//------------LiteBrite-80---------------------------------------------------------------

wire lb80_update = io_access & (~TRS_RD | ~TRS_WR);

reg [7:0] pmod_a;

always @ (posedge clk)
begin
   if(lb80_update)
      pmod_a <= TRS_A[15:8];
end

assign PMOD = {~pmod_a[7:5], ~pmod_a[1], pmod_a[4:2], pmod_a[0]};
//assign PMOD = {pmod_a[4:2], pmod_a[0], pmod_a[7:5], pmod_a[1]};


//----XFLASH---------------------------------------------------------------------

// SPI Flash control register
// bit7 is CS  (active high)
// bit6 is WPN (active low)
reg [7:0] spi_ctrl_reg = 8'h00;
assign extrom_wr_en = (spi_ctrl_reg[3:2] == 2'b10);

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
