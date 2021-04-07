/*
 * LCDLib.c
 *
 *  Created on: Mar 2, 2021
 *      Author: Daniel Dole-Muinos
 */

#include "LCDLib.h"
#include "msp.h"
#include "driverlib.h"
#include "AsciiLib.h"

/************************************  Private Functions  *******************************************/

/*
 * Delay x ms
 */
void Delay(unsigned long interval)
{
    while(interval > 0)
    {
        __delay_cycles(48000);
        interval--;
    }
}

/*******************************************************************************
 * Function Name  : LCD_initSPI
 * Description    : Configures LCD Control lines
 * Input          : None
 * Output         : None
 * Return         : None
 * Attention      : None
 *******************************************************************************/
void LCD_initSPI()
{
    // init SPI: 3 Pin, 8bit, SPI master, high polarity for inactive state, use SMCLK (NEEDS TO BE SET TO 12MHz)
    EUSCI_B3->CTLW0 |= EUSCI_B_CTLW0_SWRST;
    EUSCI_B3->CTLW0 |= EUSCI_B_CTLW0_MODE_0 | EUSCI_B_CTLW0_MST | EUSCI_B_CTLW0_CKPL | EUSCI_B_CTLW0_SYNC | EUSCI_B_CTLW0_UCSSEL_3 | EUSCI_B_CTLW0_MSB;

    // init 3 line SPI signals on P10
    P10->SEL0 |= BIT1 | BIT2 | BIT3;
    P10->SEL1 &= ~(BIT1 | BIT2 | BIT3);

    // init chip selects for LCD and TP
    P10->DIR |= BIT4 | BIT5;
    P10->OUT |= BIT4 | BIT5;

    // clear reset bit
    EUSCI_B3->CTLW0 &= ~EUSCI_B_CTLW0_SWRST;
}

/*******************************************************************************
 * Function Name  : LCD_reset
 * Description    : Resets LCD
 * Input          : None
 * Output         : None
 * Return         : None
 * Attention      : Uses P10.0 for reset
 *******************************************************************************/
void LCD_reset()
{
    P10DIR |= BIT0;
    P10OUT |= BIT0;  // high
    Delay(100);
    P10OUT &= ~BIT0; // low
    Delay(100);
    P10OUT |= BIT0;  // high
}

/************************************  Private Functions  *******************************************/


/************************************  Public Functions  *******************************************/

/*******************************************************************************
 * Function Name  : LCD_DrawRectangle
 * Description    : Draw a rectangle as the specified color
 * Input          : xStart, xEnd, yStart, yEnd, Color
 * Output         : None
 * Return         : None
 * Attention      : Must draw from left to right, top to bottom!
 *******************************************************************************/
void LCD_DrawRectangle(int16_t xStart, int16_t xEnd, int16_t yStart, int16_t yEnd, uint16_t Color)
{
    // Optimization complexity: O(64 + 2N) Bytes Written 

    /* Check special cases for out of bounds */
    if(xStart >= MAX_SCREEN_X)  xStart = MAX_SCREEN_X;
    if(xStart < MIN_SCREEN_X)   xStart = MIN_SCREEN_X;
    if(yStart >= MAX_SCREEN_Y)  yStart = MAX_SCREEN_Y;
    if(yStart < MIN_SCREEN_Y)   yStart = MIN_SCREEN_Y;
    if(xEnd >= MAX_SCREEN_X)    xEnd = MAX_SCREEN_X;
    if(xEnd < MIN_SCREEN_X)     xEnd = MIN_SCREEN_X;
    if(yEnd >= MAX_SCREEN_Y)    yEnd = MAX_SCREEN_Y;
    if(yEnd < MIN_SCREEN_Y)     yEnd = MIN_SCREEN_Y;
    /* Set window area for high-speed RAM write */
    LCD_WriteReg(HOR_ADDR_START_POS,yStart);        /* Horizontal GRAM Start Address */
    LCD_WriteReg(HOR_ADDR_END_POS, yEnd-1);         /* Horizontal GRAM End Address */
    LCD_WriteReg(VERT_ADDR_START_POS, xStart);      /* Vertical GRAM Start Address */
    LCD_WriteReg(VERT_ADDR_END_POS, xEnd-1);        /* Vertical GRAM Start Address */
    /* Set cursor */ 
    LCD_SetCursor(xStart, yStart);
    /* Set index to GRAM */ 
    LCD_WriteIndex(GRAM);
    /* Send out data only to the entire area */ 
    SPI_CS_LOW;
    LCD_Write_Data_Start();
    uint16_t len = (xStart-xEnd) * (yStart-yEnd);
    for(int i = 0; i < len; i++)
        LCD_Write_Data_Only(Color);
    SPI_CS_HIGH;
}

