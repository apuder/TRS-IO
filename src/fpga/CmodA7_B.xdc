
## 12 MHz Clock Signal
set_property -dict {PACKAGE_PIN L17 IOSTANDARD LVCMOS33} [get_ports clk_in]
create_clock -period 83.330 -name sys_clk_pin -waveform {0.000 41.660} -add [get_ports clk_in]

## LEDs
set_property -dict {PACKAGE_PIN A17 IOSTANDARD LVCMOS33} [get_ports {led[0]}]
set_property -dict {PACKAGE_PIN C16 IOSTANDARD LVCMOS33} [get_ports {led[1]}]

## GPIO Pins
## Pins 15 and 16 should remain commented if using them as analog inputs
set_property -dict {PACKAGE_PIN M3 IOSTANDARD LVCMOS33} [get_ports TRS_INT]
set_property -dict {PACKAGE_PIN L3 IOSTANDARD LVCMOS33} [get_ports VGA_VSYNC]
set_property -dict {PACKAGE_PIN A16 IOSTANDARD LVCMOS33} [get_ports VGA_HSYNC]
set_property -dict {PACKAGE_PIN K3 IOSTANDARD LVCMOS33} [get_ports SCK]
set_property -dict {PACKAGE_PIN C15 IOSTANDARD LVCMOS33} [get_ports CS]
set_property -dict {PACKAGE_PIN H1 IOSTANDARD LVCMOS33} [get_ports MISO]
set_property -dict {PACKAGE_PIN A15 IOSTANDARD LVCMOS33} [get_ports MOSI]
set_property -dict {PACKAGE_PIN B15 IOSTANDARD LVCMOS33} [get_ports VGA_RGB]
set_property -dict {PACKAGE_PIN A14 IOSTANDARD LVCMOS33} [get_ports {ESP_S[2]}]
set_property -dict {PACKAGE_PIN J3 IOSTANDARD LVCMOS33} [get_ports {ESP_S[1]}]
set_property -dict {PACKAGE_PIN J1 IOSTANDARD LVCMOS33} [get_ports {ESP_S[0]}]
set_property -dict {PACKAGE_PIN K2 IOSTANDARD LVCMOS33} [get_ports ESP_REQ]
set_property -dict {PACKAGE_PIN L1 IOSTANDARD LVCMOS33} [get_ports ESP_DONE]
set_property -dict {PACKAGE_PIN L2 IOSTANDARD LVCMOS33} [get_ports {TRS_A[9]}]
set_property -dict {PACKAGE_PIN M1 IOSTANDARD LVCMOS33} [get_ports {TRS_A[8]}]
set_property -dict {PACKAGE_PIN N3 IOSTANDARD LVCMOS33} [get_ports {TRS_A[14]}]
set_property -dict {PACKAGE_PIN P3 IOSTANDARD LVCMOS33} [get_ports {TRS_A[11]}]
set_property -dict {PACKAGE_PIN M2 IOSTANDARD LVCMOS33} [get_ports {TRS_A[2]}]
set_property -dict {PACKAGE_PIN N1 IOSTANDARD LVCMOS33} [get_ports {TRS_A[7]}]
set_property -dict {PACKAGE_PIN N2 IOSTANDARD LVCMOS33} [get_ports {TRS_A[3]}]
set_property -dict {PACKAGE_PIN P1 IOSTANDARD LVCMOS33} [get_ports {TRS_A[1]}]
set_property -dict {PACKAGE_PIN R3 IOSTANDARD LVCMOS33} [get_ports {TRS_A[0]}]
set_property -dict {PACKAGE_PIN T3 IOSTANDARD LVCMOS33} [get_ports {TRS_A[4]}]
set_property -dict {PACKAGE_PIN R2 IOSTANDARD LVCMOS33} [get_ports {TRS_A[5]}]
set_property -dict {PACKAGE_PIN T1 IOSTANDARD LVCMOS33} [get_ports {TRS_A[6]}]
set_property -dict {PACKAGE_PIN T2 IOSTANDARD LVCMOS33} [get_ports {TRS_A[10]}]
set_property -dict {PACKAGE_PIN U1 IOSTANDARD LVCMOS33} [get_ports {TRS_A[12]}]
set_property -dict {PACKAGE_PIN W2 IOSTANDARD LVCMOS33} [get_ports {TRS_A[13]}]
set_property -dict {PACKAGE_PIN V2 IOSTANDARD LVCMOS33} [get_ports {TRS_A[15]}]
set_property -dict {PACKAGE_PIN W3 IOSTANDARD LVCMOS33} [get_ports {TRS_D[4]}]
set_property -dict {PACKAGE_PIN V3 IOSTANDARD LVCMOS33} [get_ports {TRS_D[7]}]
set_property -dict {PACKAGE_PIN W5 IOSTANDARD LVCMOS33} [get_ports {TRS_D[1]}]
set_property -dict {PACKAGE_PIN V4 IOSTANDARD LVCMOS33} [get_ports {TRS_D[6]}]
set_property -dict {PACKAGE_PIN U4 IOSTANDARD LVCMOS33} [get_ports {TRS_D[3]}]
set_property -dict {PACKAGE_PIN V5 IOSTANDARD LVCMOS33} [get_ports {TRS_D[5]}]
set_property -dict {PACKAGE_PIN W4 IOSTANDARD LVCMOS33} [get_ports {TRS_D[0]}]
set_property -dict {PACKAGE_PIN U5 IOSTANDARD LVCMOS33} [get_ports {TRS_D[2]}]
set_property -dict {PACKAGE_PIN U2 IOSTANDARD LVCMOS33} [get_ports TRS_WR]
set_property -dict {PACKAGE_PIN W6 IOSTANDARD LVCMOS33} [get_ports TRS_RD]
set_property -dict {PACKAGE_PIN U3 IOSTANDARD LVCMOS33} [get_ports TRS_OUT]
set_property -dict {PACKAGE_PIN U7 IOSTANDARD LVCMOS33} [get_ports TRS_IN]
set_property -dict {PACKAGE_PIN W7 IOSTANDARD LVCMOS33} [get_ports WAIT]
set_property -dict {PACKAGE_PIN U8 IOSTANDARD LVCMOS33} [get_ports TRS_DIR]
set_property -dict {PACKAGE_PIN V8 IOSTANDARD LVCMOS33} [get_ports TRS_OE]

