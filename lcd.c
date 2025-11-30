
#include "lcd.h"

void LCD_delay(Byte ms)     // LCD 시간지연 함수
{
    delay_ms(ms);
}

void                                                                                                                                                                           PortInit(void)
{
    DDRA = 0xFF;    // PORTA를 출력으로
    DDRG = 0x0F;    // PORTG의 하위 4비트를 출력으로
}

void LCD_Data(Byte ch)      // 데이터 레지스터에 데이터 쓰기 함수
{
    LCD_CTRL |=  (1 << LCD_RS);   // RS=1, R/W=0으로 데이터 쓰기 싸이클
    LCD_CTRL &= ~(1 << LCD_RW);
    LCD_CTRL |=  (1 << LCD_EN);   // LCD Enable
    delay_us(50);                 // 시간지연
    LCD_WDATA = ch;               // 데이터 출력
    delay_us(50);
    LCD_CTRL &= ~(1 << LCD_EN);   // LCD Disable
}

void LCD_Comm(Byte ch)      // 명령 레지스터에 명령어 쓰기 함수
{
    LCD_CTRL &= ~(1 << LCD_RS);   // RS=R/W=0으로 명령어 쓰기 사이클
    LCD_CTRL &= ~(1 << LCD_RW);
    LCD_CTRL |=  (1 << LCD_EN);
    delay_us(50);
    LCD_WINST = ch;
    delay_us(50);
    LCD_CTRL &= ~(1 <<LCD_EN);
}

void LCD_Shift(char p)
{
    if(p == RIGHT){
        LCD_Comm(0x1C);
        LCD_delay(1);
    }
    else if(p == LEFT){
        LCD_Comm(0x18);
        LCD_delay(1);
    }
}


void LCD_CHAR(Byte c)       // 한 문자 출력
{
    LCD_Data(c);
    delay_ms(1);
}

void LCD_STR(Byte *str)     // 문자열 출력
{
    while(*str != 0){
        LCD_CHAR(*str);
        str++;
    }
}

void LCD_NUM_Digit(Byte num)
{
    Byte digit = num%10;
    LCD_CHAR('0'+ digit);
}

void LCD_NUM_Byte(Byte num)
{
    // Numbers in Byte: 0~255
    Byte string[] = "000";

    string[0]= '0' + num/100;
    string[1]= '0' + (num%100)/10;
    string[2]= '0' + (num%10);

    LCD_STR(string);
}

void LCD_pos(unsigned char row, unsigned char col)
{
    LCD_Comm(0x80|(row*0x40+col));
}

void LCD_Clear(void)
{
    LCD_Comm(0x01);
    LCD_delay(2);
}

void LCD_Init(void)
{
    LCD_Comm(0x38);
    LCD_delay(2);
    LCD_Comm(0x38);
    LCD_delay(2);
    LCD_Comm(0x38);
    LCD_delay(2);
    LCD_Comm(0x0C); //0b 0000 1100 화면에 cursor 표시 안함.
    LCD_delay(2);
    LCD_Comm(0x01);
    LCD_delay(2);
    LCD_Comm(0x06);
    LCD_delay(2);
}

void Cursor_Home(void)
{
    LCD_Comm(0x02);
    LCD_delay(2);
}
