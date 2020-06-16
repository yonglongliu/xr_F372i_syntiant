- Integrate the kernel driver into Linux Kernel source tree

- When doing initial integration, we suggest to keep SPI speed to 1MHz (in ndp10x_config.h) and use SPI_READ_DELAY=3.
  Once initial integration is complete and confirmed working, you can increase the speed to e.g. 8MHz
  
Sample DTS section:to place under relevant SPI controller section.
NDP101-spi {
  reg = <0>;
  compatible = "ndp10x_spi_driver";
  interrupt-parent = <&msm_gpio>;
  interrupts = <65 1>;
  spi-max-frequency = <8000000>;
};

The driver includes rudimentary support for configuring clocks via DTS settings through 'clk-name', 'clk-parent', 'clk-output-enable' & 'clk-freq' nodes, please refer to the implementation in ndp10x_config_ext_clk() in ndp10x_driver.c file for the specifics.

