module gpsdo
(
    // Clock inputs
    input CLK1,
    // Reset
    input RESETn,
    // Interrupt Requests
    output SMC_IRQn,
    // Control SPI slave
    input SPI_SCK,
    input SPI_MOSI,
    output SPI_MISO,
    input SPI_CSn,
);

/// Clocks ///
// Inputs
wire clk1; // Fast clock

assign CLK1 = clk1;

// Module clocks
wire rst_clk;
wire irq_clk;
wire cntrl_spi_clk;

assign rst_clk              = clk1; // Reset clock is CLK1
assign irq_clk              = clk1; // IRQ clock is CLK1
assign cntrl_spi_clk        = clk1; // Control SPI interface clock is CLK1
/// Clocks ///

/// Resets ///
// Inputs
wire nrst;

assign nrst = RESETn;

// Reset tree
reg  [3:0] porst_pipe = 4'b1111;
wire       porst;
reg  [3:0] extrst_pipe = 4'b1111;
wire       extrst;

assign porst  = (|porst_pipe); // Power-on reset
assign extrst = (|extrst_pipe) | porst; // External pin reset

// Module reset lines
wire irq_rst;
wire cntrl_spi_rst;

assign irq_rst       = extrst; // IRQ is reset by external pin
assign cntrl_spi_rst = extrst; // Control SPI interface is reset by external pin

