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
  input TRS_RAS,
  input TRS_IN,
  input TRS_OUT,
  output reg TRS_INT,
  output reg ESP_REQ,
  output [3:0] ESP_S,
  output reg WAIT,
  input ESP_DONE,
  output LED
);

wire clk;

/*
 * Clocking Wizard
 * Clock primary: 12 MHz
 * clk_out1 frequency: 100 MHz
 */
clk_wiz_0 clk_wiz_0(
   .clk_out1(clk),
   .reset(1'b0),
   .locked(),
   .clk_in1(clk_in)
);


reg[7:0] byte_in, byte_out;
reg byte_received;

//----TRS-IO---------------------------------------------------------------------

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


reg [1:0] TRS_A15; always @(posedge clk) TRS_A15 <= {TRS_A15[0], TRS_A[15]};
wire trs_a15 = TRS_A15[1];


reg [15:0] TRS_A_r1, TRS_A_r2;
always @(posedge clk) begin
  TRS_A_r2 <= TRS_A_r1;
  TRS_A_r1 <= TRS_A;
end

wire [15:0] trs_a = TRS_A_r2;

wire trs_bram_sel_rd = TRS_A[15] && !TRS_RD;
wire trs_bram_sel_wr = TRS_A[15] && !TRS_WR;
wire trs_bram_sel = trs_bram_sel_rd || trs_bram_sel_wr;

wire fdc_37e0_sel_rd = (TRS_A == 16'h37e0) && !TRS_RD;
wire fdc_37ec_sel_rd = (TRS_A == 16'h37ec) && !TRS_RD;
wire fdc_37ef_sel_rd = (TRS_A == 16'h37ef) && !TRS_RD;
wire fdc_sel_rd = fdc_37e0_sel_rd || fdc_37ec_sel_rd || fdc_37ef_sel_rd;
wire fdc_sel = fdc_sel_rd;

wire trs_io_sel_in = (TRS_A[7:0] == 31) && !TRS_IN;
wire trs_io_sel_out = (TRS_A[7:0] == 31) && !TRS_OUT;
wire trs_io_sel = trs_io_sel_in || trs_io_sel_out;

wire frehd_sel_in = (TRS_A[7:4] == 4'hc) && !TRS_IN;
wire frehd_sel_out = (TRS_A[7:4] == 4'hc) && !TRS_OUT;
wire frehd_sel = frehd_sel_in || frehd_sel_out;

wire esp_sel = trs_io_sel || frehd_sel;

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
  if (esp_sel_risingedge)
    begin
      // When the ESP needs to do some work, assert WAIT and trigger a pulse on ESP_REQ
      ESP_REQ <= 1;
      WAIT <= 1;
      count <= 50;
    end
  else if (esp_done_risingedge)
    begin
      // When ESP is done, de-assert WAIT
      WAIT <= 0;
    end
  if (count == 1) ESP_REQ <= 0;
  if (count != 0) count <= count - 1;
end

      
localparam [3:0]
  esp_trs_io_in = 4'b0000,
  esp_trs_io_out = 4'b0001,
  esp_frehd_in = 4'b0010,
  esp_frehd_out = 4'b0011,
  esp_fdc_rd = 4'b0100;


assign ESP_S = (esp_trs_io_in & {4{trs_io_sel_in}}) |
               (esp_trs_io_out & {4{trs_io_sel_out}}) |
               (esp_frehd_in & {4{frehd_sel_in}}) |
               (esp_frehd_out & {4{frehd_sel_out}}) |
               (esp_fdc_rd & {4{fdc_sel_rd}});



//---SPI---------------------------------------------------------

reg [2:0] SCKr;  always @(posedge clk) SCKr <= {SCKr[1:0], SCK};
wire SCK_rising_edge = (SCKr[2:1] == 2'b01);
wire SCK_falling_edge = (SCKr[2:1] == 2'b10);

reg [2:0] CSr;  always @(posedge clk) CSr <= {CSr[1:0], CS};
wire CS_active = ~CSr[1];
wire CS_startmessage = (CSr[2:1]==2'b10);
wire CS_endmessage = (CSr[2:1]==2'b01);

wire start_msg = CS_startmessage;
wire end_msg = CS_endmessage;

reg [1:0] MOSIr;  always @(posedge clk) MOSIr <= {MOSIr[0], MOSI};
wire MOSI_data = MOSIr[1];


reg [2:0] bitcnt;

always @(posedge clk)
begin
  if(~CS_active)
    begin
      bitcnt <= 3'b000;
    end
  else
  if(SCK_rising_edge)
  begin
    bitcnt <= bitcnt + 3'b001;
    byte_in <= {byte_in[6:0], MOSI_data};
  end
end

always @(posedge clk) byte_received <= CS_active && SCK_rising_edge && (bitcnt == 3'b111);

reg [7:0] byte_data_sent;

always @(posedge clk)
if(CS_active)
begin
  if(SCK_falling_edge)
  begin
    if(bitcnt == 3'b000)
      byte_data_sent <= byte_out;
    else
      byte_data_sent <= {byte_data_sent[6:0], 1'b0};
  end
end

assign MISO = CS_active ? byte_data_sent[7] : 8'bz;


//---main-------------------------------------------------------------------------


reg [2:0] state;

localparam [2:0]
  idle       = 3'b000,
  read_bytes = 3'b001,
  execute    = 3'b010,
  ignore     = 3'b011;

localparam [7:0]
  set_led    = 8'b0, // Deprecated
  bram_poke  = 8'd1,
  bram_peek  = 8'd2,
  dbus_read  = 8'd3,
  dbus_write = 8'd4,
  data_ready = 8'd5;

reg [7:0] params[0:4];
reg [2:0] bytes_to_read;
reg [2:0] bytes_to_ignore;
reg [2:0] idx;
reg [7:0] cmd;
reg trs_io_data_ready = 1'b0;


initial begin
  state = idle;
end


reg trigger_action = 1'b0;

always @(posedge clk) begin
  trigger_action <= 1'b0;

  if (esp_sel_risingedge && (trs_a[7:0] == 31)) trs_io_data_ready <= 1'b0;

  if (start_msg)
    state <= idle;
  else if (byte_received) begin
    case (state)
    idle:
      begin
        trigger_action <= 1'b0;
        case (byte_in)
          set_led: begin
            cmd <= set_led;
            state <= read_bytes;
            bytes_to_read <= 3'b001;
            idx <= 3'b000;
            bytes_to_ignore <= 0;
          end
          bram_poke: begin
            cmd <= bram_poke;
            state <= read_bytes;
            bytes_to_read <= 3'b011;
            idx <= 3'b000;
            bytes_to_ignore <= 0;
          end
          bram_peek: begin
            cmd <= bram_peek;
            state <= read_bytes;
            bytes_to_read <= 3'b010;
            idx <= 3'b000;
            bytes_to_ignore <= 1;
          end
          dbus_read: begin
            cmd <= dbus_read;
            trigger_action <= 1'b1;
            bytes_to_ignore <= 1;
            state <= ignore;
          end
          dbus_write: begin
            cmd <= dbus_write;
            state <= read_bytes;
            bytes_to_read <= 3'b001;
            bytes_to_ignore <= 0;
            idx <= 3'b000;
          end
          data_ready: begin
            trs_io_data_ready <= 1'b1;
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
            if (bytes_to_ignore == 3'b000)
              state <= idle;
            else
              state <= ignore;
          end
        else
          bytes_to_read <= bytes_to_read - 3'b001;
    end
    ignore:
      begin
        if (bytes_to_ignore == 3'b001)
          state <= idle;
        else
          bytes_to_ignore <= bytes_to_ignore - 3'b001;
      end
    default:
      state <= idle;
      endcase
  end
end


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

assign TRS_OE = !((TRS_A[15] && (!TRS_WR || !TRS_RD)) || esp_sel || fdc_sel);
assign TRS_DIR = TRS_RD && TRS_IN;


reg[2:0] brama_rd; always @(posedge clk) brama_rd <= {brama_rd[1:0], RD_falling_edge && trs_a15};

wire brama_wr = WR_falling_edge && trs_a15;

wire brama_sel = (brama_rd[0] != 0) || (brama_wr != 0);

assign ena = brama_sel;
assign wea = brama_wr;
assign regcea = brama_rd[1];

reg[7:0] trs_data;
assign TRS_D = (!TRS_RD || !TRS_IN) ? trs_data : 8'bz;

/*
    org 4200h
    ld a,1
    out (197),a
    in a,(196)
    cp a,254
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
localparam [0:(24 * 8) - 1] frehd_loader = {
  8'h3e, 8'h01, 8'hd3, 8'hc5, 8'hdb, 8'hc4, 8'hbf, 8'hc2, 8'h75, 8'h00, 8'h06,
  8'h00, 8'h21, 8'h00, 8'h50, 8'hdb, 8'hc4, 8'h77, 8'h23, 8'h10, 8'hfa, 8'hc3, 8'h00, 8'h50};

reg [7:0] fdc_sector_idx;
reg [23:0] counter_25ms;

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

  if (brama_rd[2] == 1)
    trs_data <= douta;
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
      trs_data <= (fdc_sector_idx < 25) ? frehd_loader[fdc_sector_idx * 8+:8] : 0;
      fdc_sector_idx = fdc_sector_idx + 1;
    end
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


reg[2:0] bramb_rd; always @(posedge clk) bramb_rd <= {bramb_rd[1:0], trigger_action && (cmd == bram_peek)};

wire bramb_wr = trigger_action && (cmd == bram_poke);

wire bramb_sel = (bramb_rd != 0) || (bramb_wr != 0);

assign enb = bramb_sel;
assign web = bramb_wr;
assign regceb = bramb_rd[1];

always @(posedge clk) begin
  if (bramb_rd[2] == 1)
    byte_out <= doutb;
  else if ((cmd == dbus_read) && trigger_action)
    byte_out <= TRS_D;
end

always @(posedge clk) begin
  if (trigger_action) begin
    case (cmd)
      set_led: begin
        //XXX LED <= params[0][0];
      end
      default: ;
    endcase
  end
end


endmodule
