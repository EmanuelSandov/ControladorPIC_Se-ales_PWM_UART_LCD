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

// Definiciones de las señales analógicas
#define Senal1 PORTAbits.RA0
#define Senal2 PORTAbits.RA1
#define Senal3 PORTAbits.RA2

// Definición del LED de alerta
#define ANLED PORTCbits.RC0

// Definiciones de los módulos PWM
#define PWM1 PORTCbits.RC1
#define PWM2 PORTCbits.RC2

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

#define MAX_CHARS 18 // Máximo número de caracteres a mostrar
#define MAX_RECEIVE_LENGTH 15

// Funciones
void adc_init(void);
unsigned int readADC(unsigned char channel);

// Declaraciones de funciones para el LCD
void LCD_command(unsigned char command);
void LCD_data(unsigned char data);
void LCD_init(void);
void LCD_clear(void);
void LCD_setCursor(unsigned char row, unsigned char column);
void LCD_print(const char *string);
// Declaraciones de funciones para el teclado matricial
void Keyboard_init(void);
char Read_Keypad(void);
// Funciones auxiliares para manejar los pines compartidos
void Prepare_LCD_Pins(void);
void Prepare_Keypad_Pins(void);

void display_signals(unsigned int senal1, unsigned int senal2, unsigned int senal3);
void display_number(unsigned int number);
void update_reference(unsigned int *ref);
void pwm_init(void);
void UART_Init();
void UART_Write(char data);
void UART_Write_Text(const char* text);
void UART_Write_Number(unsigned int value);
void set_pwm_duty_cycle(unsigned int duty_cycle);
void set_pwm_frequency(unsigned int frequency);

unsigned int scale_value(unsigned int value, unsigned int max);
const unsigned char teclas[4][4] = {
    {'7', '8', '9', '/'},
    {'4', '5', '6', 'X'},
    {'1', '2', '3', '-'},
    {'C', '0', '=', '+'}
};

unsigned char cursorPosition = 0;
char displayString[MAX_CHARS + 1] = "";  // Aumenta el tamaño para manejar el '\0'
unsigned char charCount = 0;
unsigned int newFrequency = 1000;
unsigned int newDutyCycle = 500;

bool Reference = false;
bool Frecuencia = false;
bool cambio = false;
bool cambioF = false;
bool cambioD = false;
unsigned int currentReference = 0;
unsigned int ref1 = 10;
unsigned int ref2 = 10;
unsigned int ref3 = 10;


void UART_Init() {
    TRISC6 = 0;  // RC6 como salida (TX)
    TRISC7 = 1;  // RC7 como entrada (RX)
    SPBRG = 25;  // Baud rate 9600 para FOSC = 4MHz
    TXSTAbits.BRGH = 0; // Baud rate de baja velocidad
    TXSTAbits.SYNC = 0; // Modo asíncrono
    RCSTAbits.SPEN = 1; // Habilita el puerto serie (TX y RX)
    TXSTAbits.TXEN = 1; // Habilita la transmisión
    RCSTAbits.CREN = 1; // Habilita la recepción continua
}

void UART_Write(char data) {
    while (!TXSTAbits.TRMT);  // Espera a que el registro esté vacío
    TXREG = data;  // Escribe el dato en el registro
}

void UART_Write_Text(const char *text) {
    while (*text) {
        UART_Write(*text++);
    }
    UART_Write('\0');
}

void UART_Write_Number(unsigned int value) {
    char digit;
    if (value == 0) {
        UART_Write('0');
        return;
    }
    char buffer[10];
    int i = 0;
    while (value > 0) {
        buffer[i++] = (value % 10) + '0';
        value /= 10;
    }
    while (i > 0) {
        UART_Write(buffer[--i]);
    }    
}

unsigned int scale_value(unsigned int value, unsigned int max) {
    if (max == 0) {
        return 0; // Para evitar división por cero
    }
    return (value * 1023) / max;
}

