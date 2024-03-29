EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L TRSIO-rescue:74LS245 U2
U 1 1 5C59F91B
P 4450 5200
F 0 "U2" H 4550 5775 50  0000 L BNN
F 1 "74LVC245" H 4500 4625 50  0000 L TNN
F 2 "Housings_DIP:DIP-20_W7.62mm" H 4450 5200 50  0001 C CNN
F 3 "" H 4450 5200 50  0001 C CNN
	1    4450 5200
	1    0    0    -1  
$EndComp
Entry Wire Line
	3400 4600 3500 4700
Entry Wire Line
	3400 4700 3500 4800
Entry Wire Line
	3400 4800 3500 4900
Entry Wire Line
	3400 4900 3500 5000
Entry Wire Line
	3400 5000 3500 5100
Entry Wire Line
	3400 5100 3500 5200
Entry Wire Line
	3400 5200 3500 5300
Entry Wire Line
	3400 5300 3500 5400
Entry Wire Line
	3400 5500 3500 5600
Text Label 3550 4700 0    60   ~ 0
D7
Text Label 3550 4800 0    60   ~ 0
D6
Text Label 3550 4900 0    60   ~ 0
D5
Text Label 3550 5000 0    60   ~ 0
D4
Text Label 3550 5100 0    60   ~ 0
D3
Text Label 3550 5200 0    60   ~ 0
D2
Text Label 3550 5300 0    60   ~ 0
D1
Text Label 3550 5400 0    60   ~ 0
D0
Text Label 3550 5600 0    60   ~ 0
/IN
Entry Wire Line
	3400 6250 3500 6350
Entry Wire Line
	3400 6350 3500 6450
Entry Wire Line
	3400 6450 3500 6550
Entry Wire Line
	3400 6550 3500 6650
Entry Wire Line
	3400 6650 3500 6750
Entry Wire Line
	3400 6750 3500 6850
Entry Wire Line
	3400 6850 3500 6950
Entry Wire Line
	3400 6950 3500 7050
Text Label 3550 6350 0    60   ~ 0
A7
Text Label 3550 6450 0    60   ~ 0
A6
Text Label 3550 6550 0    60   ~ 0
A5
Text Label 3550 6650 0    60   ~ 0
A4
Text Label 3550 6750 0    60   ~ 0
A3
Text Label 3550 6850 0    60   ~ 0
A2
Text Label 3550 6950 0    60   ~ 0
A1
Text Label 3550 7050 0    60   ~ 0
A0
Entry Wire Line
	5450 6250 5550 6150
Entry Wire Line
	5450 6350 5550 6250
Entry Wire Line
	5450 6450 5550 6350
Text Label 5200 6250 0    60   ~ 0
/IORQ
Text Label 5250 6350 0    60   ~ 0
/OUT
Text Label 5300 6450 0    60   ~ 0
/IN
Text GLabel 5100 6950 2    60   Input ~ 0
/ESP32_WAIT
Text GLabel 5900 6850 2    60   Input ~ 0
/ESP_SEL
$Comp
L conn:Conn_02x25_Odd_Even J1
U 1 1 5C5A156A
P 2350 4700
F 0 "J1" H 2400 6000 50  0000 C CNN
F 1 "TRS80 " H 2400 3400 50  0000 C CNN
F 2 "TRSIO:Pin_Header_Straight_2x25_Pitch2.54mm" H 2350 4700 50  0001 C CNN
F 3 "" H 2350 4700 50  0001 C CNN
	1    2350 4700
	-1   0    0    1   
$EndComp
Entry Wire Line
	3300 3500 3400 3400
Entry Wire Line
	3300 3600 3400 3500
Entry Wire Line
	3300 3700 3400 3600
Entry Wire Line
	3300 3800 3400 3700
Text Label 3000 3500 0    60   ~ 0
/IORQ
Text Label 3100 3600 0    60   ~ 0
/M1
Text Label 3150 3700 0    60   ~ 0
NC
Text Label 2800 3800 0    60   ~ 0
/EXTIOSEL
Entry Wire Line
	3300 4000 3400 3900
Entry Wire Line
	3300 4100 3400 4000
Entry Wire Line
	3300 4200 3400 4100
Text Label 3000 4000 0    60   ~ 0
/IOINT
Text Label 2950 4100 0    60   ~ 0
/RESET
Text Label 3050 4200 0    60   ~ 0
/OUT
Entry Wire Line
	3300 4300 3400 4200
Entry Wire Line
	3300 4400 3400 4300
Entry Wire Line
	3300 4500 3400 4400
Entry Wire Line
	3300 4600 3400 4500
Entry Wire Line
	3300 4700 3400 4600
Text Label 3150 4300 0    60   ~ 0
/IN
Text Label 3200 4400 0    60   ~ 0
A7
Text Label 3200 4500 0    60   ~ 0
A6
Text Label 3200 4600 0    60   ~ 0
A5
Text Label 3200 4700 0    60   ~ 0
A4
Entry Wire Line
	3300 4800 3400 4700
Entry Wire Line
	3300 4900 3400 4800
Entry Wire Line
	3300 5000 3400 4900
Entry Wire Line
	3300 5100 3400 5000
Entry Wire Line
	3300 5200 3400 5100
Entry Wire Line
	3300 5300 3400 5200
Entry Wire Line
	3300 5400 3400 5300
Text Label 3200 4800 0    60   ~ 0
A3
Text Label 3200 4900 0    60   ~ 0
A2
Text Label 3200 5000 0    60   ~ 0
A1
Text Label 3200 5100 0    60   ~ 0
A0
Text Label 3200 5200 0    60   ~ 0
D7
Text Label 3200 5300 0    60   ~ 0
D6
Text Label 3200 5400 0    60   ~ 0
D5
Entry Wire Line
	3300 5500 3400 5400
Entry Wire Line
	3300 5600 3400 5500
Entry Wire Line
	3300 5700 3400 5600
Entry Wire Line
	3300 5800 3400 5700
Entry Wire Line
	3300 5900 3400 5800
Text Label 3200 5500 0    60   ~ 0
D4
Text Label 3200 5600 0    60   ~ 0
D3
Text Label 3200 5700 0    60   ~ 0
D2
Text Label 3200 5800 0    60   ~ 0
D1
Text Label 3200 5900 0    60   ~ 0
D0
Entry Wire Line
	3300 3900 3400 3800
