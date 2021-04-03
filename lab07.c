/* CONFIGURACAO DO TIMER:
O programa deve fazer com que o led conectado ao pino 12 troque de estado a cada 0,78 segundos. Ou seja, 780 ms. E o led conectado a placa deve trocar de estado a cada 0,5 segundos. Ou seja, 500 ms.
A frequencia do clock e de f = 16 MHz. Alem disso, no modo de operacao normal, o overflow e o envio do sinal top acontece depois de uma contagem de R = 255 do registrador (2^n - 1. Sendo n = 8 e o numero de bits do timer 2).
Assim, o maximo intervalo de tempo que o temporizador consegue medir e dado por:
faixa = P(R + 1)/f = 1024(255 + 1)/16*10^6 = 16,384 ms. 
A princípio, e possivel definir variaveis de contagem em uma interrupcao que ocorre toda vez que um sinal top e enviado, de modo que apos uma certa quantidade de ciclos, os leds sejam piscados. No entanto, esse valor de faixa nao e um divisor de 780 e 500 ms.
Diante disso, uma saida e utilizar um registrador de comparacao. Assim, o sinal top sera enviado toda vez que for atingida a contagem no valor armazenado nesse registrador. Dessa forma, e possivel manipular os valores e conseguir um divisor dos dois valores. 
Utilizando um prescaler de 256 e uma contagem R' = 249, tem-se o seguinte valor de faixa:
faixa = 256*(249 + 1)/16*10^6 = 4 ms. 
O valor de contagem de 249 significa que um sinal top sera enviado depois de 250 contagens, ja que esta comeca em zero. 
Dessa forma, o temporizador mede um intervalo de tempo de 4 ms. A cada intervalo de tempo desses, uma interrupcao do tipo Timer/Counter2 Compare Match A acontece. 
Para atingir o valor de 780 ms, e necessario passar por 195 ciclos do temporizador, pois 195*4 = 780 ms. Assim, define-se a variavel led12 para contar 195 ciclos dentro da interrupcao, uma vez que o acontecimento de uma interrupcao indica a ocorrencia de um ciclo.
Para atingir o valor de 500 ms, e necessario passar por 195 ciclos do temporizador, pois 125*4 = 500 ms. Assim, define-se a variavel led12 para contar 125 ciclos dentro da interrupcao, uma vez que o acontecimento de uma interrupcao indica a ocorrencia de um ciclo.
*/

/* Define a frequencia do clock. */
#define F_CPU 16000000UL 

/* Bibliotecas necessarias para o funcionamento do programa. A primeira e uma biblioteca basica, a segunda permite o uso do delay e a terceira, de interrupções. */
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

unsigned char *ocr2a; /* Ponteiro associado ao registrador OCR2A, output compare register, do timer 2, que armareza o valor de comparacao. */
unsigned char *tccr2a; /* Ponteiro associado ao registrador TCCR2A, Timer/Counter Control Register, do timer 2, que define o modo de operacao e o prescaler. */
unsigned char *tccr2b; /* Ponteiro associado ao registrador TCCR2B, Timer/Counter Control Register B, do timer 2, que seta os pinos de comparacao e define o modo de operacao. */
unsigned char *timsk2; /* Ponteiro associado ao registrador TIMSK2, Timer Interrupt Mask Register, do timer 2, para setar interrupcoes. */
unsigned char *p_portb; /* Ponteiros associados aos registradores PORTB */
unsigned char *p_ddrb; /* Ponteiros associados aos registradores PORTB */
unsigned char *ubrr0h; /* UBRR0X sao registradores para a definicao de baud rate, que e a velocidade com que a transmissao e feita. */
unsigned char *ubrr0l; /* UBRR0X sao registradores para a definicao de baud rate, que e a velocidade com que a transmissao e feita. */
unsigned char *ucsr0a; /* UCSR0X sao registradores de configuracao e status da USART */
unsigned char *ucsr0b; /* UCSR0X sao registradores de configuracao e status da USART */
unsigned char *ucsr0c; /* UCSR0X sao registradores de configuracao e status da USART */
volatile char *udr0; /* Ponteiro do registrador de dados da USART0. Esta sendo definido como char pois recebera as strings de mensagem definidas como char. E como volatile pois e usado na interrupcao e em outras funcoes. */

