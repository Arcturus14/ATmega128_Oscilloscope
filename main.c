#include <iom128v.h>
#include <math.h>
#include "my128.h"
#include "lcd.h"

unsigned char adc_buffer[20];       // 파형 샘플링 버퍼
unsigned char cgram_pattern[8][8];  // CGRAM 비트맵 버퍼
volatile unsigned char measure_flag = 0; // 측정 요청 플래그


// USART0 통신
void USART0_Init(void) {
    UBRR0H = 0x00;
    UBRR0L = 0x67;   // 9600bps

    UCSR0B = (1 << TXEN0); // 송신
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // 8비트
}

void USART0_Transmit(unsigned char data) {
    long timeout = 0;
    // 버퍼 비워질 때까지 대기
    while (!(UCSR0A & (1 << UDRE0))) {
        timeout++;
        if (timeout > 60000) return; // 멈춤 방지
    }
    UDR0 = data;
    { int k; for(k=0; k<500; k++); } // 딜레이
}

void USART_Print(char *str) {
    while (*str) USART0_Transmit(*str++);
}

void USART_Send_Number(unsigned int num) {
    char buf[10];
    int i = 0;
    if (num == 0) { USART0_Transmit('0'); return; }
    while (num > 0) { buf[i++] = (num % 10) + '0'; num /= 10; }
    while (i > 0) USART0_Transmit(buf[--i]);
}

// 측정 결과 전송
void USART_Send_Result(unsigned int vpp, unsigned int vrms, unsigned char time_div) {
    unsigned int v_int = vpp / 100;
    unsigned int v_dec = vpp % 100;

    unsigned int r_int = vrms / 100;
    unsigned int r_dec = vrms % 100;

    // 시간 계산 (ms)
    unsigned long total_us = ((unsigned long)time_div * 4 + 60) * 20;
    unsigned int ms_int = (unsigned int)(total_us / 1000);
    unsigned int ms_dec = (unsigned int)((total_us % 1000) / 100);

    // Vpp
    USART_Print("V:");
    USART_Send_Number(v_int);       USART0_Transmit('.');
    USART_Send_Number(v_dec / 10);  USART_Send_Number(v_dec % 10);
    USART0_Transmit('V');           USART0_Transmit(',');

    // Vrms
    USART_Print("R:");
    USART_Send_Number(r_int);       USART0_Transmit('.');
    USART_Send_Number(r_dec / 10);  USART_Send_Number(r_dec % 10);
    USART0_Transmit('V');           USART0_Transmit(',');

    // Time/div
    USART_Print("T:");
    USART_Send_Number(ms_int);
    USART0_Transmit('.');
    USART_Send_Number(ms_dec);
    USART_Print("ms");

    USART_Print("    ");
    USART0_Transmit('\r'); // 문장 끝
}

// 초기화
void ADC_Init(void) {
    DDRF = 0x00; PORTF = 0x00;
    // AREF
    ADMUX = 0x00;
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1); // 250kHz
}

unsigned int Read_ADC(unsigned char channel) {
    ADMUX = (ADMUX & 0xE0) | (channel & 0x07);
    { int k; for(k=0; k<20; k++); }
    ADCSRA |= (1 << ADSC);
    while(ADCSRA & (1 << ADSC));
    return ADC;
}

void Timer0_Init(void) {
    TCCR0 = (1 << WGM01) | (1 << CS02); // CTC, 분주비 64
    TCNT0 = 0; OCR0 = 100;
}

void Wait_Using_Timer(unsigned char count) {
    if(count < 5) count = 5;
    OCR0 = count;
    TCNT0 = 0;
    TIFR |= (1 << OCF0);
    while (!(TIFR & (1 << OCF0)));
}

void INT0_Init(void) {
    DDRD &= ~(1 << 0);
    EICRA |= (1 << ISC01); EICRA &= ~(1 << ISC00); // Falling Edge
    EIMSK |= (1 << INT0);
    SREG |= 0x80;
}

#pragma interrupt_handler int0_isr:iv_EXT_INT0
void int0_isr(void) {
    measure_flag = 1;
}