/******************************************************************************
 * Function Name  : PutChar
 * Description    : Lcd screen displays a character
 * Input          : - Xpos: Horizontal coordinate
 *                  - Ypos: Vertical coordinate
 *                  - ASCI: Displayed character
 *                  - charColor: Character color
 * Output         : None
 * Return         : None
 * Attention      : None
 *******************************************************************************/
inline void PutChar( uint16_t Xpos, uint16_t Ypos, uint8_t ASCI, uint16_t charColor)
{
    uint16_t i, j;
    uint8_t buffer[16], tmp_char;
    GetASCIICode(buffer,ASCI);  /* get font data */
    for( i=0; i<16; i++ )
    {
        tmp_char = buffer[i];
        for( j=0; j<8; j++ )
        {
            if( (tmp_char >> 7 - j) & 0x01 == 0x01 )
            {
                LCD_SetPoint( Xpos + j, Ypos + i, charColor );  /* Character color */
            }
        }
    }
}

/******************************************************************************
 * Function Name  : GUI_Text
 * Description    : Displays the string
 * Input          : - Xpos: Horizontal coordinate
 *                  - Ypos: Vertical coordinate
 *                  - str: Displayed string
 *                  - charColor: Character color
 * Output         : None
 * Return         : None
 * Attention      : None
 *******************************************************************************/
void LCD_Text(uint16_t Xpos, uint16_t Ypos, uint8_t *str, uint16_t Color)
{
    uint8_t TempChar;

    /* Set area back to span the entire LCD */
    LCD_WriteReg(HOR_ADDR_START_POS, 0x0000);               /* Horizontal GRAM Start Address */
    LCD_WriteReg(HOR_ADDR_END_POS, (MAX_SCREEN_Y - 1));     /* Horizontal GRAM End Address */
    LCD_WriteReg(VERT_ADDR_START_POS, 0x0000);              /* Vertical GRAM Start Address */
    LCD_WriteReg(VERT_ADDR_END_POS, (MAX_SCREEN_X - 1));    /* Vertical GRAM End Address */
    do
    {
        TempChar = *str++;
        PutChar( Xpos, Ypos, TempChar, Color);
        if( Xpos < MAX_SCREEN_X - 8)
        {
            Xpos += 8;
        }
        else if ( Ypos < MAX_SCREEN_X - 16)
        {
            Xpos = 0;
            Ypos += 16;
        }
        else
        {
            Xpos = 0;
            Ypos = 0;
        }
    }
    while ( *str != 0 );
}


/*******************************************************************************
 * Function Name  : LCD_Clear
 * Description    : Fill the screen as the specified color
 * Input          : - Color: Screen Color
 * Output         : None
 * Return         : None
 * Attention      : None
 *******************************************************************************/
void LCD_Clear(uint16_t Color)
{
    /* Set area back to span the entire LCD */
    LCD_WriteReg(HOR_ADDR_START_POS, 0x0000);               /* Horizontal GRAM Start Address */
    LCD_WriteReg(HOR_ADDR_END_POS, (MAX_SCREEN_Y - 1));     /* Horizontal GRAM End Address */
    LCD_WriteReg(VERT_ADDR_START_POS, 0x0000);              /* Vertical GRAM Start Address */
    LCD_WriteReg(VERT_ADDR_END_POS, (MAX_SCREEN_X - 1));    /* Vertical GRAM Start Address */
    /* Set cursor to (0,0) */ 
    LCD_SetCursor(0x0, 0x0);
    /* Set write index to GRAM */     
    LCD_WriteIndex(GRAM);
    /* Start data transmission */
    SPI_CS_LOW;
    LCD_Write_Data_Start();
    /*Call LCD_Write_Data_Start() and then send out only data to fill entire screen with color */
    for(int i = 0; i < SCREEN_SIZE; i++) {
        LCD_Write_Data_Only(Color);
    }
    SPI_CS_HIGH;
}