Text Label 3050 3900 0    60   ~ 0
/IOWAIT
Text GLabel 5900 6650 2    60   Input ~ 0
/WAIT
Text GLabel 5100 6750 2    60   Input ~ 0
/ESP_EXTIOSEL
$Comp
L power:GND #PWR01
U 1 1 5C5A99D4
P 2050 6200
F 0 "#PWR01" H 2050 5950 50  0001 C CNN
F 1 "GND" H 2050 6050 50  0000 C CNN
F 2 "" H 2050 6200 50  0001 C CNN
F 3 "" H 2050 6200 50  0001 C CNN
	1    2050 6200
	1    0    0    -1  
$EndComp
Text GLabel 9750 4650 2    60   Input ~ 0
/ESP32_IN
Text GLabel 10400 4750 2    60   Input ~ 0
/ESP32_SEL
Text GLabel 3750 5900 2    60   Input ~ 0
/ESP_SEL
$Comp
L Device:C_Small C1
U 1 1 5C5B2B28
P 2250 7100
F 0 "C1" H 2260 7170 50  0000 L CNN
F 1 "0.1" H 2260 7020 50  0000 L CNN
F 2 "Capacitors_THT:C_Disc_D3.0mm_W2.0mm_P2.50mm" H 2250 7100 50  0001 C CNN
F 3 "" H 2250 7100 50  0001 C CNN
	1    2250 7100
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C4
U 1 1 5C5B2C13
P 2450 7100
F 0 "C4" H 2460 7170 50  0000 L CNN
F 1 "0.1" H 2460 7020 50  0000 L CNN
F 2 "Capacitors_THT:C_Disc_D3.0mm_W2.0mm_P2.50mm" H 2450 7100 50  0001 C CNN
F 3 "" H 2450 7100 50  0001 C CNN
	1    2450 7100
	1    0    0    -1  
$EndComp
$Comp
L Device:C_Small C3
U 1 1 5C5B2C7A
P 2650 7100
F 0 "C3" H 2660 7170 50  0000 L CNN
F 1 "0.1" H 2660 7020 50  0000 L CNN
F 2 "Capacitors_THT:C_Disc_D3.0mm_W2.0mm_P2.50mm" H 2650 7100 50  0001 C CNN
F 3 "" H 2650 7100 50  0001 C CNN
	1    2650 7100
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR02
U 1 1 5C5B334E
P 2450 7300
F 0 "#PWR02" H 2450 7050 50  0001 C CNN
F 1 "GND" H 2450 7150 50  0000 C CNN
F 2 "" H 2450 7300 50  0001 C CNN
F 3 "" H 2450 7300 50  0001 C CNN
	1    2450 7300
	1    0    0    -1  
$EndComp
$Comp
L transistors:2N7000 Q1
U 1 1 5C5B476F
P 9700 1200
F 0 "Q1" H 9900 1275 50  0000 L CNN
F 1 "2N7000" H 9900 1200 50  0000 L CNN
F 2 "TO_SOT_Packages_THT:TO-92_Molded_Narrow" H 9900 1125 50  0001 L CIN
F 3 "" H 9700 1200 50  0001 L CNN
	1    9700 1200
	-1   0    0    -1  
$EndComp
Text GLabel 10050 1200 2    60   Input ~ 0
/ESP32_INT
Text GLabel 9450 850  0    60   Input ~ 0
/IOINT
$Comp
L power:GND #PWR03
U 1 1 5C5B7AF3
P 9600 1500
F 0 "#PWR03" H 9600 1250 50  0001 C CNN
F 1 "GND" H 9600 1350 50  0000 C CNN
F 2 "" H 9600 1500 50  0001 C CNN
F 3 "" H 9600 1500 50  0001 C CNN
	1    9600 1500
	1    0    0    -1  
$EndComp
$Comp
L HiLetgoESP32s:ESP32S U1A1
U 1 1 5C5B94F4
P 6850 1850
F 0 "U1A1" H 6850 800 60  0000 C CNN
F 1 "ESP32S" H 6850 2900 60  0000 C CNN
F 2 "TRSIO:DIP-38_W22.9mm" H 6800 1350 60  0001 C CNN
F 3 "" H 6800 1350 60  0001 C CNN
	1    6850 1850
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR04
U 1 1 5C5B9671
P 6850 3250
F 0 "#PWR04" H 6850 3000 50  0001 C CNN
F 1 "GND" H 6850 3100 50  0000 C CNN
F 2 "" H 6850 3250 50  0001 C CNN
F 3 "" H 6850 3250 50  0001 C CNN
	1    6850 3250
	1    0    0    -1  
$EndComp
$Comp
L power:+5V #PWR05
U 1 1 5C5B9AE4
P 5700 850
F 0 "#PWR05" H 5700 700 50  0001 C CNN
F 1 "+5V" H 5700 990 50  0000 C CNN
F 2 "" H 5700 850 50  0001 C CNN
F 3 "" H 5700 850 50  0001 C CNN
	1    5700 850 
	1    0    0    -1  
$EndComp
Text GLabel 5250 4700 2    60   Input ~ 0
ESP32_D7
Text GLabel 5250 4900 2    60   Input ~ 0
ESP32_D5
Text GLabel 5250 5100 2    60   Input ~ 0
ESP32_D3
Text GLabel 5250 5300 2    60   Input ~ 0
ESP32_D1
Text GLabel 5900 4800 2    60   Input ~ 0
ESP32_D6
Text GLabel 5900 5000 2    60   Input ~ 0
ESP32_D4
Text GLabel 5900 5200 2    60   Input ~ 0
ESP32_D2
Text GLabel 5900 5400 2    60   Input ~ 0
ESP32_D0
Text GLabel 5600 2150 0    60   Input ~ 0
ESP32_D0
Text GLabel 5600 2350 0    60   Input ~ 0
ESP32_D1
Text GLabel 4950 2050 0    60   Input ~ 0
ESP32_D2
Text GLabel 7800 2450 2    60   Input ~ 0
ESP32_D3
Text GLabel 7800 2050 2    60   Input ~ 0
ESP32_D4
Text GLabel 8350 1950 2    60   Input ~ 0
ESP32_D5
Text GLabel 8350 1750 2    60   Input ~ 0
ESP32_D6
Text GLabel 7800 1650 2    60   Input ~ 0
ESP32_D7
Text GLabel 8350 1050 2    60   Input ~ 0
/ESP32_SEL
Text GLabel 5600 1150 0    60   Input ~ 0
/ESP32_IN
Text GLabel 5600 1950 0    60   Input ~ 0
/ESP32_WAIT
Text GLabel 5600 1750 0    60   Input ~ 0
/ESP32_INT
$Comp
L switches:SW_Push SW1
U 1 1 5C5CC661
P 8350 3400
F 0 "SW1" H 8400 3500 50  0000 L CNN
F 1 "SW_Push" H 8350 3340 50  0001 C CNN
F 2 "Buttons_Switches_THT:SW_PUSH_6mm_h9.5mm" H 8350 3600 50  0001 C CNN
F 3 "" H 8350 3600 50  0001 C CNN
	1    8350 3400
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR06
U 1 1 5C5CDB99
P 4900 4050
F 0 "#PWR06" H 4900 3800 50  0001 C CNN
F 1 "GND" H 4900 3900 50  0000 C CNN
F 2 "" H 4900 4050 50  0001 C CNN
F 3 "" H 4900 4050 50  0001 C CNN
	1    4900 4050
	1    0    0    -1  
