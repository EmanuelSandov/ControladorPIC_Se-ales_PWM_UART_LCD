#include <xc.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

// Configuración de bits de configuración para PIC16F873A
#pragma config FOSC = XT      // Oscilador externo
#pragma config WDTE = OFF     // Watchdog Timer deshabilitado
#pragma config PWRTE = OFF    // Power-up Timer deshabilitado
#pragma config CP = OFF       // Code Protection deshabilitado
#pragma config BOREN = ON     // Brown-out Reset habilitado
#pragma config LVP = OFF      // Low Voltage Programming deshabilitado
#pragma config CPD = OFF      // Data Code Protection deshabilitado
#pragma config WRT = OFF      // Flash Program Memory Write Enable
#pragma config DEBUG = OFF    // Debug deshabilitado

#define _XTAL_FREQ 4000000 // Frecuencia del oscilador en Hz

// Definiciones de pines para el LCD
#define RS PORTCbits.RC3
#define RW PORTCbits.RC4
#define E  PORTCbits.RC5
#define DATA_PORT PORTB

// Definición del buzzer
#define BUZZER PORTAbits.RA5

// Definiciones de la comunicación serie
#define TX PORTCbits.RC6
#define RX PORTCbits.RC7

// Definiciones de comandos del LCD
#define CLEAR_DISPLAY 0x01
#define RETURN_HOME 0x02
#define DISPLAY_ON_CURSOR_BLINK 0x0F
#define FUNCTION_SET_8BIT 0x38
#define SET_CURSOR_ROW1 0x80
#define SET_CURSOR_ROW2 0xC0

#define MAX_CHARS 20 // Máximo número de caracteres a mostrar
#define MAX_RECEIVE_LENGTH 15

// Declaraciones de funciones para el LCD
void LCD_command(unsigned char command);
void LCD_data(unsigned char data);
void LCD_init(void);
void LCD_clear(void);
void LCD_setCursor(unsigned char row, unsigned char column);
void LCD_print(const char *string);
void __interrupt() ISR(void);
void UART_Init();
char UART_Read();
void UART_Write(char data);

// Buffer para almacenar los caracteres recibidos
char receivedData[MAX_CHARS];
unsigned char dataIndex = 0;

void LCD_command(unsigned char command) {
    RS = 0;
    RW = 0;
    DATA_PORT = command;
    E = 1;
    __delay_ms(2); // Aumenta este tiempo si es necesario
    E = 0;
}

void LCD_data(unsigned char data) {
    RS = 1;
    RW = 0;
    DATA_PORT = data;
    E = 1;
    __delay_ms(2); // Aumenta este tiempo si es necesario
    E = 0;
}

void LCD_init() {
    // Configura todos los pines de PORTA como digitales si es necesario
    ADCON1 = 0x07;  // Desactiva las entradas analógicas
    TRISC = 0x00;   // Configura los pines de control como salidas
    TRISB = 0x00;   // Configura los pines de datos como salidas

    // Espera inicial antes de configurar el LCD
    __delay_ms(20); // Espera para que el LCD se estabilice después de encender

    // Inicialización del LCD en modo de 8 bits
    LCD_command(FUNCTION_SET_8BIT);  // Función Set: modo de 8 bits
    __delay_ms(5);                   // Espera para que el comando se procese

    LCD_command(DISPLAY_ON_CURSOR_BLINK); // Display ON, cursor y parpadeo ON
    __delay_ms(5);                   // Espera para que el comando se procese

    LCD_command(CLEAR_DISPLAY);     // Limpia el display
    __delay_ms(5);                  // Espera para que el display se limpie

    LCD_command(RETURN_HOME);       // Retorna el cursor a casa
    __delay_ms(5);                  // Espera para que el cursor se posicione en casa
}

void LCD_clear() {
    LCD_command(CLEAR_DISPLAY);
    __delay_ms(2);
}

void LCD_setCursor(unsigned char row, unsigned char column) {
    unsigned char position = (row == 1) ? 0x80 : 0xC0;
    position += column;
    LCD_command(position);
}

void LCD_print(const char *string) {
    while (*string) {
        LCD_data(*string++);
    }
}

void UART_Init() {
    TRISC6 = 0;  // RC6 como salida (TX)
    TRISC7 = 1;  // RC7 como entrada (RX)
    SPBRG = 25;  // Baud rate 9600 para FOSC = 4MHz
    TXSTAbits.BRGH = 0; // Baud rate de baja velocidad
    TXSTAbits.SYNC = 0; // Modo asíncrono
    RCSTAbits.SPEN = 1; // Habilita el puerto serie (TX y RX)
    TXSTAbits.TXEN = 1; // Habilita la transmisión
    RCSTAbits.CREN = 1; // Habilita la recepción continua

    // Habilita las interrupciones
    PIE1bits.RCIE = 1;  // Habilita la interrupción de recepción serie
    INTCONbits.PEIE = 1; // Habilita las interrupciones periféricas
    INTCONbits.GIE = 1;  // Habilita las interrupciones globales
}

void UART_Write(char data) {
    while (!TXSTAbits.TRMT);  // Espera a que el registro esté vacío
    TXREG = data;  // Escribe el dato en el registro
}

char UART_Read() {
    while (!PIR1bits.RCIF);  // Espera a que el dato esté disponible
    return RCREG;  // Lee el dato del registro
}

void __interrupt() ISR(void) {
    if (PIR1bits.RCIF) {
        char temp = RCREG;  // Leer el dato recibido
        if (dataIndex < MAX_CHARS) {
            receivedData[dataIndex++] = temp;
            if (temp == '\0' || dataIndex == MAX_CHARS) {
                receivedData[dataIndex] = '\0';
                LCD_clear();
                LCD_setCursor(0, 0);
                LCD_print(receivedData);
                dataIndex = 0;  // Reiniciar índice para la próxima cadena
                __delay_ms(1000);
                LCD_clear();
            }
        }
    }
}

void main(void) {
    LCD_init();
    UART_Init();
    LCD_clear();
    LCD_setCursor(0, 0);
    LCD_print("ESPERANDO MSG");
    // Bucle principal vacío; la recepción se maneja en la ISR
    while (1) {
        // Aquí se pueden agregar otras funcionalidades si es necesario
    }
}