/******************************************************************************
 * Function Name  : LCD_SetPoint
 * Description    : Drawn at a specified point coordinates
 * Input          : - Xpos: Row Coordinate
 *                  - Ypos: Line Coordinate
 * Output         : None
 * Return         : None
 * Attention      : 18N Bytes Written
 *******************************************************************************/
void LCD_SetPoint(uint16_t Xpos, uint16_t Ypos, uint16_t color)
{
    /* Should check for out of bounds */ 
    if(Xpos <= MIN_SCREEN_X) Xpos = MIN_SCREEN_X;       // X check
    if(Xpos >= MAX_SCREEN_X) Xpos = MAX_SCREEN_X - 1;
    if(Ypos <= MIN_SCREEN_Y) Ypos = MIN_SCREEN_Y;       // Y check
    if(Ypos >= MAX_SCREEN_Y) Ypos = MAX_SCREEN_Y - 1;
    /* Set cursor to Xpos and Ypos */ 
    LCD_SetCursor(Xpos,Ypos);
    /* Write color to GRAM reg */ 
    LCD_WriteReg(GRAM, color);
}

/*******************************************************************************
 * Function Name  : LCD_Write_Data_Only
 * Description    : Data writing to the LCD controller
 * Input          : - data: data to be written
 * Output         : None
 * Return         : None
 * Attention      : None
 *******************************************************************************/
inline void LCD_Write_Data_Only(uint16_t data)
{
    /* Send out MSB */ 
    SPISendRecvByte((data >> 8));       // Write D15-D8
    /* Send out LSB */ 
    SPISendRecvByte((data & 0xFF));     // Write D7-D0

}

/*******************************************************************************
 * Function Name  : LCD_WriteData
 * Description    : LCD write register data
 * Input          : - data: register data
 * Output         : None
 * Return         : None
 * Attention      : None
 *******************************************************************************/
inline void LCD_WriteData(uint16_t data)
{
    SPI_CS_LOW;

    SPISendRecvByte(SPI_START | SPI_WR | SPI_DATA);    /* Write : RS = 1, RW = 0       */
    SPISendRecvByte((data >>   8));                    /* Write D8..D15                */
    SPISendRecvByte((data & 0xFF));                    /* Write D0..D7                 */

    SPI_CS_HIGH;
}

/*******************************************************************************
 * Function Name  : LCD_WriteReg
 * Description    : Reads the selected LCD Register.
 * Input          : None
 * Output         : None
 * Return         : LCD Register Value.
 * Attention      : None
 *******************************************************************************/
inline uint16_t LCD_ReadReg(uint16_t LCD_Reg)
{
    /* Write 16-bit Index */
    LCD_WriteIndex(LCD_Reg);
    /* Return 16-bit Reg using LCD_ReadData() */
    return LCD_ReadData();
}

/*******************************************************************************
 * Function Name  : LCD_WriteIndex
 * Description    : LCD write register address
 * Input          : - index: register address
 * Output         : None
 * Return         : None
 * Attention      : None
 *******************************************************************************/
inline void LCD_WriteIndex(uint16_t index)
{
    SPI_CS_LOW;

    /* SPI write data */
    SPISendRecvByte(SPI_START | SPI_WR | SPI_INDEX);   /* Write : RS = 0, RW = 0  */
    SPISendRecvByte(0);
    SPISendRecvByte(index);

    SPI_CS_HIGH;
}

/*******************************************************************************
 * Function Name  : SPISendRecvByte
 * Description    : Send one byte then receive one byte of response
 * Input          : uint8_t: byte
 * Output         : None
 * Return         : Recieved value 
 * Attention      : None
 *******************************************************************************/