volatile int led12; /* Variavel de contagem do led conectado no pino 12. Conta a quantidade de ciclos do temporizador que ocorrem antes de mudar o estado desse led. */
volatile int led13; /* Variavel de contagem do led conectado no pino 13. Conta a quantidade de ciclos do temporizador que ocorrem antes de mudar o estado desse led. */
volatile int sinalizacao; /* Variavel que sinaliza o fim da transmissao da mensagem. */
char msg[] = "Atividade 7 - Interrupcoes temporizadas tratam concorrencia entre tarefas! \n\n"; /* String que armazena a mensagem a ser exibida no monitor serial. */
volatile char *m = &(msg[0]); /* Ponteiro que aponta para o endereço do inicio da string de mensagem a ser exibida no monitor serial. */

/* Funcao que configura os perifericos. */
void setup() {
  
  cli(); /* Desabilita todas as interrupcoes para que elas nao ocorram sem que os perifericos estejam configurados. */

  /* Atribui ao ponteiro o endereco do registrador OCR2A. */
  ocr2a = (unsigned char *)0xB3;
  /* O registrador armazena o valor em binario do valor maximo de contagem. No caso, 249. Em binario, 11111001. */
  *ocr2a = 0b11111001;  
  /* Atribui ao ponteiro o endereco do registrador TCCR2A. */
  tccr2a = (unsigned char *)0xB0;
  /* O registrador e configurado da seguinte forma:
  7, 6, 5, 4: bits que controlam o pino de output compare. Como os pinos nao sao usados, sao setados como zero para desconectar os pinos. 
  3, 2: bits reservados. não sao setados.
  1, 0: WGM21 e WGM20: junto ao bit WGM22, no registrador TCCR2B, definem o modo de operacao. Para o modo de operacao CTC, usando o registrador OCR2A, eles sao configurados como 010. 
  Portanto, tem-se a seguinte configuracao: 0000xx10 */
  *tccr2a &= 0xE; /* Reseta os bits que devem ser iguais a zero, usando o comparativo 'e'. */
  *tccr2a |= 0x2; /* Seta o bit que deve ser setado como um, usando o comparativo 'ou'. */
  /* Atribui ao ponteiro o endereco do registrador TCCR2B. */
  tccr2b = (unsigned char *)0xB1;
  /* O registrador e configurado da seguinte forma: 
  7, 6: bits associados ao modo de force output compare. Para desabilitar esse modo, os bits sao setados como zero. 
  5, 4: bits reservados. não sao setados.
  3: bit WGM22. Como dito anteriormente, esta associado ao modo de operacao e e setado como zero. 
  2, 1, 0: bits CS22, CS21, CS20. Definem o valor a ser usado no prescaler. No caso, 256. Para tanto, sao setados como 110.
  Portanto, tem-se a seguinte configuracao: 00xx0110. */
  *tccr2b &= 0x36; /* Reseta os bits que devem ser iguais a zero, usando o comparativo 'e'. */
  *tccr2b |= 0x6; /* Seta o bit que deve ser setado como um, usando o comparativo 'ou'. */
  /* Atribui ao ponteiro o endereco do registrador TIMSK2. */
  timsk2 = (unsigned char *)0x70;
  /*O registrador e configurado da seguinte forma:
  7, 6, 5, 4, 3: bits reservados. nao sao setados. 
  2: bit OCIE2B. Habilita/desabilita a interrupcao de comparacao com o registrador B. Para desabilita-la, e setado como zero. 
  1: bit OCIE2A. Habilita/desabilita a interrupcao de comparacao com o registrador A. Para habilita-la, e setado como um.
  0: bit TOIE2. Habilita/desabilita a interrupcao de overflow. Para desabilita-la, e setado como zero.
  Portanto, tem-se a configuracao: xxxxx010. */
  *timsk2 &= 0xFA; /* Reseta os bits que devem ser iguais a zero, usando o comparativo 'e'. */
  *timsk2 |= 0x2; /* Seta o bit que deve ser setado como um, usando o comparativo 'ou'. */
  /* Atribui ao ponteiros os endereços dos registradores PORTB */
  p_portb = (unsigned char *)0x25; 
  p_ddrb = (unsigned char *)0x24;
  /* Seta como zero os bits 4 e 5 do registrador portb. Dessa forma, eles iniciam desligados. */
  *p_portb &= ~0x30;
  /* Seta como um os bits 4 e 5 do registrador ddrb. Dessa forma, eles sao configurados como saida. */
  *p_ddrb |= 0x30;
  /* Atribui ao ponteiro o endereco do registrador UDR0. */
  udr0 = (char *) 0xC6;
  /* Atribui aos ponteiros os endereços dos registradores UBRR0X. */
  ubrr0h = (unsigned char *) 0xC5;
  ubrr0l = (unsigned char *) 0xC4;
  /* Para um baud rate de 19.2 kbps, UBRR0X = 51. Em binario, 0000 0000 0101 0001. Os oito primeiros bits correspondem ao UBRR0H e os oito ultimos ao UBRR0L.  */
  *ubrr0h = 0b00000000;  /* Aqui, todos os bits estao sendo usados. Por isso esta sendo usado o = ao inves de uma mascara. */
  *ubrr0l = 0b01010001;  /* Aqui, todos os bits estao sendo usados. Por isso esta sendo usado o = ao inves de uma mascara. */
  /* Atribi aos ponteiros os endereços dos registradores UCSR0X. */
  ucsr0a = (unsigned char *) 0xC0; 
  ucsr0b = (unsigned char *) 0xC1;
  ucsr0c = (unsigned char *) 0xC2;
  /* Os bits do registrador UCSR0A correspondem a: 
  7 - RXCO: uma flag que indica que existem dados nao lidos no buffer de recepcao. Como o programa envolve apenas uma transmissao, ele nao sera usado. 
  6 - TXCO: uma flag setada quando todo o dado de transmissao foi enviado. E usada ao longo do processo de transmissao. Inicialmente, nao esta setada. 
  5 - UDRE0: flag que indica se o buffer de transmissao esta pronto para receber novos dados. E um bit a ser lido, logo nao e setado inicialmente. 
  4, 3, 2 - FE0, DOR0, UPE0: flas que indicam, respectivamente, erro no frame recebido, overrun e erro de paridade. Tambem sao bits a serem lidos e nao sao setados inicialmente.
  1 - U2X0: quando igual a 1, a taxa de transmissao e dobrada. Como se quer uma velocidade de transmissao normal, ele e setado como 0.
  0 - MPCM0: ativa o modo multiprocessador. Como se deseja desabilita-lo, o bit e setado como 0. */
  *ucsr0a &= 0xFC;
  /* Os bits do registrador UCSR0B correspondem a:
  7, 6, 5 - RXCIE0, TXCIE0, UDRIE0: esses bits habilitam/desabilitam interrupcoes. Como se deseja habilitar apenas a interrupcao de buffer vazio, o bit 5 e setado como 1 e os outros dois, como zero. 
  4 - RXEN0: habilia/desabilita o receptor, logo e setado como zero para desabilita-lo, ja que o programa envolve apenas transmissao. 
  3 - TXEN0: habilia/desabilita o transmissor, logo e setado como um para habilita-lo, ja que o programa envolve uma transmissao.
  2 - UCSZ02: esse bit, em conjunto com UCSZ01 e UCSZ00 selecionam quantos bits de dados serao transmitidos em cada frame. Como sao 8 bits, UCSZ0X e setado como 011. Logo, esse bit e setado como zero. 
  1, 0 - RXB80, TXB80: sao usados quando se trata de uma transmissao com 9 bits. Logo, nao sao utilizados nessa transmissao. */
  *ucsr0b &= 0x2B; /* Reseta os bits que devem ser iguais a 0. */
  *ucsr0b |= 0x28; /* Seta o bit que deve ser igual a 1. */
  /*Os bits do registrador UCSR0C correspondem a:
  7, 6 - UMSEL01, UMSEL00: definem o modo de operação da USART. Para que ela funcione em modo assincrono, os bits devem ser setados como 00.
  5, 4 - UPM01, UPM00: definem o uso (ou não) do bit de paridade e o seu tipo. Aqui nao sera usado bit de paridade, logo os bits sao setados como 00.
  3 - USBS0: esta relacionado aos bits de parada. E igual a 0 quando há um único bit de parada em cada frame e igual a 1 quando dois bits de parada são utilizados. Nessa transmissao, e setado como 0.
  2, 1 - UCSZ01, UCSZ00: esses bits, em conjunto com UCSZ02 selecionam quantos bits de dados serao transmitidos em cada frame. Como sao 8 bits, UCSZ0X e setado como 011. Logo, esses bits sao setados como 11.
  0 - UCPOL0: deve ser igual a zero para transmissao assíncrona.*/
  *ucsr0c = 0b00000110; /* Aqui, todos os bits estao sendo usados. Por isso esta sendo usado o = ao inves de uma mascara. */

}