## Pmod Header JA
set_property -dict { PACKAGE_PIN N18   IOSTANDARD LVCMOS33 } [get_ports { VGA_H }];
set_property -dict { PACKAGE_PIN L18   IOSTANDARD LVCMOS33 } [get_ports { VGA_V }];
set_property -dict { PACKAGE_PIN H19   IOSTANDARD LVCMOS33 } [get_ports { VGA_R }];
set_property -dict { PACKAGE_PIN J19   IOSTANDARD LVCMOS33 } [get_ports { VGA_G }];
set_property -dict { PACKAGE_PIN K18   IOSTANDARD LVCMOS33 } [get_ports { VGA_B }];

## Configuration Flash
set_property -dict { PACKAGE_PIN K19   IOSTANDARD LVCMOS33 } [get_ports { SPI_CS_N }];
set_property -dict { PACKAGE_PIN E19   IOSTANDARD LVCMOS33 } [get_ports { SPI_SCK }];
set_property -dict { PACKAGE_PIN D18   IOSTANDARD LVCMOS33 } [get_ports { SPI_SDO }];
set_property -dict { PACKAGE_PIN D19   IOSTANDARD LVCMOS33 } [get_ports { SPI_SDI }];
set_property -dict { PACKAGE_PIN G18   IOSTANDARD LVCMOS33 } [get_ports { SPI_WP_N }];
set_property -dict { PACKAGE_PIN F18   IOSTANDARD LVCMOS33 } [get_ports { SPI_HLD_N }];

set_property CONFIG_MODE SPIx4 [current_design]
set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]
set_property BITSTREAM.CONFIG.CONFIGRATE 33 [current_design]