void pwm_init(void) {
    PR2 = 0xFF; // Periodo del PWM
    CCP1CONbits.CCP1M = 0b1100; // Configurar CCP1 en modo PWM
    CCP2CONbits.CCP2M = 0b1100; // Configurar CCP2 en modo PWM
    T2CONbits.T2CKPS = 0b11; // Prescaler 1:16
    T2CONbits.TMR2ON = 1; // Encender TMR2
    CCPR1L = 0x00; // Ciclo de trabajo inicial en 0 para CCP1
    CCPR2L = 0x00; // Ciclo de trabajo inicial en 0 para CCP2
    
    TRISAbits.TRISA5 = 0;   // Asegura que el pin RA5 (buzzer) sea salida
    TRISCbits.TRISC0 = 0;   // Asegura que el pin RC0 (LED) sea salida
    TRISCbits.TRISC1 = 0;   // Asegura que el pin RC1 (PWM1) sea salida
    TRISCbits.TRISC2 = 0;   // Asegura que el pin RC2 (PWM2) sea salida
    TRISAbits.TRISA3 = 0;   // Configura el pin RA3 (EN) como salida
    TRISCbits.TRISC3 = 0;   // Asegura que el pin RC3 (PWM2) sea salida
}

void set_pwm_frequency(unsigned int frequency) {
    // Ajustar el periodo del PWM (PR2)
    // Fórmula: PR2 = (_XTAL_FREQ / (4 * TMR2_Prescaler * PWM_Frequency)) - 1
    // Por ejemplo, para 1kHz: PR2 = (4000000 / (4 * 16 * 1000)) - 1 = 62
    PR2 = (_XTAL_FREQ / (4 * 16 * frequency)) - 1;
}

void set_pwm_duty_cycle(unsigned int duty_cycle) {
    // Ajustar el ciclo de trabajo (CCPR1L)
    // Ciclo de trabajo del PWM: duty_cycle = (duty * (PR2 + 1)) / 1024
    unsigned int duty = (duty_cycle * (PR2 + 1)) / 1024;
    CCPR1L = duty; // Los 8 bits más significativos
}

void display_debug_info(unsigned int senal2, unsigned int duty, unsigned int ref2) {
    LCD_clear();
    LCD_setCursor(0, 0);
    LCD_print("S2: ");
    display_number(senal2);
    LCD_setCursor(1, 0);
    LCD_print("D: ");
    display_number(duty);
    LCD_setCursor(0, 8);
    LCD_print("R2: ");
    display_number(ref2);
    __delay_ms(3000);
    LCD_clear();
}
// Configuración del ADC
void adc_init(void) {
    ADCON0 = 0x41;  // ADC encendido, selecciona el canal AN0
    ADCON1 = 0x80;  // Configura AN0, AN1 y AN2 como entradas analógicas
}

unsigned int readADC(unsigned char channel) {
    ADCON0 &= 0xC5;         // Limpiar los bits de selección de canal
    ADCON0 |= (channel<<3); // Seleccionar canal
    __delay_ms(2);          // Tiempo de adquisición
    GO_nDONE = 1;           // Iniciar conversión ADC
    while (GO_nDONE);       // Esperar a que la conversión termine
    return (ADRESH<<8) + ADRESL; // Retornar el resultado
}

// Configuración del TMR0 para el conteo de pulsos
void tmr1_init(void) {
    T1CON = 0b00110001;  // Prescaler 1:8, TMR1 encendido, fuente de reloj interno (FOSC/4)
    TMR1 = 0;            // Reiniciar el TMR1
    PIE1bits.TMR1IE = 1; // Habilitar la interrupción del TMR1
    PIR1bits.TMR1IF = 0; // Limpiar la bandera de interrupción del TMR1
    INTCONbits.PEIE = 1; // Habilitar interrupciones periféricas
    INTCONbits.GIE = 1;  // Habilitar las interrupciones globales
}

