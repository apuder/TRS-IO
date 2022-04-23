`timescale 1ns / 1ps

module main(
  input clk_in,
  input SCK,
  input MOSI,
  output MISO,
  input CS,
  input [15:0] TRS_A,
  inout [7:0] TRS_D,
  output TRS_OE,
  output TRS_DIR,
  input TRS_RD,
  input TRS_WR,
  input TRS_IN,
  input TRS_OUT,
  output reg TRS_INT,
  output reg ESP_REQ,
  output [2:0] ESP_S,
  output reg WAIT,
  input ESP_DONE,
  output VGA_RGB,
  output VGA_HSYNC,
  output VGA_VSYNC,
  output VGA_R,
  output VGA_G,
  output VGA_B,
  output VGA_H,
  output VGA_V,
  output [1:0] led,
  
  output SPI_CS_N,
  output SPI_SCK,
  output SPI_SDO,
  input SPI_SDI,
  output SPI_WP_N,
  output SPI_HLD_N
);

localparam [2:0] VERSION_MAJOR = 0;
localparam [4:0] VERSION_MINOR = 2;

localparam [7:0] COOKIE = 8'haf;

wire clk;
wire vga_clk;

/*
 * Clocking Wizard
 * Clock primary: 12 MHz
 * clk_out1 frequency: 100 MHz
 * clk_out2: 20 MHz
 */
clk_wiz_0 clk_wiz_0(
   .clk_out1(clk),
   .clk_out2(vga_clk),
   .reset(1'b0),
   .locked(),
   .clk_in1(clk_in)
);


reg[7:0] byte_in, byte_out;
reg byte_received = 1'b0;

//----TRS-IO---------------------------------------------------------------------

localparam[7:0]
  PRINTER_STATUS_READY = 8'h30,
  PRINTER_STATUS_BUSY = 8'hf0;

reg[7:0] printer_status = PRINTER_STATUS_READY;

// One byte buffer for printer output
reg[7:0] printer_byte;


wire trs_wr;
wire WR_falling_edge;
wire WR_rising_edge;

filter WR_filter(
  .clk(clk),
  .in(TRS_WR),
  .out(trs_wr),
  .rising_edge(WR_rising_edge),
  .falling_edge(WR_falling_edge)
);


wire trs_rd;
wire RD_falling_edge;
wire RD_rising_edge;

filter RD_filter(
  .clk(clk),
  .in(TRS_RD),
  .out(trs_rd),
  .rising_edge(RD_rising_edge),
  .falling_edge(RD_falling_edge)
);


wire trs_out;
wire OUT_falling_edge;
wire OUT_rising_edge;

filter OUT_filter(
  .clk(clk),
  .in(TRS_OUT),
  .out(trs_out),
  .rising_edge(OUT_rising_edge),
  .falling_edge(OUT_falling_edge)
);

wire trs_in;
wire IN_falling_edge;
wire IN_rising_edge;

filter IN_filter(
  .clk(clk),
  .in(TRS_IN),
  .out(trs_in),
  .rising_edge(IN_rising_edge),
  .falling_edge(IN_falling_edge)
);


reg full_addr = 1'b0;

// rom
wire trs_rom_sel = full_addr
                 ? (~TRS_A[15] & ~TRS_A[14] & (~TRS_A[13] | ~TRS_A[12])) // 12k
                 : 1'b0;                                                 // none (original design)

// ram
wire trs_ram_sel = full_addr
                 ? (TRS_A[15] | TRS_A[14]) // full 48k
                 : TRS_A[15];              // upper 32k (original design)

// map rom and ram
wire trs_mem_sel = trs_rom_sel | trs_ram_sel;

wire trs_bram_sel_rd = trs_mem_sel && !TRS_RD;
wire trs_bram_sel_wr = trs_ram_sel && !TRS_WR;
wire trs_bram_sel = trs_bram_sel_rd || trs_bram_sel_wr;

wire fdc_37e0_sel_rd = (TRS_A == 16'h37e0) && !TRS_RD;
wire fdc_37ec_sel_rd = (TRS_A == 16'h37ec) && !TRS_RD;
wire fdc_37ef_sel_rd = (TRS_A == 16'h37ef) && !TRS_RD;
wire fdc_sel_rd = fdc_37e0_sel_rd || fdc_37ec_sel_rd || fdc_37ef_sel_rd;
wire fdc_sel = fdc_sel_rd;

wire printer_sel_rd = (TRS_A == 16'h37e8) && !TRS_RD;
wire printer_sel_wr = (TRS_A == 16'h37e8) && !TRS_WR;
wire printer_sel = printer_sel_wr;
reg printer_sel_reg = 0;

wire trs_io_sel_in = (TRS_A[7:0] == 31) && !TRS_IN;
wire trs_io_sel_out = (TRS_A[7:0] == 31) && !TRS_OUT;
wire trs_io_sel = trs_io_sel_in || trs_io_sel_out;

wire frehd_sel_in = (TRS_A[7:4] == 4'hc) && !TRS_IN;
wire frehd_sel_out = (TRS_A[7:4] == 4'hc) && !TRS_OUT;
wire frehd_sel = frehd_sel_in || frehd_sel_out;

wire z80_dsp_sel_wr = (TRS_A[15:10] == 6'b001111) && !TRS_WR;

wire z80_le18_data_sel_in  = (TRS_A[7:0] == 8'hec) & ~TRS_IN;

wire z80_spi_ctrl_sel_out = (TRS_A[7:0] == 8'hfc) & OUT_falling_edge;
wire z80_spi_data_sel_in  = (TRS_A[7:0] == 8'hfd) & ~TRS_IN;
wire z80_spi_data_sel_out = (TRS_A[7:0] == 8'hfd) & OUT_falling_edge;

wire xray_sel;

wire esp_sel = trs_io_sel || frehd_sel || printer_sel || xray_sel;

wire esp_sel_fallingedge;
wire esp_sel_risingedge;

filter esp_sel_filter(
  .clk(clk),
  .in(esp_sel),
  .out(),
  .rising_edge(esp_sel_risingedge),
  .falling_edge(esp_sel_fallingedge)
);

reg [2:0] esp_done_raw; always @(posedge clk) esp_done_raw <= {esp_done_raw[1:0], ESP_DONE};
wire esp_done_risingedge = esp_done_raw[2:1] == 2'b01;

reg [5:0] count;

always @(posedge clk) begin
  if (esp_sel_risingedge) begin
    // ESP needs to do something
    ESP_REQ <= 1;
    count <= 50;
    if (printer_sel) begin
      // The next byte for the printer is ready
      printer_sel_reg <= 1;
      printer_byte <= TRS_D;
      printer_status <= PRINTER_STATUS_BUSY;
    end
    else begin
      // This is not a write to 0x37e8 (the printer). Need to assert WAIT
      WAIT <= 1;
    end
  end
  else if (esp_done_risingedge)
    begin
      // When ESP is done, de-assert WAIT
      WAIT <= 0;
      printer_sel_reg <= 0;
      printer_status <= PRINTER_STATUS_READY;
    end
  if (count == 1) ESP_REQ <= 0;
  if (count != 0) count <= count - 1;
end

      
localparam [2:0]
  esp_trs_io_in = 3'd0,
  esp_trs_io_out = 3'd1,
  esp_frehd_in = 3'd2,
  esp_frehd_out = 3'd3,
  esp_printer_wr = 3'd4,
  esp_xray = 3'd5;


assign ESP_S = (esp_trs_io_in & {3{trs_io_sel_in}}) |
               (esp_trs_io_out & {3{trs_io_sel_out}}) |
               (esp_frehd_in & {3{frehd_sel_in}}) |
               (esp_frehd_out & {3{frehd_sel_out}}) |
               (esp_printer_wr & {3{printer_sel_reg}}) |
               (esp_xray & {3{xray_sel}});



//---main-------------------------------------------------------------------------


localparam [2:0]
  idle       = 3'b000,
  read_bytes = 3'b001,
  execute    = 3'b010;

reg [2:0] state = idle;

wire start_msg;

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
  get_printer_byte    = 8'd16;
  



reg [7:0] params[0:4];
reg [2:0] bytes_to_read;
reg [7:0] bits_to_send;
reg [2:0] idx;
reg [7:0] cmd;
reg trs_io_data_ready = 1'b0;


reg trigger_action = 1'b0;

always @(posedge clk) begin
  trigger_action <= 1'b0;
  bits_to_send <= 0;

  if (esp_sel_risingedge && (TRS_A[7:0] == 31)) trs_io_data_ready <= 1'b0;

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
            bits_to_send <= 9;
            state <= idle;
          end
          get_version: begin
            trigger_action <= 1'b1;
            bits_to_send <= 9;
            state <= idle;
          end
          bram_poke: begin
            bytes_to_read <= 3'b011;
          end
          bram_peek: begin
            bytes_to_read <= 3'b010;
            bits_to_send <= 9;
          end
          dbus_read: begin
            trigger_action <= 1'b1;
            bits_to_send <= 9;
            state <= idle;
          end
          dbus_write: begin
            bytes_to_read <= 3'b001;
          end
          data_ready: begin
            trs_io_data_ready <= 1'b1;
            state <= idle;
          end
          set_breakpoint: begin
            bytes_to_read <= 3;
          end
          clear_breakpoint: begin
            bytes_to_read <= 1;
          end
          xray_code_poke: begin
            bytes_to_read <= 2;
          end
          xray_data_poke: begin
            bytes_to_read <= 2;
          end
          xray_data_peek: begin
            bytes_to_read <= 1;
            bits_to_send <= 9;
          end
          xray_resume: begin
            trigger_action <= 1'b1;
            state <= idle;
          end
          set_full_addr: begin
            bytes_to_read <= 1;
          end
          get_printer_byte: begin
            trigger_action <= 1'b1;
            bits_to_send <= 9;
            state <= idle;
          end
          default:
            begin
              state <= idle;
            end
        endcase
      end
    read_bytes:
      begin
        params[idx] <= byte_in;
        idx <= idx + 3'b001;
        
        if (bytes_to_read == 3'b001)
          begin
            trigger_action <= 1'b1;
            state <= idle;
          end
        else
          bytes_to_read <= bytes_to_read - 3'b001;
    end
    default:
      state <= idle;
      endcase
  end
end


//---SPI---------------------------------------------------------

reg [2:0] SCKr;  always @(posedge clk) SCKr <= {SCKr[1:0], SCK};
wire SCK_rising_edge = (SCKr[2:1] == 2'b01);
wire SCK_falling_edge = (SCKr[2:1] == 2'b10);

reg [2:0] CSr;  always @(posedge clk) CSr <= {CSr[1:0], CS};
wire CS_active = ~CSr[1];
wire CS_startmessage = (CSr[2:1]==2'b10);
wire CS_endmessage = (CSr[2:1]==2'b01);

assign start_msg = CS_startmessage;
wire end_msg = CS_endmessage;

reg [1:0] MOSIr;  always @(posedge clk) MOSIr <= {MOSIr[0], MOSI};
wire MOSI_data = MOSIr[1];

reg [7:0] remaining_bits_to_send;


reg [2:0] bitcnt = 3'b000;


always @(posedge clk) begin
  if(~CS_active)
    bitcnt <= 3'b000;
  else
    if(SCK_rising_edge) begin
      bitcnt <= bitcnt + 3'b001;
      byte_in <= {byte_in[6:0], MOSI_data};
    end
end

wire need_to_read_data = ((state == idle) && (remaining_bits_to_send == 0)) || (state == read_bytes);

always @(posedge clk) byte_received <= CS_active && SCK_rising_edge && need_to_read_data && (bitcnt == 3'b111);

reg [7:0] byte_data_sent;

always @(posedge clk) begin
  if (bits_to_send != 0) remaining_bits_to_send = bits_to_send;
  if(CS_active) begin
    if(SCK_falling_edge && state == idle) begin
      if(remaining_bits_to_send == 8)
        byte_data_sent <= byte_out;
      else
        byte_data_sent <= {byte_data_sent[6:0], 1'b0};
      if (remaining_bits_to_send != 0) remaining_bits_to_send <= remaining_bits_to_send - 1;
    end
  end
end

assign MISO = CS_active ? byte_data_sent[7] : 8'bz;


//---Full Address--------------------------------------------------------------------------


always @(posedge clk) begin
  if (trigger_action && cmd == set_full_addr) begin
    full_addr <= params[0];
  end
end


//---Breakpoint Management-----------------------------------------------------------------

reg [15:0] breakpoints[0:7];
reg breakpoint_active[0:7];
reg breakpoints_enabled;
wire [7:0] breakpoint_idx;
reg [7:0] current_breakpoint_idx;


always @(posedge clk) begin
  if (trigger_action) begin
    case(cmd)
      set_breakpoint: begin
        breakpoints[params[0]] <= {params[2], params[1]};
        breakpoint_active[params[0]] <= 1;
      end
      clear_breakpoint: begin
        breakpoint_active[params[0]] <= 0;
      end
      enable_breakpoints: begin
        breakpoints_enabled <= 1;
      end
      disable_breakpoints: begin
        breakpoints_enabled <= 0;
      end
      default: ;
    endcase
  end
end

wire ram_read_access = RD_falling_edge && trs_mem_sel;
wire ram_write_access = WR_falling_edge && trs_ram_sel;

wire pre_ram_access_check;
wire do_ram_access;

trigger pre_ram_access_check_trigger(
  .clk(clk),
  .cond(ram_read_access || ram_write_access),
  .one(pre_ram_access_check),
  .two(do_ram_access),
  .three()
);

assign breakpoint_idx = (({8{(breakpoint_active[0] && (breakpoints[0] == TRS_A))}} & 8'd1) |
                        ({8{(breakpoint_active[1] && (breakpoints[1] == TRS_A))}} & 8'd2) |
                        ({8{(breakpoint_active[2] && (breakpoints[2] == TRS_A))}} & 8'd3) |
                        ({8{(breakpoint_active[3] && (breakpoints[3] == TRS_A))}} & 8'd4) |
                        ({8{(breakpoint_active[4] && (breakpoints[4] == TRS_A))}} & 8'd5) |
                        ({8{(breakpoint_active[5] && (breakpoints[5] == TRS_A))}} & 8'd6) |
                        ({8{(breakpoint_active[6] && (breakpoints[6] == TRS_A))}} & 8'd7) |
                        ({8{(breakpoint_active[7] && (breakpoints[7] == TRS_A))}} & 8'd8)) &
                        {8{~TRS_RD}};


reg [15:0] xray_base_addr;

wire [15:0] diff = TRS_A - xray_base_addr;
wire himem = ((TRS_A & 16'hff00) == 16'hff00);

wire [8:0] xaddra = {9{~himem}} & {1'b0, diff[7:0]} |
                    {9{himem}} & {1'b1, TRS_A[7:0]};


localparam [1:0]
  state_xray_run = 2'b00,
  state_xray_stop = 2'b01,
  state_xray_resume = 2'b11;

reg stub_ran_once = 1'b0;

reg [1:0] state_xray = state_xray_run;


always @(posedge clk) begin
  if (trigger_action && (cmd == xray_resume) && (state_xray == state_xray_stop)) begin
    state_xray <= state_xray_resume;
    stub_ran_once <= 1'b0;
  end
  if (pre_ram_access_check && (state_xray == state_xray_run) && (breakpoint_idx != 0)) begin
    state_xray <= state_xray_stop;
    xray_base_addr <= TRS_A;
    current_breakpoint_idx <= breakpoint_idx - 1;
    stub_ran_once <= 1'b0;
  end
  if (pre_ram_access_check && (state_xray == state_xray_resume) && (xray_base_addr == TRS_A)) begin
    state_xray <= state_xray_run;
    stub_ran_once <= 1'b0;
  end
  if (pre_ram_access_check && (state_xray == state_xray_stop) && (xray_base_addr == TRS_A)) begin
    stub_ran_once <= 1'b1;
  end
end

wire xray_run_stub = (state_xray != state_xray_run);

assign xray_sel = xray_run_stub && stub_ran_once;


//--------BRAM-------------------------------------------------------------------------

wire ena;
wire regcea;
wire [0:0]wea;
wire [15:0]addra;
wire [7:0]dina;
wire [7:0]douta;
wire clkb;
wire enb;
wire regceb;
wire [0:0]web;
wire [15:0]addrb;
wire [7:0]dinb;
wire [7:0]doutb;


/*
 * BRAM configuration
 * ------------------
 * BRAM is 64K in size and coveres the complete 16-bit address range of the Z80.
 *
 * Basics: Native interface, True dual port, Common Clock, Write Enable, Byte size: 8
 * Port A: Write/Read width: 8, Write depth: 65536, Operating mode: Write First, Core Output Register, REGCEA pin
 * Port B: Write/Read width: 8, Write depth: 65536, Operating mode: Read First, Core Output Register, REGCEB pin
 */
blk_mem_gen_0 bram(
  .clka(clk),
  .ena(ena),
  .regcea(regcea),
  .wea(wea),
  .addra(addra),
  .dina(dina),
  .douta(douta),
  .clkb(clk),
  .enb(enb),
  .regceb(regceb),
  .web(web),
  .addrb(addrb), 
  .dinb(dinb),
  .doutb(doutb)
);




assign addra = TRS_A;
assign dina = !TRS_WR ? TRS_D : 8'bz;

assign TRS_OE = !((trs_mem_sel && (!TRS_WR || !TRS_RD)) || esp_sel || fdc_sel || z80_dsp_sel_wr ||
                   printer_sel_rd || printer_sel_wr || z80_le18_data_sel_in || z80_spi_data_sel_in || !TRS_OUT);
assign TRS_DIR = TRS_RD && TRS_IN;

wire ena_read;
wire ena_write;
assign ena = ena_read || ena_write;

wire brama_data_ready;

trigger brama_read_trigger(
  .clk(clk),
  .cond(do_ram_access && !trs_rd && !xray_run_stub),
  .one(ena_read),
  .two(regcea),
  .three(brama_data_ready)
);

trigger brama_write_trigger(
  .clk(clk),
  .cond(do_ram_access && !trs_wr && !xray_run_stub),
  .one(ena_write),
  .two(),
  .three()
);

assign wea = !trs_wr;

reg[7:0] trs_data;
assign TRS_D = (!TRS_RD || !TRS_IN) ? trs_data : 8'bz;

/*
  ; Assembly of the autoboot. This will be returned when the M1 ROM reads in the
  ; boot sector from the FDC.
    org 4200h
    ld a,1
    out (197),a
    in a,(196)
    cp 254
    jp nz,0075h
    ld b,0
    ld hl,20480
LOOP:
    in a,(196)
    ld (hl),a
    inc hl
    djnz LOOP
    jp 20480
*/
localparam [0:(25 * 8) - 1] frehd_loader = {
  8'h3e, 8'h01, 8'hd3, 8'hc5, 8'hdb, 8'hc4, 8'hfe, 8'hfe, 8'hc2, 8'h75, 8'h00, 8'h06,
  8'h00, 8'h21, 8'h00, 8'h50, 8'hdb, 8'hc4, 8'h77, 8'h23, 8'h10, 8'hfa, 8'hc3, 8'h00, 8'h50};

reg [7:0] fdc_sector_idx = 8'd0;
reg [23:0] counter_25ms;

wire [7:0] xdouta;
wire xrama_data_ready;

wire [7:0] le18_dout;
wire le18_dout_rdy;

wire [7:0] spi_data_in;

always @(posedge clk) begin
  if (counter_25ms == 2500000)
    begin
      counter_25ms <= 0;
      TRS_INT <= 1;
    end
  else
    begin
      counter_25ms <= counter_25ms + 1;
    end

  if (brama_data_ready == 1)
    trs_data <= douta;
  else if (xrama_data_ready == 1)
    trs_data <= xdouta;
  else if (trigger_action && cmd == dbus_write)
    trs_data <= params[0];
  else if (RD_falling_edge && fdc_37ec_sel_rd)
    trs_data <= 2;
  else if (RD_falling_edge && fdc_37e0_sel_rd)
    begin
      trs_data <= ({8{~trs_io_data_ready}} & 8'h20) | ({8{TRS_INT}} & 8'h80);
      TRS_INT <= 0;
    end
  else if (RD_falling_edge && fdc_37ef_sel_rd)
    begin
      trs_data <= (fdc_sector_idx < 26) ? frehd_loader[fdc_sector_idx * 8+:8] : 0;
      fdc_sector_idx = fdc_sector_idx + 1;
    end
  else if (RD_falling_edge && printer_sel_rd)
    trs_data <= printer_status;
  else if (le18_dout_rdy)
    trs_data <= le18_dout;
  else if (IN_falling_edge && z80_spi_data_sel_in)
    trs_data <= spi_data_in;
end


/*
assign TRS_OE = !(TRS_A[15] && (!TRS_WR || !TRS_RD));
assign TRS_DIR = TRS_RD;
*/

/*
assign RamOEn = !(TRS_A[15] && (!TRS_WR || !TRS_RD));
assign RamWEn = TRS_WR;
assign RamCEn = 1'b0;

assign MemAdr = { 3'b000, TRS_A };

assign TRS_D = !TRS_RD ? MemDB : 8'bz;
assign MemDB = !TRS_WR ? TRS_D : 8'bz;
*/

//---BRAM-------------------------------------------------------------------------

assign addrb = {params[1], params[0]};
assign dinb = params[2];



wire xram_peek_done;
wire [7:0] xdoutb;

wire enb_peek, enb_poke;
assign enb = enb_peek || enb_poke;
assign web = (cmd == bram_poke);
wire bram_peek_done;

trigger bram_poke_trigger(
  .clk(clk),
  .cond(trigger_action && (cmd == bram_poke)),
  .one(enb_poke),
  .two(),
  .three());

trigger bram_peek_trigger(
  .clk(clk),
  .cond(trigger_action && (cmd == bram_peek)),
  .one(enb_peek),
  .two(regceb),
  .three(bram_peek_done));

always @(posedge clk) begin
  if (bram_peek_done) byte_out <= doutb;
  else if (xram_peek_done) byte_out <= xdoutb;
  else if (trigger_action && cmd == dbus_read) byte_out <= TRS_D;
  else if (trigger_action && cmd == get_cookie) byte_out <= COOKIE;
  else if (trigger_action && cmd == get_version) byte_out <= {VERSION_MAJOR, VERSION_MINOR};
  else if (trigger_action && cmd == get_printer_byte) byte_out <= printer_byte;
end



//---XRAY-------------------------------------------------------------------------

wire xena;
wire xregcea;
wire [0:0] xwea;
wire [7:0] xdina;
wire xclkb;
wire xenb;
wire xregceb;
wire [0:0] xweb;
wire [8:0] xaddrb;
wire [7:0] xdinb;


/*
 * XRAM configuration
 * ------------------
 * XRAM is 512 bytes in size. The first 256 bytes hold the debug stub (xray-stub.asm)
 * that gets injected when execution hits a breapoint. The upper 256 bytes are always
 * mapped to $FF00. This is where the debug stub stores the context (i.e., Z80 registers)
 *
 * Basics: Native interface, True dual port, Common Clock, Write Enable, Byte size: 8
 * Port A: Write/Read width: 8, Write depth: 512, Operating mode: Write First, Core Output Register, REGCEA pin
 * Port B: Write/Read width: 8, Write depth: 512, Operating mode: Read First, Core Output Register, REGCEB pin
 */
blk_mem_gen_1 xram(
  .clka(clk),
  .ena(xena),
  .regcea(xregcea),
  .wea(xwea),
  .addra(xaddra),
  .dina(xdina),
  .douta(xdouta),
  .clkb(clk),
  .enb(xenb),
  .regceb(xregceb),
  .web(xweb),
  .addrb(xaddrb), 
  .dinb(xdinb),
  .doutb(xdoutb)
);

// Port A
assign xdina = !TRS_WR ? TRS_D : 8'bz;

assign xwea = !trs_wr;

wire xena_read;
wire xena_write;
assign xena = xena_read || xena_write;

trigger xrama_read_trigger(
  .clk(clk),
  .cond(do_ram_access && !trs_rd && xray_run_stub),
  .one(xena_read),
  .two(xregcea),
  .three(xrama_data_ready)
);

trigger xrama_write_trigger(
  .clk(clk),
  .cond(do_ram_access && !trs_wr && xray_run_stub),
  .one(xena_write),
  .two(),
  .three()
);


// Port B
wire xenb_peek, xenb_poke;
assign xenb = xenb_peek || xenb_poke;
assign xaddrb = {(cmd != xray_code_poke), params[0]};
assign xdinb = params[1];
assign xweb = (cmd == xray_code_poke) || (cmd == xray_data_poke);

trigger xram_poke_trigger(
  .clk(clk),
  .cond(trigger_action && ((cmd == xray_code_poke) || (cmd == xray_data_poke))),
  .one(xenb_poke),
  .two(),
  .three());

trigger xram_peek_trigger(
  .clk(clk),
  .cond(trigger_action && (cmd == xray_data_peek)),
  .one(xenb_peek),
  .two(xregceb),
  .three(xram_peek_done));

//-----VGA-------------------------------------------------------------------------------

wire VGA_RGBx, VGA_HSYNCx, VGA_VSYNCx;

vga vga(
  .clk(clk),     // 100 MHz
  .vga_clk(vga_clk), // 20 MHz
  .TRS_A(TRS_A),
  .TRS_D(TRS_D),
  .WR_falling_edge(WR_falling_edge),
  .OUT_falling_edge(OUT_falling_edge),
  .IN_falling_edge(IN_falling_edge),
  .le18_dout(le18_dout),
  .le18_dout_rdy(le18_dout_rdy),
  .VGA_RGB(VGA_RGBx),
  .VGA_HSYNC(VGA_HSYNCx),
  .VGA_VSYNC(VGA_VSYNCx));

assign VGA_RGB   = VGA_RGBx;
assign VGA_HSYNC = VGA_HSYNCx;
assign VGA_VSYNC = VGA_VSYNCx;

assign VGA_R = VGA_RGBx;
assign VGA_G = VGA_RGBx;
assign VGA_B = VGA_RGBx;
assign VGA_H = VGA_HSYNCx;
assign VGA_V = VGA_VSYNCx;


assign led[0] = xray_run_stub;


//----XFLASH---------------------------------------------------------------------

// SPI Flash control register
// bit7 is CS  (active high)
// bit6 is WPN (active low)
reg [7:0] z80_spi_ctrl_reg = 8'h00;

always @(posedge clk)
begin
   if(z80_spi_ctrl_sel_out)
      z80_spi_ctrl_reg <= TRS_D;
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
            spi_shift_reg <= {spi_shift_reg[6:0], SPI_SDI};
      end
   end
   else if(z80_spi_data_sel_out)
   begin
      spi_shift_reg <= TRS_D;
      spi_counter <= 8'b10000000;
   end
end

assign spi_data_in = spi_shift_reg;


assign SPI_CS_N  = ~z80_spi_ctrl_reg[7];
assign SPI_SCK   = spi_counter[3];
assign SPI_SDO   =  spi_sdo;
assign SPI_WP_N  =  z80_spi_ctrl_reg[6];
assign SPI_HLD_N =  1'bz;

endmodule
