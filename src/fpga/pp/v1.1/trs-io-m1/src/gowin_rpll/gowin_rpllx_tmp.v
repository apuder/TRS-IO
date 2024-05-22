//Copyright (C)2014-2023 Gowin Semiconductor Corporation.
//All rights reserved.
//File Title: Template file for instantiation
//GOWIN Version: V1.9.9 Beta-4 Education
//Part Number: GW2A-LV18PG256C8/I7
//Device: GW2A-18
//Device Version: C
//Created Time: Sun Mar 24 08:57:40 2024

//Change the instance name and port connections to the signal names
//--------Copy here to design--------

    Gowin_rPLLx your_instance_name(
        .clkout(clkout_o), //output clkout
        .clkin(clkin_i), //input clkin
        .fbdsel(fbdsel_i) //input [5:0] fbdsel
    );

//--------Copy end-------------------