$EndComp
Text GLabel 7800 1850 2    60   Input ~ 0
LED_RED
Text GLabel 8350 2150 2    60   Input ~ 0
LED_GREEN
Text GLabel 6050 3700 2    60   Input ~ 0
LED_RED
Text GLabel 6050 3900 2    60   Input ~ 0
LED_GREEN
$Comp
L Device:R R1
U 1 1 5C5CEC7E
P 5800 3700
F 0 "R1" V 5880 3700 50  0000 C CNN
F 1 "100" V 5800 3700 50  0000 C CNN
F 2 "Resistors_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal" V 5730 3700 50  0001 C CNN
F 3 "" H 5800 3700 50  0001 C CNN
	1    5800 3700
	0    1    1    0   
$EndComp
$Comp
L Device:R R2
U 1 1 5C5CEE27
P 5800 3900
F 0 "R2" V 5880 3900 50  0000 C CNN
F 1 "100" V 5800 3900 50  0000 C CNN
F 2 "Resistors_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal" V 5730 3900 50  0001 C CNN
F 3 "" H 5800 3900 50  0001 C CNN
	1    5800 3900
	0    1    1    0   
$EndComp
$Comp
L Device:R R3
U 1 1 5C5CEE9B
P 5800 4100
F 0 "R3" V 5880 4100 50  0000 C CNN
F 1 "100" V 5800 4100 50  0000 C CNN
F 2 "Resistors_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal" V 5730 4100 50  0001 C CNN
F 3 "" H 5800 4100 50  0001 C CNN
	1    5800 4100
	0    1    1    0   
$EndComp
Text GLabel 6050 4100 2    60   Input ~ 0
LED_BLUE
Text GLabel 7800 1450 2    60   Input ~ 0
LED_BLUE
Text GLabel 3600 2850 2    60   Input ~ 0
/IOWAIT
Entry Wire Line
	3400 2750 3500 2850
Text GLabel 3600 3000 2    60   Input ~ 0
/EXTIOSEL
Text GLabel 3600 3150 2    60   Input ~ 0
/IOINT
Text GLabel 3600 3300 2    60   Input ~ 0
/IN
Entry Wire Line
	3400 2900 3500 3000
Entry Wire Line
	3400 3050 3500 3150
Entry Wire Line
	3400 3200 3500 3300
Text Notes 7000 6900 0    60   ~ 0
Reconfiguration of RetroStoreCard to support TRS-HD Emulation\n\nPrepared by Andrew Quinn
Text Notes 8150 7650 0    60   ~ 0
4 December 2019
Text Notes 10600 7650 0    60   ~ 0
2
Text Notes 7400 7500 0    60   ~ 0
TRS-IO Version 1.1
$Comp
L TRSIO-rescue:74LS245 U4
U 1 1 5C5A80B1
P 8900 5150
F 0 "U4" H 9000 5725 50  0000 L BNN
F 1 "74LVC245" H 8950 4575 50  0000 L TNN
F 2 "Housings_DIP:DIP-20_W7.62mm" H 8900 5150 50  0001 C CNN
F 3 "" H 8900 5150 50  0001 C CNN
	1    8900 5150
	1    0    0    -1  
$EndComp
Text GLabel 7950 4650 0    60   Input ~ 0
/IN
Text GLabel 7700 4750 0    60   Input ~ 0
/ESP_SEL
Text GLabel 7950 4850 0    60   Input ~ 0
A3
Text GLabel 7700 4950 0    60   Input ~ 0
A2
Text GLabel 7950 5050 0    60   Input ~ 0
A1
Text GLabel 7700 5150 0    60   Input ~ 0
A0
Text GLabel 9750 4850 2    60   Input ~ 0
ESP32_A3
Text GLabel 10400 4950 2    60   Input ~ 0
ESP32_A2
Text GLabel 9750 5050 2    60   Input ~ 0
ESP32_A1
Text GLabel 10400 5150 2    60   Input ~ 0
ESP32_A0
Text GLabel 3600 3450 2    60   Input ~ 0
A0
Entry Wire Line
	3400 3350 3500 3450
Text GLabel 3600 3600 2    60   Input ~ 0
A1
Entry Wire Line
	3400 3500 3500 3600
Text GLabel 3600 3750 2    60   Input ~ 0
A2
Entry Wire Line
	3400 3650 3500 3750
Text GLabel 3600 3900 2    60   Input ~ 0
A3
Entry Wire Line
	3400 3800 3500 3900
Text GLabel 5600 1550 0    60   Input ~ 0
ESP32_A0
Text GLabel 5000 1650 0    60   Input ~ 0
ESP32_A1
Text GLabel 5600 1350 0    60   Input ~ 0
ESP32_A2
Text GLabel 5000 1450 0    60   Input ~ 0
ESP32_A3
$Comp
L power:GND #PWR07
U 1 1 5CCC1B0A
P 7750 5800
F 0 "#PWR07" H 7750 5550 50  0001 C CNN
F 1 "GND" H 7750 5650 50  0000 C CNN
F 2 "" H 7750 5800 50  0001 C CNN
F 3 "" H 7750 5800 50  0001 C CNN
	1    7750 5800
	1    0    0    -1  
$EndComp
$Comp
L transistors:2N7000 Q3
U 1 1 5CCC1D78
P 9700 3050
F 0 "Q3" H 9900 3125 50  0000 L CNN
F 1 "2N7000" H 9900 3050 50  0000 L CNN
F 2 "TO_SOT_Packages_THT:TO-92_Molded_Narrow" H 9900 2975 50  0001 L CIN
F 3 "" H 9700 3050 50  0001 L CNN
	1    9700 3050
	-1   0    0    -1  