inline uint8_t SPISendRecvByte (uint8_t byte)
{
    /* Send byte of data */
    SPI_transmitData(EUSCI_B3_BASE, byte);
    /* Wait as long as busy */ 
    while(!(EUSCI_B3->IFG & UCRXIFG));
    /* Return received value*/
    return SPI_receiveData(EUSCI_B3_BASE);
}

/*******************************************************************************
 * Function Name  : LCD_Write_Data_Start
 * Description    : Start of data writing to the LCD controller
 * Input          : None
 * Output         : None
 * Return         : None
 * Attention      : None
 *******************************************************************************/
inline void LCD_Write_Data_Start(void)
{
    SPISendRecvByte(SPI_START | SPI_WR | SPI_DATA);    /* Write : RS = 1, RW = 0 */
}

/*******************************************************************************
 * Function Name  : LCD_ReadData
 * Description    : LCD read data
 * Input          : None
 * Output         : None
 * Return         : return data
 * Attention      : Diagram (d) in datasheet
 *******************************************************************************/
inline uint16_t LCD_ReadData()
{
    uint16_t value;
    SPI_CS_LOW;

    SPISendRecvByte(SPI_START | SPI_RD | SPI_DATA);   /* Read: RS = 1, RW = 1   */
    SPISendRecvByte(0);                               /* Dummy read 1           */
    value = (SPISendRecvByte(0) << 8);                /* Read D8..D15           */
    value |= SPISendRecvByte(0);                      /* Read D0..D7            */

    SPI_CS_HIGH;
    return value;
}

/*******************************************************************************
 * Function Name  : LCD_WriteReg
 * Description    : Writes to the selected LCD register.
 * Input          : - LCD_Reg: address of the selected register.
 *                  - LCD_RegValue: value to write to the selected register.
 * Output         : None
 * Return         : None
 * Attention      : None
 *******************************************************************************/
inline void LCD_WriteReg(uint16_t LCD_Reg, uint16_t LCD_RegValue)
{
    /* Write 16-bit Index */
    LCD_WriteIndex(LCD_Reg);
    /* Write 16-bit Reg Data */
    LCD_WriteData(LCD_RegValue);
}

/*******************************************************************************
 * Function Name  : LCD_SetCursor
 * Description    : Sets the cursor position.
 * Input          : - Xpos: specifies the X position.
 *                  - Ypos: specifies the Y position.
 * Output         : None
 * Return         : None
 * Attention      : None
 *******************************************************************************/
inline void LCD_SetCursor(uint16_t Xpos, uint16_t Ypos )
{
    /* Set horizonal GRAM coordinate (Ypos) */ 
    LCD_WriteReg(GRAM_HORIZONTAL_ADDRESS_SET, Ypos);
    /* Set vertical GRAM coordinate (Xpos) */
    LCD_WriteReg(GRAM_VERTICAL_ADDRESS_SET, Xpos);
}

/*******************************************************************************
 * Function Name  : LCD_Init
 * Description    : Configures LCD Control lines, sets whole screen black
 * Input          : bool usingTP: determines whether or not to enable TP interrupt 
 * Output         : None
 * Return         : None
 * Attention      : None
 *******************************************************************************/
