#include <stdint.h>

// CC2630 电源域 & 寄存器
#define PRCM_BASE       0x40082000
#define PRCM_PDCTL0     (*((volatile uint32_t *)(PRCM_BASE + 0x004)))
#define PRCM_CLKGR0     (*((volatile uint32_t *)(PRCM_BASE + 0x020)))

#define GPIO_BASE       0x40022000
#define GPIO_DIOSET     (*((volatile uint32_t *)(GPIO_BASE + 0x038)))
#define GPIO_DIOCLR     (*((volatile uint32_t *)(GPIO_BASE + 0x040)))
#define GPIO_DIODIR     (*((volatile uint32_t *)(GPIO_BASE + 0x044)))

// TG-GM29MT1C 引脚
#define EPD_SCK     10
#define EPD_MOSI    9
#define EPD_CS      12
#define EPD_DC      13
#define EPD_RST     14
#define EPD_BUSY    15

#define EPD_WIDTH   296
#define EPD_HEIGHT  128
#define EPD_BUF_SIZE (EPD_HEIGHT * (EPD_WIDTH / 8))
uint8_t epd_buf[EPD_BUF_SIZE];

// SSD1680 全屏LUT
const uint8_t lut_full[] = {
0x00,0x08,0x00,0x00,0x00,0x01,
0x60,0x08,0x08,0x00,0x00,0x01,
0x00,0x08,0x08,0x00,0x00,0x01,
0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,
0x22,0x22,0x22,0x22,0x22,0x22,0x00,0x00
};

void my_memset(uint8_t *dst, uint8_t val, uint32_t len)
{
    while(len--) *dst++ = val;
}

static inline void gpio_set(uint8_t pin) { GPIO_DIOSET = (1U << pin); }
static inline void gpio_clr(uint8_t pin) { GPIO_DIOCLR = (1U << pin); }

void delay_ms(uint32_t ms)
{
    for(uint32_t i=0; i<ms * 2500; i++);
}

void spi_wr(uint8_t dat)
{
    for(int i=7;i>=0;i--){
        gpio_clr(EPD_SCK);
        if(dat & (1U << i)) gpio_set(EPD_MOSI);
        else gpio_clr(EPD_MOSI);
        gpio_set(EPD_SCK);
    }
}

void epd_cmd(uint8_t c)
{
    gpio_clr(EPD_DC);
    gpio_clr(EPD_CS);
    spi_wr(c);
    gpio_set(EPD_CS);
}
void epd_data(uint8_t d)
{
    gpio_set(EPD_DC);
    gpio_clr(EPD_CS);
    spi_wr(d);
    gpio_set(EPD_CS);
}

void epd_load_lut(const uint8_t *lut, uint32_t len)
{
    epd_cmd(0x32);
    for(uint32_t i=0;i<len;i++) epd_data(lut[i]);
}

void epd_init(void)
{
    // 硬件复位
    gpio_clr(EPD_RST);
    delay_ms(20);
    gpio_set(EPD_RST);
    delay_ms(200);

    epd_cmd(0x12); // soft reset
    delay_ms(100);

    epd_cmd(0x01);
    epd_data(0x27);
    epd_data(0x01);
    epd_data(0x00);

    epd_cmd(0x11);
    epd_data(0x03);

    epd_cmd(0x44);
    epd_data(0x00);
    epd_data(0x11);
    epd_cmd(0x45);
    epd_data(0x00);
    epd_data(0x7F);

    epd_cmd(0x3C);
    epd_data(0x05);

    epd_load_lut(lut_full, sizeof(lut_full));
}

void epd_render_full(void)
{
    epd_cmd(0x24);
    for(int i=0;i<EPD_BUF_SIZE;i++) epd_data(epd_buf[i]);
    epd_cmd(0x22);
    epd_data(0xF7);
    epd_cmd(0x20);
    delay_ms(800);
}

void Reset_Handler(void)
{
    // =========【关键！打开外设电源域，启用GPIO模块】=========
    PRCM_PDCTL0 |= (1U << 0); // PERIPH 电源域上电
    while( !(PRCM_PDCTL0 & (1U << 0)) );
    PRCM_CLKGR0 |= (1U << 13); // GPIO时钟使能

    // 设置SPI、控制引脚全部为输出
    GPIO_DIODIR |= (1U<<EPD_SCK)|(1U<<EPD_MOSI)|(1U<<EPD_CS)|(1U<<EPD_DC)|(1U<<EPD_RST);
    gpio_set(EPD_CS);
    gpio_set(EPD_DC);

    epd_init();

    // 填充全黑
    my_memset(epd_buf, 0x00, EPD_BUF_SIZE);
    epd_render_full();

    while(1)
    {
        // 停在这里，只刷一次全屏黑色
    }
}

__attribute__((used,section(".isr_vector")))
const uint32_t vector_table[] = {
    0x20005000,
    (uint32_t)Reset_Handler
};