$EndComp
Text GLabel 10050 3050 2    60   Input ~ 0
/ESP_EXTIOSEL
Text GLabel 9450 2700 0    60   Input ~ 0
/EXTIOSEL
$Comp
L power:GND #PWR08
U 1 1 5CCC1D80
P 9600 3350
F 0 "#PWR08" H 9600 3100 50  0001 C CNN
F 1 "GND" H 9600 3200 50  0000 C CNN
F 2 "" H 9600 3350 50  0001 C CNN
F 3 "" H 9600 3350 50  0001 C CNN
	1    9600 3350
	1    0    0    -1  
$EndComp
$Comp
L transistors:2N7000 Q2
U 1 1 5CCC1E34
P 9700 2100
F 0 "Q2" H 9900 2175 50  0000 L CNN
F 1 "2N7000" H 9900 2100 50  0000 L CNN
F 2 "TO_SOT_Packages_THT:TO-92_Molded_Narrow" H 9900 2025 50  0001 L CIN
F 3 "" H 9700 2100 50  0001 L CNN
	1    9700 2100
	-1   0    0    -1  
$EndComp
Text GLabel 10050 2100 2    60   Input ~ 0
/WAIT
Text GLabel 9450 1750 0    60   Input ~ 0
/IOWAIT
$Comp
L power:GND #PWR09
U 1 1 5CCC1E3C
P 9600 2400
F 0 "#PWR09" H 9600 2150 50  0001 C CNN
F 1 "GND" H 9600 2250 50  0000 C CNN
F 2 "" H 9600 2400 50  0001 C CNN
F 3 "" H 9600 2400 50  0001 C CNN
	1    9600 2400
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR010
U 1 1 5CCD38E3
P 5950 850
F 0 "#PWR010" H 5950 700 50  0001 C CNN
F 1 "+3.3V" H 5950 990 50  0000 C CNN
F 2 "" H 5950 850 50  0001 C CNN
F 3 "" H 5950 850 50  0001 C CNN
	1    5950 850 
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR011
U 1 1 5CCD3925
P 8900 4600
F 0 "#PWR011" H 8900 4450 50  0001 C CNN
F 1 "+3.3V" H 8900 4740 50  0000 C CNN
F 2 "" H 8900 4600 50  0001 C CNN
F 3 "" H 8900 4600 50  0001 C CNN
	1    8900 4600
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR012
U 1 1 5CCD4354
P 8900 5700
F 0 "#PWR012" H 8900 5450 50  0001 C CNN
F 1 "GND" H 8900 5550 50  0000 C CNN
F 2 "" H 8900 5700 50  0001 C CNN
F 3 "" H 8900 5700 50  0001 C CNN
	1    8900 5700
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR013
U 1 1 5CCD44AA
P 4450 5750
F 0 "#PWR013" H 4450 5500 50  0001 C CNN
F 1 "GND" H 4450 5600 50  0000 C CNN
F 2 "" H 4450 5750 50  0001 C CNN
F 3 "" H 4450 5750 50  0001 C CNN
	1    4450 5750
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR014
U 1 1 5CCD4A45
P 4450 4650
F 0 "#PWR014" H 4450 4500 50  0001 C CNN
F 1 "+3.3V" H 4450 4790 50  0000 C CNN
F 2 "" H 4450 4650 50  0001 C CNN
F 3 "" H 4450 4650 50  0001 C CNN
	1    4450 4650
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR015
U 1 1 5CCD7B61
P 6700 5500
F 0 "#PWR015" H 6700 5350 50  0001 C CNN
F 1 "+3.3V" H 6700 5640 50  0000 C CNN
F 2 "" H 6700 5500 50  0001 C CNN
F 3 "" H 6700 5500 50  0001 C CNN
	1    6700 5500
	1    0    0    -1  
$EndComp
Text GLabel 5100 6550 2    60   Input ~ 0
/ESP_P31
Text GLabel 7450 5250 0    60   Input ~ 0
/ESP_P31
Wire Wire Line
	3500 4700 3750 4700
Wire Wire Line
	3500 4800 3750 4800
Wire Wire Line
	3500 4900 3750 4900
Wire Wire Line
	3500 5000 3750 5000
Wire Wire Line
	3500 5100 3750 5100
Wire Wire Line
	3500 5200 3750 5200
Wire Wire Line
	3500 5300 3750 5300
Wire Wire Line
	3500 5400 3750 5400
Wire Wire Line
	3500 5600 3750 5600
Wire Wire Line
	3500 6350 3950 6350
Wire Wire Line
	3500 6450 3950 6450
Wire Wire Line
	3500 6550 3950 6550
Wire Wire Line
	3500 6650 3950 6650
Wire Wire Line
	3500 6750 3950 6750
Wire Wire Line
	3500 6850 3950 6850
Wire Wire Line
	3500 6950 3950 6950
Wire Wire Line
	3500 7050 3950 7050
Wire Wire Line
	4950 6250 5450 6250
Wire Wire Line
	4950 6350 5450 6350
Wire Wire Line
	4950 6450 5450 6450
Wire Wire Line
	4950 6850 5900 6850
Wire Wire Line
	4950 6950 5100 6950
Wire Wire Line
	2050 3500 2050 3600
Connection ~ 2050 3600
Connection ~ 2050 3700
Connection ~ 2050 3800
Connection ~ 2050 3900
Connection ~ 2050 4000
Connection ~ 2050 4100
Connection ~ 2050 4200
Connection ~ 2050 4300
Connection ~ 2050 4400
Connection ~ 2050 4500
Connection ~ 2050 4600
Connection ~ 2050 4700
Connection ~ 2050 4800
Connection ~ 2050 4900
Connection ~ 2050 5000
Connection ~ 2050 5100
Connection ~ 2050 5200
Connection ~ 2050 5300
Connection ~ 2050 5400
Connection ~ 2050 5500
Connection ~ 2050 5600
Connection ~ 2050 5700
Connection ~ 2050 5800
Connection ~ 2050 5900
Wire Wire Line
	2550 3500 3300 3500
Wire Wire Line
	2550 3600 3300 3600
Wire Wire Line
	2550 3700 3300 3700
Wire Wire Line
	2550 3800 3300 3800
Wire Wire Line
	2550 4000 3300 4000
Wire Wire Line
	2550 4100 3300 4100
Wire Wire Line
	2550 4200 3300 4200
Wire Wire Line
	2550 4300 3300 4300
Wire Wire Line
	2550 4400 3300 4400
Wire Wire Line
	2550 4500 3300 4500
Wire Wire Line
	2550 4600 3300 4600
Wire Wire Line
	2550 4700 3300 4700
Wire Wire Line
	2550 4800 3300 4800