void LCD_Init(bool usingTP)
{
    LCD_initSPI();

    if (usingTP)
    {
        /* Configure low true interrupt on P4.0 for TP */ 
        P4->DIR &= ~BIT0;
        P4->IFG &= ~BIT0; // P4.0 IFG cleared
        P4->IE  |=  BIT0; // enable interrupt on P4.4
        P4->IES |=  BIT0; // high-to-low transition
        P4->REN |=  BIT0; // pull-up resistor
        P4->OUT |=  BIT0; // sets resistor to pull-up
    }

    LCD_reset();

    LCD_WriteReg(0xE5, 0x78F0);                             /* set SRAM internal timing */
    LCD_WriteReg(DRIVER_OUTPUT_CONTROL, 0x0100);            /* set Driver Output Control */
    LCD_WriteReg(DRIVING_WAVE_CONTROL, 0x0700);             /* set 1 line inversion */
    LCD_WriteReg(ENTRY_MODE, 0x1038);                       /* set GRAM write direction and BGR=1 */
    LCD_WriteReg(RESIZING_CONTROL, 0x0000);                 /* Resize register */
    LCD_WriteReg(DISPLAY_CONTROL_2, 0x0207);                /* set the back porch and front porch */
    LCD_WriteReg(DISPLAY_CONTROL_3, 0x0000);                /* set non-display area refresh cycle ISC[3:0] */
    LCD_WriteReg(DISPLAY_CONTROL_4, 0x0000);                /* FMARK function */
    LCD_WriteReg(RGB_DISPLAY_INTERFACE_CONTROL_1, 0x0000);  /* RGB interface setting */
    LCD_WriteReg(FRAME_MARKER_POSITION, 0x0000);            /* Frame marker Position */
    LCD_WriteReg(RGB_DISPLAY_INTERFACE_CONTROL_2, 0x0000);  /* RGB interface polarity */

    /* Power On sequence */
    LCD_WriteReg(POWER_CONTROL_1, 0x0000);                  /* SAP, BT[3:0], AP, DSTB, SLP, STB */
    LCD_WriteReg(POWER_CONTROL_2, 0x0007);                  /* DC1[2:0], DC0[2:0], VC[2:0] */
    LCD_WriteReg(POWER_CONTROL_3, 0x0000);                  /* VREG1OUT voltage */
    LCD_WriteReg(POWER_CONTROL_4, 0x0000);                  /* VDV[4:0] for VCOM amplitude */
    LCD_WriteReg(DISPLAY_CONTROL_1, 0x0001);
    Delay(200);

    /* Dis-charge capacitor power voltage */
    LCD_WriteReg(POWER_CONTROL_1, 0x1090);                  /* SAP, BT[3:0], AP, DSTB, SLP, STB */
    LCD_WriteReg(POWER_CONTROL_2, 0x0227);                  /* Set DC1[2:0], DC0[2:0], VC[2:0] */
    Delay(50); /* Delay 50ms */
    LCD_WriteReg(POWER_CONTROL_3, 0x001F);
    Delay(50); /* Delay 50ms */
    LCD_WriteReg(POWER_CONTROL_4, 0x1500);                  /* VDV[4:0] for VCOM amplitude */
    LCD_WriteReg(POWER_CONTROL_7, 0x0027);                  /* 04 VCM[5:0] for VCOMH */
    LCD_WriteReg(FRAME_RATE_AND_COLOR_CONTROL, 0x000D);     /* Set Frame Rate */
    Delay(50); /* Delay 50ms */
    LCD_WriteReg(GRAM_HORIZONTAL_ADDRESS_SET, 0x0000);      /* GRAM horizontal Address */
    LCD_WriteReg(GRAM_VERTICAL_ADDRESS_SET, 0x0000);        /* GRAM Vertical Address */

    /* Adjust the Gamma Curve */
    LCD_WriteReg(GAMMA_CONTROL_1,    0x0000);
    LCD_WriteReg(GAMMA_CONTROL_2,    0x0707);
    LCD_WriteReg(GAMMA_CONTROL_3,    0x0307);
    LCD_WriteReg(GAMMA_CONTROL_4,    0x0200);
    LCD_WriteReg(GAMMA_CONTROL_5,    0x0008);
    LCD_WriteReg(GAMMA_CONTROL_6,    0x0004);
    LCD_WriteReg(GAMMA_CONTROL_7,    0x0000);
    LCD_WriteReg(GAMMA_CONTROL_8,    0x0707);
    LCD_WriteReg(GAMMA_CONTROL_9,    0x0002);
    LCD_WriteReg(GAMMA_CONTROL_10,   0x1D04);

    /* Set GRAM area */
    LCD_WriteReg(HOR_ADDR_START_POS, 0x0000);               /* Horizontal GRAM Start Address */
    LCD_WriteReg(HOR_ADDR_END_POS, (MAX_SCREEN_Y - 1));     /* Horizontal GRAM End Address */
    LCD_WriteReg(VERT_ADDR_START_POS, 0x0000);              /* Vertical GRAM Start Address */
    LCD_WriteReg(VERT_ADDR_END_POS, (MAX_SCREEN_X - 1));    /* Vertical GRAM Start Address */
    LCD_WriteReg(GATE_SCAN_CONTROL_0X60, 0x2700);           /* Gate Scan Line */
    LCD_WriteReg(GATE_SCAN_CONTROL_0X61, 0x0001);           /* NDL,VLE, REV */
    LCD_WriteReg(GATE_SCAN_CONTROL_0X6A, 0x0000);           /* set scrolling line */

    /* Partial Display Control */
    LCD_WriteReg(PART_IMAGE_1_DISPLAY_POS, 0x0000);
    LCD_WriteReg(PART_IMG_1_START_END_ADDR_0x81, 0x0000);
    LCD_WriteReg(PART_IMG_1_START_END_ADDR_0x82, 0x0000);
    LCD_WriteReg(PART_IMAGE_2_DISPLAY_POS, 0x0000);
    LCD_WriteReg(PART_IMG_2_START_END_ADDR_0x84, 0x0000);
    LCD_WriteReg(PART_IMG_2_START_END_ADDR_0x85, 0x0000);

    /* Panel Control */
    LCD_WriteReg(PANEL_ITERFACE_CONTROL_1, 0x0010);
    LCD_WriteReg(PANEL_ITERFACE_CONTROL_2, 0x0600);
    LCD_WriteReg(DISPLAY_CONTROL_1, 0x0133);                /* 262K color and display ON */
    Delay(50);                                              /* delay 50 ms */

    LCD_Clear(LCD_BLACK);
}

