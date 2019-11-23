### gal

This is the program that is run on the ATF16V8B GAL.  There are two versions, one with FreHD support and one without.  The PLD is the source code that gets compiled into the JED file using a compiler.  The JED is then programmed into the GAL using a chip programmer.  You only need the appropriate JED file to program the GAL, unless you want to hack TRS-IO by changing the circuit logic.

**TRS-IO.jed**:  Use this version if you want to use the built in FreHD support.  

**TRS-IO_noFreHD.jed**: Use this version if you want to use TRS-IO on the same bus with an existing hard drive, such as an OEM Radio Shack Hard Drive, a [M3SE](http://bartlettlabs.com/M3SE/index.html), a [Lo-tech IDE](https://www.lo-tech.co.uk/wiki/Lo-tech_TRS-80_IDE_Adapter_rev.2) or a standlone [FreHD](http://www.frehd.com).  You cannot use the built-in FreHD in this configuration.

Use a chip programmer to program the ATF16V8B with the desired JED file.  The [TL866](http://www.autoelectric.cn/en/tl866_main.html) works fine for this purpose.

If you want to modify the GAL program, you can get the free [WinCUPL](https://www.microchip.com/design-centers/programmable-logic/spld-cpld/tools/software/wincupl) to compile the desired PLD into a JED file.