Wire Wire Line
	2550 4900 3300 4900
Wire Wire Line
	2550 5100 3300 5100
Wire Wire Line
	2550 5200 3300 5200
Wire Wire Line
	2550 5300 3300 5300
Wire Wire Line
	2550 5400 3300 5400
Wire Wire Line
	2550 5500 3300 5500
Wire Wire Line
	2550 5600 3300 5600
Wire Wire Line
	2550 5700 3300 5700
Wire Wire Line
	2550 5800 3300 5800
Wire Wire Line
	2550 5900 3300 5900
Wire Wire Line
	2550 3900 3300 3900
Wire Wire Line
	4950 6750 5100 6750
Wire Wire Line
	4950 6650 5900 6650
Wire Wire Line
	3750 5700 3550 5700
Wire Wire Line
	3550 5700 3550 5900
Wire Wire Line
	3550 5900 3750 5900
Wire Wire Line
	2250 7200 2250 7250
Wire Wire Line
	2250 7250 2450 7250
Wire Wire Line
	2650 7250 2650 7200
Wire Wire Line
	2450 7200 2450 7250
Connection ~ 2450 7250
Wire Wire Line
	2250 7000 2250 6950
Wire Wire Line
	2650 6950 2650 7000
Wire Wire Line
	2450 6850 2450 6950
Connection ~ 2450 6950
Wire Wire Line
	9900 1200 10050 1200
Wire Wire Line
	9450 850  9600 850 
Wire Wire Line
	9600 850  9600 1000
Wire Wire Line
	9600 1400 9600 1500
Wire Wire Line
	7450 950  7750 950 
Wire Wire Line
	7750 950  7750 1550
Wire Wire Line
	7450 1550 7750 1550
Wire Wire Line
	7750 3150 6850 3150
Wire Wire Line
	6850 3150 6850 3250
Connection ~ 7750 1550
Wire Wire Line
	6200 2250 5950 2250
Wire Wire Line
	5950 2250 5950 3150
Connection ~ 6850 3150
Wire Wire Line
	6200 950  5950 950 
Wire Wire Line
	5950 950  5950 850 
Wire Wire Line
	5700 850  5700 2750
Wire Wire Line
	5700 2750 6200 2750
Wire Wire Line
	5150 4700 5250 4700
Wire Wire Line
	5150 4900 5250 4900
Wire Wire Line
	5150 5100 5250 5100
Wire Wire Line
	5150 5300 5250 5300
Wire Wire Line
	5150 4800 5900 4800
Wire Wire Line
	5150 5000 5900 5000
Wire Wire Line
	5150 5200 5900 5200
Wire Wire Line
	5150 5400 5900 5400
Wire Wire Line
	5600 2150 6200 2150
Wire Wire Line
	5600 2350 6200 2350
Wire Wire Line
	7450 2450 7800 2450
Wire Wire Line
	7450 2050 7800 2050
Wire Wire Line
	7450 1950 8350 1950
Wire Wire Line
	7450 1050 8350 1050
Wire Wire Line
	5600 1150 6200 1150
Wire Wire Line
	5600 1950 6200 1950
Wire Wire Line
	5600 1750 6200 1750
Wire Wire Line
	8350 2150 7450 2150
Wire Wire Line
	5400 3700 5500 3700
Wire Wire Line
	5400 3900 5600 3900
Wire Wire Line
	5950 3700 6050 3700
Wire Wire Line
	5950 3900 6050 3900
Wire Wire Line
	5950 4100 6050 4100
Wire Wire Line
	7800 1450 7450 1450
Wire Wire Line
	3500 2850 3600 2850
Wire Wire Line
	3500 3000 3600 3000
Wire Wire Line
	3500 3150 3600 3150
Wire Wire Line
	3500 3300 3600 3300
Wire Wire Line
	7950 4650 8200 4650
Wire Wire Line
	7700 4750 8200 4750
Wire Wire Line
	7950 4850 8200 4850
Wire Wire Line
	7700 4950 8200 4950
Wire Wire Line
	7950 5050 8200 5050
Wire Wire Line
	7700 5150 8200 5150
Wire Wire Line
	9600 4650 9750 4650
Wire Wire Line
	9600 4750 10400 4750
Wire Wire Line
	9600 4850 9750 4850
Wire Wire Line
	9600 4950 10400 4950
Wire Wire Line
	9600 5050 9750 5050
Wire Wire Line
	9600 5150 10400 5150
Wire Wire Line
	3500 3450 3600 3450
Wire Wire Line
	3500 3600 3600 3600
Wire Wire Line
	3500 3750 3600 3750
Wire Wire Line
	3500 3900 3600 3900
Wire Wire Line
	6200 1550 5600 1550
Wire Wire Line
	6200 1350 5600 1350
Wire Wire Line
	8200 5650 7750 5650
Wire Wire Line
	2550 5000 3300 5000
Wire Wire Line
	9900 3050 10050 3050
Wire Wire Line
	9450 2700 9600 2700
Wire Wire Line
	9600 2700 9600 2850
Wire Wire Line
	9600 3250 9600 3350
Wire Wire Line
	9900 2100 10050 2100
Wire Wire Line
	9450 1750 9600 1750
Wire Wire Line
	9600 1750 9600 1900
Wire Wire Line
	9600 2300 9600 2400
Wire Wire Line
	4950 6550 5100 6550
Wire Wire Line
	8200 5250 7450 5250
Text GLabel 9750 5250 2    60   Input ~ 0
/ESP32_P31
Wire Wire Line
	9600 5250 9750 5250
Text GLabel 5000 1250 0    60   Input ~ 0
/ESP32_P31
$Comp
L Device:C_Small C2
U 1 1 5CCDC567
P 2900 7100
F 0 "C2" H 2910 7170 50  0000 L CNN
F 1 "0.1" H 2910 7020 50  0000 L CNN
F 2 "Capacitors_THT:C_Disc_D3.0mm_W2.0mm_P2.50mm" H 2900 7100 50  0001 C CNN
F 3 "" H 2900 7100 50  0001 C CNN
	1    2900 7100
	1    0    0    -1  
$EndComp
Wire Wire Line
	2900 6850 2900 7000
Wire Wire Line
	2900 7200 2900 7300
Wire Wire Line
	7750 5350 7750 5650
Wire Wire Line
	8200 5550 6700 5550
Wire Wire Line
	6700 5550 6700 5500
Wire Wire Line
	8200 5350 7750 5350