/*******************************************************************************
 * Function Name  : LCD_ReadPixelColor
 * Description    : Reads color from designated pixel
 * Input          : x and y coordinate
 * Output         : None
 * Return         : Pixel color
 * Attention      : None
 *******************************************************************************/
uint16_t LCD_ReadPixelColor(uint16_t x, uint16_t y)
{
    LCD_SetCursor(x, y);
    LCD_WriteIndex(GRAM);
    return LCD_ReadData();
}

/*******************************************************************************
 * Function Name  : TP_ReadXY
 * Description    : Obtain X and Y touch coordinates
 * Input          : None
 * Output         : None
 * Return         : Pointer to "Point" structure
 * Attention      : None
 *******************************************************************************/
Point TP_ReadXY()
{
    Point tp_point;                                 // create result var
    uint8_t dummy = 0;                              // prep dummy byte to receive data

    EUSCI_B3->CTLW0 |= EUSCI_B_CTLW0_SWRST;         // set prescaler to make the SPI clock slower for touchpad
    EUSCI_B3->BRW   = 10;
    EUSCI_B3->CTLW0 &= ~EUSCI_B_CTLW0_SWRST;

    /* Read X coord. */
    SPI_CS_TP_LOW;
    SPISendRecvByte(CHX);
    tp_point.x =  SPISendRecvByte(dummy) << 5;      // return X11:5
    tp_point.x |= SPISendRecvByte(dummy) >> 3;      // return X4:0
    SPI_CS_TP_HIGH;

    /* Read Y coord. */
    SPI_CS_TP_LOW;
    SPISendRecvByte(CHY);
    tp_point.y =  SPISendRecvByte(dummy) << 5;      // return X11:5
    tp_point.y |= SPISendRecvByte(dummy) >> 3;      // return X4:0
    SPI_CS_TP_HIGH;

    EUSCI_B3->CTLW0 |= EUSCI_B_CTLW0_SWRST;         // restore SPI prescaler setting for LCD
    EUSCI_B3->BRW   = 0;
    EUSCI_B3->CTLW0 &= ~EUSCI_B_CTLW0_SWRST;

    /* Convert TP pos to LCD pos */
    tp_point.x = ((tp_point.x - 320) * MAX_SCREEN_X) / (3600);
    tp_point.y = ((tp_point.y - 188) * MAX_SCREEN_Y) / (3600);

    /* Return point  */
    return tp_point;
}

/************************************  Public Functions  *******************************************/