always @(posedge rst_clk)
    begin
        porst_pipe <= {porst_pipe[2:0], 1'b0};
        extrst_pipe <= {extrst_pipe[2:0], !nrst};
    end
/// Resets ///

/// Interrupt Request ///
// Config
localparam IRQ_COUNT = 1;

// Inputs
wire [IRQ_COUNT - 1:0] irq_lines;
wire [IRQ_COUNT - 1:0] irq_lines_q;
reg  [IRQ_COUNT - 1:0] irq_lines_ed;
reg  [IRQ_COUNT - 1:0] irq_state;
wire [IRQ_COUNT - 1:0] irq_mask; // Controlled by SMC via SPI
wire [IRQ_COUNT - 1:0] irq_clear; // Controlled by SMC via SPI
wire [IRQ_COUNT - 1:0] irq_set; // Controlled by SMC via SPI

assign SMC_IRQn = !(|(irq_state & irq_mask));

// IRQ lines selection
assign irq_lines[0] = 1'b0;

// Synchronizer
synchronizer irq_lines_sync [IRQ_COUNT - 1:0]
(
    .in_clk({1'b0}),
    .out_clk(irq_clk),
    .in(irq_lines),
    .out(irq_lines_q)
);

always @(posedge irq_clk)
    begin
        if(irq_rst)
            begin
                irq_lines_ed <= {IRQ_COUNT{1'b0}};
                irq_state <= {IRQ_COUNT{1'b0}};
            end
        else
            begin
                irq_lines_ed <= irq_lines_q;

                irq_state <= (irq_state | irq_set | (~irq_lines_ed & irq_lines_q)) & ~irq_clear;
            end
    end
/// Interrupt Request ///

/// Control SPI slave interface ///
// Inputs
wire [6:0]  cntrl_spi_addr;
wire [31:0] cntrl_spi_data_out;
reg  [31:0] cntrl_spi_data_in;
wire        cntrl_spi_wr_en;
wire        cntrl_spi_rd_en;
wire        cntrl_spi_sck;
wire        cntrl_spi_mosi;
wire        cntrl_spi_miso;
wire        cntrl_spi_ncs;

SB_IO #(
    .PIN_TYPE(6'b1010_01),
    .PULLUP(1'b0)
)
cntrl_spi_miso_od
(
    .PACKAGE_PIN(SPI_MISO),
    .OUTPUT_ENABLE(!cntrl_spi_ncs),
    .D_OUT_0(cntrl_spi_miso)
);

assign SPI_SCK = cntrl_spi_sck;
assign SPI_MOSI = cntrl_spi_mosi;
assign SPI_CSn = cntrl_spi_ncs;

// Registers
localparam CNTRL_SPI_REG_ID                     = 7'h00;
localparam CNTRL_SPI_REG_RST_CNTRL              = 7'h01;
localparam CNTRL_SPI_REG_IRQ_STATE              = 7'h02;
localparam CNTRL_SPI_REG_IRQ_MASK               = 7'h03;
localparam CNTRL_SPI_REG_IRQ_SET_CLEAR          = 7'h04;

// Register parameters
//// CNTRL_SPI_REG_RST_CNTRL

//// CNTRL_SPI_REG_IRQ_STATE
wire [IRQ_COUNT - 1:0] cntrl_spi_irq_state;
reg                    cntrl_spi_irq_state_clear_on_read;

//// CNTRL_SPI_REG_IRQ_MASK
reg  [IRQ_COUNT - 1:0] cntrl_spi_irq_mask;

//// CNTRL_SPI_REG_IRQ_SET_CLEAR
reg  [IRQ_COUNT - 1:0] cntrl_spi_irq_set;
reg  [IRQ_COUNT - 1:0] cntrl_spi_irq_clear;

// Synchronizers
//// CNTRL_SPI_REG_RST_CNTRL

//// CNTRL_SPI_REG_IRQ_STATE
assign cntrl_spi_irq_state = irq_state; // No sync needed since runs on the same clock

//// CNTRL_SPI_REG_IRQ_MASK
assign irq_mask = cntrl_spi_irq_mask; // No sync needed since runs on the same clock

//// CNTRL_SPI_REG_IRQ_SET_CLEAR
assign irq_set = cntrl_spi_irq_set; // No sync needed since runs on the same clock
assign irq_clear = cntrl_spi_irq_clear; // No sync needed since runs on the same clock

// Module
spi_slave cntrl_spi
(
    .clk(cntrl_spi_clk),
    .reset(cntrl_spi_rst),
    .spi_sck(cntrl_spi_sck),
    .spi_mosi(cntrl_spi_mosi),
    .spi_miso(cntrl_spi_miso),
    .spi_ncs(cntrl_spi_ncs),
    .addr(cntrl_spi_addr),
    .data_out(cntrl_spi_data_out),
    .data_in(cntrl_spi_data_in),
    .wr_en(cntrl_spi_wr_en),
    .rd_en(cntrl_spi_rd_en)
);

always @(posedge cntrl_spi_clk)
    begin
        if(cntrl_spi_rst) // Register reset values
            begin
                cntrl_spi_irq_state_clear_on_read <= 1'b0;

                cntrl_spi_irq_mask <= {IRQ_COUNT{1'b0}};

                cntrl_spi_irq_set <= {IRQ_COUNT{1'b0}};
                cntrl_spi_irq_clear <= {IRQ_COUNT{1'b0}};
            end
        else
            begin
                cntrl_spi_irq_set <= ~cntrl_spi_irq_state & cntrl_spi_irq_set; // Remove IRQ set requests on lines that are already set
                cntrl_spi_irq_clear <= cntrl_spi_irq_state & cntrl_spi_irq_clear; // Remove IRQ clear requests on lines that are already cleared

                if(cntrl_spi_wr_en) // Synchronous writes
                    begin
                        case(cntrl_spi_addr)
                            CNTRL_SPI_REG_ID:
                                begin
                                    // Nothing to do
                                end
                            CNTRL_SPI_REG_RST_CNTRL:
                                begin
                                    
                                end
                            CNTRL_SPI_REG_IRQ_STATE:
                                begin
                                    cntrl_spi_irq_state_clear_on_read <= cntrl_spi_data_out[0];
                                end
                            CNTRL_SPI_REG_IRQ_MASK:
                                begin
                                    cntrl_spi_irq_mask <= cntrl_spi_data_out[7:0];
                                end
                            CNTRL_SPI_REG_IRQ_SET_CLEAR:
                                begin
                                    cntrl_spi_irq_set <= cntrl_spi_irq_set | cntrl_spi_data_out[7:0];
                                    cntrl_spi_irq_clear <= cntrl_spi_irq_clear | cntrl_spi_data_out[15:8];
                                end
                        endcase
                    end

                if(cntrl_spi_rd_en) // Synchronous reads
                    begin
                        case(cntrl_spi_addr)
                            CNTRL_SPI_REG_ID:
                                begin
                                    cntrl_spi_data_in <= {16'hDADC, 16'h0001};
                                end
                            CNTRL_SPI_REG_RST_CNTRL:
                                begin
                                    cntrl_spi_data_in <= {32'd0};
                                end
                            CNTRL_SPI_REG_IRQ_STATE:
                                begin
                                    cntrl_spi_data_in <= {24'd0, cntrl_spi_irq_state};

                                    if(cntrl_spi_irq_state_clear_on_read)
                                        cntrl_spi_irq_clear <= cntrl_spi_irq_state;
                                end
                            CNTRL_SPI_REG_IRQ_MASK:
                                begin
                                    cntrl_spi_data_in <= {24'd0, cntrl_spi_irq_mask};
                                end
                            CNTRL_SPI_REG_IRQ_SET_CLEAR:
                                begin
                                    cntrl_spi_data_in <= {16'd0, cntrl_spi_irq_clear, cntrl_spi_irq_set};
                                end
                            default:
                                begin
                                    cntrl_spi_data_in <= 32'h00000000;
                                end
                        endcase
                    end
            end
    end
/// Control SPI slave interface ///

endmodule