Connection ~ 7750 5650
$Comp
L power:+5V #VCC016
U 1 1 5CD113D2
P 4450 6050
F 0 "#VCC016" H 4450 5900 50  0001 C CNN
F 1 "+5V" H 4450 6190 50  0000 C CNN
F 2 "" H 4450 6050 50  0001 C CNN
F 3 "" H 4450 6050 50  0001 C CNN
	1    4450 6050
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR017
U 1 1 5CD11B70
P 4450 7450
F 0 "#PWR017" H 4450 7200 50  0001 C CNN
F 1 "GND" H 4450 7300 50  0000 C CNN
F 2 "" H 4450 7450 50  0001 C CNN
F 3 "" H 4450 7450 50  0001 C CNN
	1    4450 7450
	1    0    0    -1  
$EndComp
Wire Wire Line
	4450 6050 4450 6150
Wire Wire Line
	4450 7450 4450 7350
$Comp
L TRSIO:GAL16V8 U3
U 1 1 5CD79EB9
P 4450 6750
F 0 "U3" H 4100 7400 50  0000 L CNN
F 1 "GAL16V8" H 4500 7400 50  0000 L CNN
F 2 "Housings_DIP:DIP-20_W7.62mm" H 4450 6750 50  0001 C CNN
F 3 "" H 4450 6750 50  0001 C CNN
	1    4450 6750
	1    0    0    -1  
$EndComp
Wire Wire Line
	2250 6950 2450 6950
$Comp
L power:GND #PWR018
U 1 1 5CD7B33C
P 2900 7300
F 0 "#PWR018" H 2900 7050 50  0001 C CNN
F 1 "GND" H 2900 7150 50  0000 C CNN
F 2 "" H 2900 7300 50  0001 C CNN
F 3 "" H 2900 7300 50  0001 C CNN
	1    2900 7300
	1    0    0    -1  
$EndComp
$Comp
L power:+5V #PWR019
U 1 1 5CD7B38B
P 2900 6850
F 0 "#PWR019" H 2900 6700 50  0001 C CNN
F 1 "+5V" H 2900 6990 50  0000 C CNN
F 2 "" H 2900 6850 50  0001 C CNN
F 3 "" H 2900 6850 50  0001 C CNN
	1    2900 6850
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR020
U 1 1 5CD7B801
P 2450 6850
F 0 "#PWR020" H 2450 6700 50  0001 C CNN
F 1 "+3.3V" H 2450 6990 50  0000 C CNN
F 2 "" H 2450 6850 50  0001 C CNN
F 3 "" H 2450 6850 50  0001 C CNN
	1    2450 6850
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR021
U 1 1 5CFCD628
P 1700 850
F 0 "#PWR021" H 1700 700 50  0001 C CNN
F 1 "+3.3V" H 1700 990 50  0000 C CNN
F 2 "" H 1700 850 50  0001 C CNN
F 3 "" H 1700 850 50  0001 C CNN
	1    1700 850 
	1    0    0    -1  
$EndComp
Wire Wire Line
	1900 1050 1700 1050
Wire Wire Line
	1700 1050 1700 850 
Text GLabel 1750 1250 0    60   Input ~ 0
ESP32_D3
Text GLabel 1200 1450 0    60   Input ~ 0
LED_GREEN
Text GLabel 1750 1550 0    60   Input ~ 0
ESP32_D4
Text GLabel 1200 1650 0    60   Input ~ 0
ESP32_D5
Text GLabel 1200 1850 0    60   Input ~ 0
ESP32_D6
Text GLabel 1750 1950 0    60   Input ~ 0
ESP32_D7
Text GLabel 1200 2050 0    60   Input ~ 0
LED_BLUE
$Comp
L HiLetgoESP32s:ESP32S_AliExpress U1B1
U 1 1 5CFD771A
P 2550 1950
F 0 "U1B1" H 2550 1300 60  0000 C CNN
F 1 "ESP32_AliExpress" H 2550 3000 60  0000 C CNN
F 2 "TRSIO:DIP-30_W25.4mm" H 2500 1450 60  0001 C CNN
F 3 "" H 2500 1450 60  0001 C CNN
	1    2550 1950
	1    0    0    -1  
$EndComp
Text GLabel 1200 2450 0    60   Input ~ 0
/ESP32_SEL
$Comp
L power:+5V #PWR022
U 1 1 5CFD82CB
P 3400 850
F 0 "#PWR022" H 3400 700 50  0001 C CNN
F 1 "+5V" H 3400 990 50  0000 C CNN
F 2 "" H 3400 850 50  0001 C CNN
F 3 "" H 3400 850 50  0001 C CNN
	1    3400 850 
	1    0    0    -1  
$EndComp
Wire Wire Line
	3150 1050 3400 1050
Wire Wire Line
	3400 1050 3400 850 
Text GLabel 3250 2350 2    60   Input ~ 0
/ESP32_IN
Text GLabel 3900 2250 2    60   Input ~ 0
/ESP32_P31
Text GLabel 3250 2150 2    60   Input ~ 0
ESP32_A2
Text GLabel 3900 2050 2    60   Input ~ 0
ESP32_A3
Wire Wire Line
	3150 2050 3900 2050
Wire Wire Line
	3150 2350 3250 2350
Wire Wire Line
	3150 2150 3250 2150
Wire Wire Line
	3150 2250 3900 2250
Text GLabel 3250 1950 2    60   Input ~ 0
ESP32_A0
Wire Wire Line
	3150 1950 3250 1950
Text GLabel 3900 1850 2    60   Input ~ 0
ESP32_A1
Wire Wire Line
	3150 1850 3900 1850
Text GLabel 3250 1750 2    60   Input ~ 0
/ESP32_INT
Wire Wire Line
	3150 1750 3250 1750
Text GLabel 3250 1550 2    60   Input ~ 0
/ESP32_WAIT
Wire Wire Line
	3150 1550 3250 1550
Text GLabel 3900 1450 2    60   Input ~ 0
ESP32_D2
Wire Wire Line
	3150 1450 3900 1450
Text GLabel 3250 1350 2    60   Input ~ 0
ESP32_D0
Wire Wire Line
	3150 1350 3250 1350
Text GLabel 3800 1250 2    60   Input ~ 0
ESP32_D1
Wire Wire Line
	6200 1250 5000 1250
Wire Wire Line
	5000 1450 6200 1450
Wire Wire Line
	5000 1650 6200 1650
Wire Wire Line
	4950 2050 6200 2050
Wire Wire Line
	3150 1250 3800 1250