/* Rotina de interrupcao de buffer vazio. */
ISR(USART_UDRE_vect){

  /* Entra na rotina se a mensagem ainda nao foi completamente transmistida. */
  if(*m != '\0') {

    *udr0 = *m; /* Envia o caractere apontado pelo ponteiro. */
    m++; /* Faz o ponteiro apontar para o proximo caractere. */
  }
  else { /* Entra na rotina quando a mensagem e completamente transmitida. */
  
    *ucsr0b &= 0xDF; /* Desabilita a interrupao para que nao sejam enviados caracteres indesejados. */
    sinalizacao = 1; /* Seta como um a flag que sinaliza o fim da transmissao da mensagem. */
    m = &(msg[0]); /* O ponteiro aponta novamente para o inicio da mensagem. */
    
    
  }
  
}

/* Rotina de interrupcao de comparacao com o registrador A. */
ISR(TIMER2_COMPA_vect) {

  led12++; /* Toda vez que a interrupcao ocorre, ou seja, que um ciclo do timer e completado, incrementa o valor desta variavel e da seguinte. */
  led13++;
  if (led12 == 195) { /* Quando a variavel atinge o valor de 195, o estado do led 12 e alterado e a variavel e zerada para que a contagem se inicie novamente. */
  
    *p_portb ^= 0x10; /* O operador xor altera o estado do led conectado no pino 12, controlado pelo bit 4. Se ele esta ligado, desliga; e se esta desligado, liga. */
    led12 = 0; /* Zera a variavel para que a contagem se inicie novamente. Apos atingir outra vez o valor de 195, o estado do led e mais uma vez alterado. */
  }
  if (led13 == 125) { /* Quando a variavel atinge o valor de 125, o estado do led 13 e alterado e a variavel e zerada para que a contagem se inicie novamente. */
  
    *p_portb ^= 0x20; /* O operador xor altera o estado do led conectado no pino 13, que e o led da placa, controlado pelo bit 5. Se ele esta ligado, desliga; e se esta desligado, liga. */
    led13 = 0; /* Zera a variavel para que a contagem se inicie novamente. Apos atingir outra vez o valor de 125, o estado do led e mais uma vez alterado. */
  }
}

int main(){
  /* Inicializa os perifericos */
  setup();
  /* Inicializa a flag de sinalizacao como zero. */
  sinalizacao = 0;
  /* Inicializa as variaveis de contagem em zero. */
  led12 = 0;
  led13 = 0;
  /* Habilita as interrupcoes. */
  sei();

 /* Looping infinito. */
  while (1) {
  
    if (sinalizacao == 1) { /* Entra na rotina quando a sinalizacao e igual a um, ou seja, a mensagem terminou de ser transmitida. */
    
      _delay_ms(5000); /*Espera 5 segundos antes de iniciar uma nova transmissao. */
      sinalizacao = 0; /* Reseta a flag de mensagem transmitida. */
      *ucsr0b |= 0x20; /* Habilita novamente a interrupcao de buffer vazio. */    
        
    }
    
    
    
  } 
  }