void __interrupt() ISR() {
    if (PIR1bits.TMR1IF){
        TMR1 = 0;             // Reiniciar el TMR1
        PIR1bits.TMR1IF = 0;  // Limpiar la bandera de interrupción del TMR1
    }
}

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

    LCD_command(RETURN_HOME);       // Retorna
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
    while(*string) {
        LCD_data(*string++);
    }
}

void Keyboard_init() {
    TRISAbits.TRISA3 = 0;
    TRISA = 0x00; // Asegura que los pines de control del LCD sean salidas
    TRISB = 0x0F; // RB4-RB7 como salidas (filas) y RB0-RB3 como entradas (columnas)
    PORTB = 0xF0; // Asegura que las filas estén en alto
}
// Función para leer el teclado
char Read_Keypad() {
    // Deshabilitar interrupciones globales
    INTCONbits.GIE = 0;

    for(int row = 0; row < 4; row++) {
        // Pone una fila a nivel bajo a la vez
        PORTB = (unsigned char)~(0x10 << row);
        
        // Configura RB0 a RB3 como entradas para leer las columnas
        TRISB = 0x0F;
        
        __delay_ms(20); // Debounce time
        
        for(int col = 0; col < 4; col++) {
            // Si detectamos una entrada en bajo en una de las columnas, se presionó una tecla
            if(!(PORTB & (0x01 << col))) {
                while(!(PORTB & (0x01 << col))); // Espera a que se suelte la tecla
                BUZZER = 1; // Activa el buzzer
                __delay_ms(100); // Duración del sonido del buzzer
                BUZZER = 0; // Apaga el buzzer
                // Configura RB4 a RB7 como salidas antes de continuar
                TRISB = 0xF0; // Esto es necesario para preparar el teclado para la próxima lectura
                
                // Habilitar interrupciones globales nuevamente
                INTCONbits.GIE = 1;

                return teclas[row][col]; // Retorna el carácter correspondiente
            }
        }
        
        // Configura RB4 a RB7 como salidas antes de continuar
        TRISB = 0xF0; // Esto es necesario para preparar el teclado para la próxima lectura
    }

    // Habilitar interrupciones globales nuevamente
    INTCONbits.GIE = 1;

    return 0; // No se presionó ninguna tecla
}

void Prepare_LCD_Pins() {
    // Configura los pines para el uso del LCD antes de usarlo para enviar comandos o datos
    TRISB = 0x00; // Configura PORTB como salida
}

void Prepare_Keypad_Pins() {
    // Configura los pines para el uso del teclado antes de leerlo
    TRISB = 0xF0; // RB0-RB3 como salida (para filas), RB4-RB7 como entrada (para columnas)
    PORTB = 0x0F; // Baja las filas para activar el teclado, activa resistencias de pull-up para columnas
}

void display_number(unsigned int number) {
    if (number == 0) {
        LCD_data('0');
        return;
    }

    char digits[5];
    int i = 0;

    while (number > 0) {
        digits[i++] = (number % 10) + '0';
        number /= 10;
    }

    while (i > 0) {
        LCD_data(digits[--i]);
    }
}

void display_signals(unsigned int senal1, unsigned int senal2, unsigned int senal3) {
    // Mostrar Señal 1
    LCD_clear();
    LCD_setCursor(1, 1);
    Prepare_LCD_Pins();
    LCD_print("Senal 1");
    LCD_setCursor(0, 1);
    display_number(senal1);
    __delay_ms(1000);
    LCD_clear();
    LCD_setCursor(1, 1);
    Prepare_LCD_Pins();
    LCD_print("Ref 1");
    LCD_setCursor(0, 1);
    display_number(ref1);
    __delay_ms(1000);
    
    // Mostrar Señal 2
    LCD_clear();
    LCD_setCursor(1, 0);
    Prepare_LCD_Pins();
    LCD_print("Senal 2");
    LCD_setCursor(0, 1);
    display_number(senal2);
    __delay_ms(1000);
    LCD_clear();
    LCD_setCursor(1, 1);
    Prepare_LCD_Pins();
    LCD_print("Ref 2");
    LCD_setCursor(0, 1);
    display_number(ref2);
    __delay_ms(1000);

    // Mostrar Señal 3
    LCD_clear();
    LCD_setCursor(1, 0);
    Prepare_LCD_Pins();
    LCD_print("\tSenal 3");
    LCD_setCursor(0, 1);
    display_number(senal3);
    __delay_ms(1000);
    LCD_clear();
    LCD_setCursor(1, 1);
    Prepare_LCD_Pins();
    LCD_print("Ref 3");
    LCD_setCursor(0, 1);
    display_number(ref3);
    __delay_ms(1000);
    LCD_clear();
}