$Comp
L power:GND #PWR023
U 1 1 5CFDCCBD
P 3700 850
F 0 "#PWR023" H 3700 600 50  0001 C CNN
F 1 "GND" H 3700 700 50  0000 C CNN
F 2 "" H 3700 850 50  0001 C CNN
F 3 "" H 3700 850 50  0001 C CNN
	1    3700 850 
	-1   0    0    1   
$EndComp
Wire Wire Line
	3150 1150 3700 1150
Wire Wire Line
	3700 1150 3700 850 
$Comp
L power:GND #PWR024
U 1 1 5CFDD343
P 1350 850
F 0 "#PWR024" H 1350 600 50  0001 C CNN
F 1 "GND" H 1350 700 50  0000 C CNN
F 2 "" H 1350 850 50  0001 C CNN
F 3 "" H 1350 850 50  0001 C CNN
	1    1350 850 
	-1   0    0    1   
$EndComp
Wire Wire Line
	1900 1150 1350 1150
Wire Wire Line
	1350 1150 1350 850 
Wire Wire Line
	1750 1250 1900 1250
Wire Wire Line
	1750 1550 1900 1550
Wire Wire Line
	1750 1950 1900 1950
Wire Wire Line
	1200 2450 1900 2450
Wire Wire Line
	1200 2050 1900 2050
Wire Wire Line
	1200 1850 1900 1850
Wire Wire Line
	1200 1650 1900 1650
Wire Wire Line
	1200 1450 1900 1450
Text GLabel 1750 1750 0    60   Input ~ 0
LED_RED
Wire Wire Line
	1750 1750 1900 1750
Wire Wire Line
	7450 1650 7800 1650
Wire Wire Line
	7450 1750 8350 1750
Wire Wire Line
	7450 1850 7800 1850
Text GLabel 7900 3400 0    60   Input ~ 0
BUTTON
Wire Wire Line
	7900 3400 8050 3400
$Comp
L power:GND #PWR025
U 1 1 5CFDF75E
P 8800 3800
F 0 "#PWR025" H 8800 3550 50  0001 C CNN
F 1 "GND" H 8800 3650 50  0000 C CNN
F 2 "" H 8800 3800 50  0001 C CNN
F 3 "" H 8800 3800 50  0001 C CNN
	1    8800 3800
	1    0    0    -1  
$EndComp
Wire Wire Line
	8550 3400 8800 3400
Wire Wire Line
	8800 3400 8800 3700
Text GLabel 7800 1150 2    60   Input ~ 0
BUTTON
Wire Wire Line
	7450 1150 7800 1150
Text GLabel 1750 2350 0    60   Input ~ 0
BUTTON
Wire Wire Line
	1750 2350 1900 2350
$Comp
L Device:LED_RCBG D1
U 1 1 5CFE2F1A
P 5200 3900
F 0 "D1" H 5200 4270 50  0000 C CNN
F 1 "LED_RCBG" H 5200 3550 50  0000 C CNN
F 2 "LEDs:LED_D5.0mm-4" H 5200 3850 50  0001 C CNN
F 3 "" H 5200 3850 50  0001 C CNN
	1    5200 3900
	1    0    0    -1  
$EndComp
$Comp
L conn:Conn_01x02 J2
U 1 1 5D04909B
P 800 5200
F 0 "J2" H 800 5300 50  0000 C CNN
F 1 "+5V" H 800 5000 50  0000 C CNN
F 2 "Pin_Headers:Pin_Header_Straight_1x02_Pitch2.54mm" H 800 5200 50  0001 C CNN
F 3 "" H 800 5200 50  0001 C CNN
	1    800  5200
	-1   0    0    1   
$EndComp
$Comp
L power:GND #PWR026
U 1 1 5D04925C
P 1200 5300
F 0 "#PWR026" H 1200 5050 50  0001 C CNN
F 1 "GND" H 1200 5150 50  0000 C CNN
F 2 "" H 1200 5300 50  0001 C CNN
F 3 "" H 1200 5300 50  0001 C CNN
	1    1200 5300
	1    0    0    -1  
$EndComp
$Comp
L power:+5V #PWR027
U 1 1 5D0492A0
P 1200 4950
F 0 "#PWR027" H 1200 4800 50  0001 C CNN
F 1 "+5V" H 1200 5090 50  0000 C CNN
F 2 "" H 1200 4950 50  0001 C CNN
F 3 "" H 1200 4950 50  0001 C CNN
	1    1200 4950
	1    0    0    -1  
$EndComp
Wire Wire Line
	1000 5100 1200 5100
Wire Wire Line
	1200 5100 1200 4950
Wire Wire Line
	1000 5200 1200 5200
Wire Wire Line
	1200 5200 1200 5300
$Comp
L conn:Conn_01x02 J3
U 1 1 5D04AF05
P 800 6100
F 0 "J3" H 800 6200 50  0000 C CNN
F 1 "BUTTON" H 800 5900 50  0000 C CNN
F 2 "Pin_Headers:Pin_Header_Straight_1x02_Pitch2.54mm" H 800 6100 50  0001 C CNN
F 3 "" H 800 6100 50  0001 C CNN
	1    800  6100
	-1   0    0    1   
$EndComp
$Comp
L power:GND #PWR028
U 1 1 5D04AFE0
P 1200 6200
F 0 "#PWR028" H 1200 5950 50  0001 C CNN
F 1 "GND" H 1200 6050 50  0000 C CNN
F 2 "" H 1200 6200 50  0001 C CNN
F 3 "" H 1200 6200 50  0001 C CNN
	1    1200 6200
	1    0    0    -1  
$EndComp
Wire Wire Line
	1000 6100 1200 6100
Wire Wire Line
	1200 6100 1200 6200
Text GLabel 1200 6000 2    60   Input ~ 0
BUTTON
Wire Wire Line
	1000 6000 1200 6000
$Comp
L conn:Conn_01x04 J4
U 1 1 5D04CE51
P 800 7050
F 0 "J4" H 800 7250 50  0000 C CNN
F 1 "LED" H 800 6750 50  0000 C CNN
F 2 "Pin_Headers:Pin_Header_Straight_1x04_Pitch2.54mm" H 800 7050 50  0001 C CNN
F 3 "" H 800 7050 50  0001 C CNN
	1    800  7050
	-1   0    0    1   
$EndComp
$Comp
L power:GND #PWR029
U 1 1 5D04DCCE
P 1800 7250
F 0 "#PWR029" H 1800 7000 50  0001 C CNN
F 1 "GND" H 1800 7100 50  0000 C CNN
F 2 "" H 1800 7250 50  0001 C CNN
F 3 "" H 1800 7250 50  0001 C CNN
	1    1800 7250
	1    0    0    -1  