void LCD_Write_CGRAM(unsigned char char_num, unsigned char *pattern) {
    int i;
    LCD_Comm(0x40 | (char_num << 3));
    for (i = 0; i < 8; i++)
        LCD_Data(pattern[i]);
}


// 파형 그리기
void Init_Waveform_Screen(void) {
    int i;
    LCD_pos(0, 0);
    for(i=0; i<16; i++)
        LCD_CHAR(i % 4);

    LCD_pos(1, 0);
    for(i=0; i<16; i++)
        LCD_CHAR((i % 4) + 4);
}

void Run_Waveform(void) {
    unsigned char i, j, x, y_curr, y_prev;
    unsigned char y_start, y_end, k; // 수직선 그리기용
    unsigned char char_idx, col_idx;
    unsigned int time_div_val;

    // Time/div 가변저항 읽기
    time_div_val = Read_ADC(1) / 4;

    // 트리거 (Rising Edge)
    Read_ADC(0);
    while(Read_ADC(0) > 512);
    while(Read_ADC(0) < 512);

    // 샘플링 (20개)
    for(i = 0; i < 20; i++) {
        adc_buffer[i] = Read_ADC(0) / 64; // 0~15 높이
        Wait_Using_Timer((unsigned char)time_div_val);
    }

    // 비트맵 생성
    for(i=0; i<8; i++)
        for(j=0; j<8; j++)
            cgram_pattern[i][j]=0;

    for(x=0; x<20; x++) {
        y_curr = adc_buffer[x];
        if (x == 0) {
            y_start = y_curr; y_end = y_curr;
        } else {
            y_prev = adc_buffer[x-1];
            if (y_prev < y_curr) { y_start = y_prev; y_end = y_curr; }
            else { y_start = y_curr; y_end = y_prev; }
        }

        for (k = y_start; k <= y_end; k++) {
            char_idx = x / 5;
            col_idx  = x % 5;
            if (k >= 8) {
                cgram_pattern[char_idx][15 - k] |= (0x10 >> col_idx);
            } else {
                cgram_pattern[char_idx + 4][7 - k] |= (0x10 >> col_idx);
            }
        }
    }

    // 화면 갱신
    for(i=0; i<8; i++) LCD_Write_CGRAM(i, cgram_pattern[i]);
}

// 측정 및 전송
void Measure_And_Send(void) {
    unsigned int val, max=0, min=1023, vpp=0, vrms=0;
    int i, k;

    // RMS 계산
    unsigned long sum_squares = 0;
    unsigned int sample_count = 0;
    unsigned long mean_square;
    unsigned int rms_adc;

    unsigned int time_current = Read_ADC(1) / 4;
    if(time_current < 5) time_current = 5;

    // 1000개 샘플링
    for (i = 0; i < 1000; i++) {
        val = Read_ADC(0);

        // Vpp Max/Min
        if(val > max) max = val;
        if(val < min) min = val;

        // 제곱 합
        sum_squares += (unsigned long)val * (unsigned long)val;
        sample_count++;

        // 샘플링 딜레이
        { for(k=0; k<10; k++); }
    }

    // Vpp 계산
    if(max >= min) {
        vpp = ((unsigned long)(max-min)*500UL)/1023UL;
    }
    // 평균 제곱
    mean_square = sum_squares / sample_count;
    rms_adc = (unsigned int)sqrt((double)mean_square);
    // 전압 변환
    vrms = (unsigned long)rms_adc * 500UL / 1023UL;

    // 데이터 전송
    USART_Send_Result(vpp, vrms, (unsigned char)time_current);

    LCD_delay(200);
    LCD_Clear();
    Init_Waveform_Screen(); // 화면 복구
}

int main(void) {
    PortInit();
    LCD_Init();
    ADC_Init();
    Timer0_Init();
    INT0_Init();
    USART0_Init();

    LCD_Clear();
    Init_Waveform_Screen();

    while(1) {
        Run_Waveform();

        if (measure_flag) {
            measure_flag = 0;
            Measure_And_Send();
        }
    }
    return 0;
}
