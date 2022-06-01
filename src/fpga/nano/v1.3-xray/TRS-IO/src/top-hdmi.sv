module top (
  input logic clk_in,
  output logic [2:0] tmds_p,
  output logic [2:0] tmds_n,
  output logic tmds_clock_p,
  output logic tmds_clock_n
);

//logic [2:0] tmds;
//logic tmds_clock;

logic clk_pixel;
logic clk_pixel_x5;
logic clk_audio;
logic reset = 1'b0;

Gowin_rPLL0 pll0(
  .clkout(clk_pixel_x5), //output clkout
  .clkin(clk_in) //input clkin
);

Gowin_CLKDIV0 clkdiv0(
  .clkout(clk_pixel), //output clkout
  .hclkin(clk_pixel_x5), //input hclkin
  .resetn(1'b1) //input resetn
);

//pll pll(.c0(clk_pixel_x5), .c1(clk_pixel), .c2(clk_audio));

logic [15:0] audio_sample_word [1:0] = '{16'd0, 16'd0};
always @(posedge clk_audio)
  audio_sample_word <= '{audio_sample_word[1] + 16'd1, audio_sample_word[0] - 16'd1};

logic [23:0] rgb = 24'd0;
logic [9:0] cx, cy, screen_start_x, screen_start_y, frame_width, frame_height, screen_width, screen_height;
// Border test (left = red, top = green, right = blue, bottom = blue, fill = black)
always @(posedge clk_pixel)
  rgb <= {cx == 0 ? ~8'd0 : 8'd0, cy == 0 ? ~8'd0 : 8'd0, cx == screen_width - 1'd1 || cy == screen_width - 1'd1 ? ~8'd0 : 8'd0};

// 640x480 @ 59.94Hz
hdmi #(.VIDEO_ID_CODE(1), .VIDEO_REFRESH_RATE(59.94), .AUDIO_RATE(48000), .AUDIO_BIT_WIDTH(16)) hdmi(
  .clk_pixel_x5(clk_pixel_x5),
  .clk_pixel(clk_pixel),
  .clk_audio(clk_audio),
  .reset(reset),
  .rgb(rgb),
  .audio_sample_word(audio_sample_word),
  .tmds(tmds_p),
  .tmds_clock(),
  .cx(cx),
  .cy(cy),
  .frame_width(frame_width),
  .frame_height(frame_height),
  .screen_width(screen_width),
  .screen_height(screen_height)
);

assign tmds_n = ~tmds_p;
assign tmds_clock_p = clk_pixel;
assign tmds_clock_n = ~clk_pixel;

endmodule