$EndComp
Wire Wire Line
	1000 7050 1800 7050
Text GLabel 1100 7150 2    60   Input ~ 0
EXT_RED
Text GLabel 1100 6850 2    60   Input ~ 0
EXT_GREEN
Text GLabel 1700 6950 2    60   Input ~ 0
EXT_BLUE
Wire Wire Line
	1000 6850 1100 6850
Wire Wire Line
	1000 6950 1700 6950
Wire Wire Line
	1000 7150 1100 7150
Wire Wire Line
	1800 7050 1800 7250
Wire Wire Line
	5000 3900 4900 3900
Wire Wire Line
	4900 3900 4900 4050
Text GLabel 5700 3550 2    60   Input ~ 0
EXT_GREEN
Text GLabel 5700 3400 2    60   Input ~ 0
EXT_BLUE
Text GLabel 5700 3250 2    60   Input ~ 0
EXT_RED
Wire Wire Line
	5700 3550 5600 3550
Wire Wire Line
	5600 3550 5600 3900
Connection ~ 5600 3900
Wire Wire Line
	5400 4100 5550 4100
Wire Wire Line
	5700 3400 5550 3400
Wire Wire Line
	5550 3400 5550 4100
Connection ~ 5550 4100
Wire Wire Line
	5700 3250 5500 3250
Wire Wire Line
	5500 3250 5500 3700
Connection ~ 5500 3700
$Comp
L switches:SW_Push SW2
U 1 1 5D0574E5
P 8350 3700
F 0 "SW2" H 8400 3800 50  0000 L CNN
F 1 "Right Angle" H 8350 3640 50  0000 C CNN
F 2 "Buttons_Switches_THT:SW_Tactile_SPST_Angled_PTS645Vx31-2LFS" H 8350 3900 50  0001 C CNN
F 3 "" H 8350 3900 50  0001 C CNN
	1    8350 3700
	1    0    0    -1  
$EndComp
Wire Wire Line
	8150 3700 8050 3700
Wire Wire Line
	8050 3700 8050 3400
Connection ~ 8050 3400
Wire Wire Line
	8550 3700 8800 3700
Connection ~ 8800 3700
$Comp
L Device:R R4
U 1 1 5DE36AF9
P 6500 6200
F 0 "R4" V 6580 6200 50  0000 C CNN
F 1 "4K7" V 6500 6200 50  0000 C CNN
F 2 "Resistors_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal" V 6430 6200 50  0001 C CNN
F 3 "" H 6500 6200 50  0001 C CNN
	1    6500 6200
	1    0    0    -1  
$EndComp
Text GLabel 3800 6200 0    60   Input ~ 0
/XHD
Wire Wire Line
	3950 6250 3900 6250
Wire Wire Line
	3900 6250 3900 6200
Wire Wire Line
	3900 6200 3800 6200
$Comp
L power:+5V #VCC030
U 1 1 5DE376C6
P 6500 5950
F 0 "#VCC030" H 6500 5800 50  0001 C CNN
F 1 "+5V" H 6500 6090 50  0000 C CNN
F 2 "" H 6500 5950 50  0001 C CNN
F 3 "" H 6500 5950 50  0001 C CNN
	1    6500 5950
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR031
U 1 1 5DE379AE
P 6550 6650
F 0 "#PWR031" H 6550 6400 50  0001 C CNN
F 1 "GND" H 6550 6500 50  0000 C CNN
F 2 "" H 6550 6650 50  0001 C CNN
F 3 "" H 6550 6650 50  0001 C CNN
	1    6550 6650
	1    0    0    -1  
$EndComp
$Comp
L conn:Conn_01x02 J5
U 1 1 5DE379E9
P 6750 6450
F 0 "J5" H 6750 6550 50  0000 C CNN
F 1 "FreHD Disable" H 7150 6450 50  0000 C CNN
F 2 "Pin_Headers:Pin_Header_Straight_1x02_Pitch2.54mm" H 6750 6450 50  0001 C CNN
F 3 "" H 6750 6450 50  0001 C CNN
	1    6750 6450
	1    0    0    -1  
$EndComp
Wire Wire Line
	6550 6550 6550 6650
Text GLabel 6350 6450 0    60   Input ~ 0
/XHD
Wire Wire Line
	6550 6450 6500 6450
Wire Wire Line
	6500 5950 6500 6050
Wire Wire Line
	6500 6350 6500 6450
Connection ~ 6500 6450
Wire Wire Line
	2050 3600 2050 3700
Wire Wire Line
	2050 3700 2050 3800
Wire Wire Line
	2050 3800 2050 3900
Wire Wire Line
	2050 3900 2050 4000
Wire Wire Line
	2050 4000 2050 4100
Wire Wire Line
	2050 4100 2050 4200
Wire Wire Line
	2050 4200 2050 4300
Wire Wire Line
	2050 4300 2050 4400
Wire Wire Line
	2050 4400 2050 4500
Wire Wire Line
	2050 4500 2050 4600
Wire Wire Line
	2050 4600 2050 4700
Wire Wire Line
	2050 4700 2050 4800
Wire Wire Line
	2050 4800 2050 4900
Wire Wire Line
	2050 4900 2050 5000
Wire Wire Line
	2050 5000 2050 5100
Wire Wire Line
	2050 5100 2050 5200
Wire Wire Line
	2050 5200 2050 5300
Wire Wire Line
	2050 5300 2050 5400
Wire Wire Line
	2050 5400 2050 5500
Wire Wire Line
	2050 5500 2050 5600
Wire Wire Line
	2050 5600 2050 5700
Wire Wire Line
	2050 5700 2050 5800
Wire Wire Line
	2050 5800 2050 5900
Wire Wire Line
	2050 5900 2050 6200
Wire Wire Line
	2450 7250 2650 7250
Wire Wire Line
	2450 7250 2450 7300
Wire Wire Line
	2450 6950 2450 7000
Wire Wire Line
	2450 6950 2650 6950
Wire Wire Line
	7750 1550 7750 3150
Wire Wire Line
	6850 3150 5950 3150
Wire Wire Line
	7750 5650 7750 5800
Wire Wire Line
	5600 3900 5650 3900
Wire Wire Line
	5550 4100 5650 4100
Wire Wire Line
	5500 3700 5650 3700
Wire Wire Line
	8050 3400 8150 3400
Wire Wire Line
	8800 3700 8800 3800
Wire Wire Line
	6500 6450 6350 6450
Wire Bus Line
	5550 6100 5550 6450
Wire Bus Line
	3400 2550 3400 7250
$EndSCHEMATC