void main(void) {
    LCD_init();
    Keyboard_init();
    pwm_init();  // Inicializa el PWM
    adc_init();
    UART_Init();
    
    set_pwm_frequency(1000); // Configura la frecuencia del PWM a 1kHz
    set_pwm_duty_cycle(512); // Configura el ciclo de trabajo al 50%
    
    while (1) {
        Prepare_Keypad_Pins();
        char key = Read_Keypad();  // Lee una tecla presionada

        if (key != 0) {
            if (Reference == true) {
                Prepare_LCD_Pins();
                if (key == '1' && cambio == false) {
                    cambio = true;
                    currentReference = 1;
                    ref1 = 0;  // Reiniciar la referencia antes de capturar
                    LCD_clear();
                    LCD_setCursor(0, 0);
                    LCD_print("Ref1: ");
                } else if (key == '2' && cambio == false) {
                    cambio = true;
                    currentReference = 2;
                    ref2 = 0;  // Reiniciar la referencia antes de capturar
                    LCD_clear();
                    LCD_setCursor(0, 0);
                    LCD_print("Ref2: ");
                } else if (key == '3' && cambio == false) {
                    cambio = true;
                    currentReference = 3;
                    ref3 = 0;  // Reiniciar la referencia antes de capturar
                    LCD_clear();
                    LCD_setCursor(0, 0);
                    LCD_print("Ref3: ");
                } else if (key >= '0' && key <= '9' && cambio == true) {
                    // Mostrar el número en el LCD y actualizar la referencia actual
                    LCD_data(key);
                    if (currentReference == 1) {
                        ref1 = (ref1 * 10) + (key - '0');
                    } else if (currentReference == 2) {
                        ref2 = (ref2 * 10) + (key - '0');
                    } else if (currentReference == 3) {
                        ref3 = (ref3 * 10) + (key - '0');
                    }
                } else if (key == '=' && cambio == true) {
                    Reference = false;
                    cambio = false;
                    LCD_clear();
                    LCD_print("Ref Guardada");
                    __delay_ms(1000); // Delay to show the mode change
                    LCD_clear();
                } 
            } else if (Frecuencia == true) {
                if (key == '1') {
                    cambio = true;
                    Prepare_LCD_Pins();
                    LCD_clear();
                    LCD_setCursor(1, 1);
                    LCD_print("CAMBIO DE");
                    LCD_setCursor(0, 0);
                    LCD_print("FRECUENCIA");
                    cambioF = true;
                    __delay_ms(1000);
                    LCD_clear();
                    LCD_setCursor(1, 1);
                    LCD_print("Subir + ");
                    LCD_print("Bajar -");
                    LCD_setCursor(0, 0);
                    LCD_print("Salir =");
                    __delay_ms(1000);
                    LCD_clear();
                } else if (key == '2' && cambio == false) {
                    cambio = true;
                    Prepare_LCD_Pins();
                    LCD_clear();
                    LCD_setCursor(1, 1);
                    LCD_print("CAMBIO DE");
                    LCD_setCursor(0, 0);
                    LCD_print("PULSO");
                    cambioD = true;
                    __delay_ms(1000);
                    LCD_clear();
                    LCD_setCursor(1, 1);
                    LCD_print("Subir + ");
                    LCD_print("Bajar -");
                    LCD_setCursor(0, 0);
                    LCD_print("Salir =");
                    __delay_ms(1000);
                    LCD_clear();
                }
                
                if (cambioF == true) {
                    if (key == '+') {
                        newFrequency += 100;
                        set_pwm_frequency(newFrequency);
                        Prepare_LCD_Pins();
                        LCD_clear();
                        LCD_setCursor(1, 1);
                        display_number(newFrequency);
                        LCD_setCursor(0, 0);
                        LCD_print("FRECUENCIA");
                        __delay_ms(1000);
                        LCD_clear();
                    } else if (key == '-') {
                        newFrequency -= 100;
                        set_pwm_frequency(newFrequency);
                        Prepare_LCD_Pins();
                        LCD_clear();
                        LCD_setCursor(1, 1);
                        display_number(newFrequency);
                        LCD_setCursor(0, 0);
                        LCD_print("FRECUENCIA");
                        __delay_ms(1000);
                        LCD_clear();
                    } else if (key == '=') {
                        Prepare_LCD_Pins();
                        LCD_clear();
                        LCD_setCursor(1, 1);
                        LCD_print("SALIENDO");
                        __delay_ms(1000);
                        LCD_clear();
                        cambioF = false;
                        Frecuencia = false;
                        cambio = false;
                    }
                } else if (cambioD == true) {
                    if (key == '+') {
                        newDutyCycle += 10;
                        set_pwm_duty_cycle(newDutyCycle);
                        Prepare_LCD_Pins();
                        LCD_clear();
                        LCD_setCursor(1, 1);
                        display_number(newDutyCycle);
                        LCD_setCursor(0, 0);
                        LCD_print("PULSO");
                        __delay_ms(1000);
                        LCD_clear();
                    } else if (key == '-') {
                        newDutyCycle -= 10;
                        set_pwm_duty_cycle(newDutyCycle);
                        Prepare_LCD_Pins();
                        LCD_clear();
                        LCD_setCursor(1, 1);
                        display_number(newDutyCycle);
                        LCD_setCursor(0, 0);
                        LCD_print("PULSO");
                        __delay_ms(1000);
                        LCD_clear();
                    } else if (key == '=') {
                        Prepare_LCD_Pins();
                        LCD_clear();
                        LCD_setCursor(1, 1);
                        LCD_print("SALIENDO");
                        __delay_ms(1000);
                        LCD_clear();
                        cambioD = false;
                        Frecuencia = false;
                        cambio = false;
                    }
                }
            } else {
                if (key == '+') {
                    Reference = true;
                    Prepare_LCD_Pins();
                    LCD_clear();
                    LCD_setCursor(1, 1);
                    LCD_print("Modo cambio de");
                    LCD_setCursor(0, 1);
                    LCD_print("Referencia");
                    __delay_ms(1000);
                    LCD_clear();
                    LCD_print("1) Ref1   2)Ref2");
                    LCD_setCursor(0, 1);
                    LCD_print("3) Ref3");
                    __delay_ms(1000);
                    LCD_clear();
                } else if (key == '-') {
                    // Medición de las señales analógicas
                    unsigned int senal1 = readADC(0); // Leer canal AN0
                    unsigned int senal2 = readADC(1); // Leer canal AN1
                    unsigned int senal3 = readADC(2); // Leer canal AN2

                    // Mostrar las señales en el LCD
                    display_signals(senal1, senal2, senal3);
                } else if (key == 'X') {
                    // Medición de las señales analógicas
                    unsigned int senal1 = readADC(0); // Leer canal AN0
                    unsigned int senal2 = readADC(1); // Leer canal AN1
                    unsigned int senal3 = readADC(2); // Leer canal AN2
                    Prepare_LCD_Pins();
                    LCD_clear();
                    LCD_setCursor(1, 1);
                    LCD_print("ENVIANDO MSG");
                    UART_Write_Number(senal1);
                    __delay_ms(500);
                    UART_Write_Text(": SENAL 1");
                    __delay_ms(1000);
                    UART_Write_Number(senal2);
                    __delay_ms(500);
                    UART_Write_Text(": SENAL 2");
                    __delay_ms(1000);
                    UART_Write_Number(senal3);
                    __delay_ms(500);
                    UART_Write_Text(": SENAL 3");
                    __delay_ms(1000);
                    UART_Write_Number(ref1);
                    __delay_ms(500);
                    UART_Write_Text(": REF 1");
                    __delay_ms(1000);
                    UART_Write_Number(ref2);
                    __delay_ms(500);
                    UART_Write_Text(": REF 2");
                    __delay_ms(1000);
                    UART_Write_Number(ref3);
                    __delay_ms(500);
                    UART_Write_Text(": REF 3");
                    LCD_clear();
                } else if (key == '/') {
                    Frecuencia = true;
                    Prepare_LCD_Pins();
                    LCD_clear();
                    LCD_setCursor(1, 1);
                    LCD_print("Modo cambio de");
                    LCD_setCursor(0, 1);
                    LCD_print("Frecuencia y ");
                    __delay_ms(1000);
                    LCD_clear();
                    LCD_setCursor(1, 1);
                    LCD_print("Ancho de pulso");
                    __delay_ms(1000);
                    LCD_clear();
                    LCD_print("1) FRECUENCIA ");
                    LCD_setCursor(0, 1);
                    LCD_print("2) PULSO ");
                    __delay_ms(1000);
                    LCD_clear();
                }else {
                    switch (key) {
                        case 'C':
                            Prepare_LCD_Pins();
                            LCD_clear();
                            LCD_setCursor(1,1);
                            LCD_print("+)Cambiar datos");
                            LCD_setCursor(0,0);
                            LCD_print("-)Mostrar datos");
                            __delay_ms(2000);
                            LCD_clear();
                            LCD_setCursor(1,1);
                             LCD_print("X)Enviar datos");
                            LCD_setCursor(0,0);
                            LCD_print("/)Frecuencia");
                            __delay_ms(2000);
                            LCD_clear();
                            break;
                        default:
                            if (charCount < MAX_CHARS - 1) {
                                for (int i = charCount; i > cursorPosition; i--) {
                                    displayString[i] = displayString[i - 1];
                                }
                                displayString[cursorPosition] = key;
                                if (cursorPosition < MAX_CHARS - 1) cursorPosition++;
                                if (charCount < MAX_CHARS) charCount++;
                            }
                            break;
                    }

                    // Actualización del LCD después de la inserción
                    Prepare_LCD_Pins();
                    LCD_clear();
                    LCD_setCursor(0, 0);
                    LCD_print(displayString);
                    LCD_setCursor(0, cursorPosition);  // Asegura que el cursor esté en la posición correcta después de la inserción
                }
            }
        } 

        unsigned int senal1 = readADC(0);
        unsigned int senal2 = readADC(1);
        unsigned int senal3 = readADC(2); // Leer canal AN2

        // Escala de senal2 al rango de 0 a ref2
        unsigned int scaled_value1 = scale_value(senal2, ref2);
          
        // Control del LED acua mediante PWM1 basado en senal2 escalada
        CCPR1L = scaled_value1 >> 2; // Ajuste del ciclo de trabajo
          
        if (senal1 > ref1) {
            ANLED = 1;
        } else {
            ANLED = 0;
        }
          
        if (senal3 > ref3) {
            BUZZER = 1;
        } else {
            BUZZER = 0;
        }

        int pwm = 1;
        if (pwm == 1) {
            PORTAbits.RA3 = 1; 
        } else {
            PORTAbits.RA3 = 0;
        }

        __delay_ms(100); // Debounce delay para evitar lecturas múltiples
    }
}
