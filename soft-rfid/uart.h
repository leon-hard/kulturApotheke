#ifndef F_CPU
#define F_CPU 16000000L    // Systemtakt in Hz, das L am Ende ist wichtig, NICHT UL verwenden! 
#endif
 
#define BAUD 38400L          // Baudrate, das L am Ende ist wichtig, NICHT UL verwenden!
 
// Berechnungen
#define UBRR_VAL ((F_CPU+BAUD*8)/(BAUD*16)-1)   // clever runden
#define BAUD_REAL (F_CPU/(16*(UBRR_VAL+1)))     // Reale Baudrate
#define BAUD_ERROR ((BAUD_REAL*1000)/BAUD-1000) // Fehler in Promille 
 
#if ((BAUD_ERROR>10) || (BAUD_ERROR<-10))
  #error Systematischer Fehler der Baudrate grösser 1% und damit zu hoch! 
#endif

int uart_putc(unsigned char c) ;
void uart_puts (char *s);
void uart_init(void);

