#include <p18f452.h>
#include <stdint.h>
#include "config.h"
#include "st7920.h"

#define TIMER_PRESET 0
#define BALL_SIZE 1
#define BALL_MAX_SPEED 10
#define EVENT_WAIT_RESET 100
#define PADDLE_SIZE_X 1
#define PADDLE_SIZE_Y 6
#define IA_TRIGGER 40
#define WAIT_RESET 1000

// EEPROM ADDRESS FOR XORSHIFT
#define ADDR_A 0xF0
#define ADDR_B 0xF1
#define ADDR_C 0xF2
#define ADDR_Y 0xF3

struct coord
{
  int x, y;
};

volatile struct coord ball_position;
volatile struct coord ball_speed;
volatile struct coord ball_wait;
struct coord ball_draw;

volatile int paddle_position[2] = {0, 0};
volatile int paddle_speed[2];
volatile int paddle_wait[2];
int paddle_draw[2];
int score[2] = {0, 0};
int reset = 1;

// Pseudo random generator
volatile uint8_t seed_a, seed_b, seed_c, seed_y;

void writeEEP(char addr, char data){
    EEADR=addr; // Set address
    EEDATA=data; // Sets data to write
    EEPGD=0; // Writing Data EEPROM
    CFGS=0; // Not writing Config.
    // DISABLE INTERRUPTS
    GIE = 0;
    WREN=1; // Enable write
    EECON2 = 0x55;
    EECON2 = 0xAA;
    EECON1bits.WR=1; // Triggering wire
    while(EECON1bits.WR==1);
    WREN=0; // Disable write
    // ENABLE INTERRUPTS
    GIE = 1;
}

char readEEP(char addr){
    EEADR=addr; // Set address
    EEPGD=0; // Writing Data EEPROM
    CFGS=0; // Not writing Config.
    EECON1bits.RD=1;
    while(EECON1bits.RD==1);
    return EEDATA;
}

void drawField(){
    int i;
    for(i=0; i<128; i++){
        setPixel(i,0,1);
        setPixel(i,63,1);
    }

    for(i=0; i<64; i++){
        if(i%3==0)
            setPixel(64,i,1);
    }
}

void drawScore(int a, int b){
    disableUpdate();
    drawNumber(58,3,a);
    drawNumber(67,3,b);
    enableUpdate();
}

void drawBall(){
    int i, j;
    for(j=ball_draw.y; j<ball_draw.y+BALL_SIZE; j++){
        for(i=ball_draw.x; i<ball_draw.x+BALL_SIZE; i++){
            if((i!=64))
                setPixel(i,j,0);
        }
    }

    ball_draw=ball_position;

    for(j=ball_draw.y; j<ball_draw.y+BALL_SIZE; j++){
        for(i=ball_draw.x; i<ball_draw.x+BALL_SIZE; i++){
            if((i!=64))
                setPixel(i,j,1);
        }
    }
}

void drawPaddle(){
    int i, j, k;
    for(k=0; k<2; k++){
        int x_start=k==0?0:128-PADDLE_SIZE_X;
        int x_end=k==0?PADDLE_SIZE_X:128;
        for(j=paddle_draw[k]; j<paddle_draw[k]+PADDLE_SIZE_Y; j++)
            for(i=x_start; i<x_end; i++)
                setPixel(i,j,0);

        paddle_draw[k]=paddle_position[k];

        for(j=paddle_draw[k]; j<paddle_draw[k]+PADDLE_SIZE_Y; j++)
            for(i=x_start; i<x_end; i++)
                setPixel(i,j,1);
    }
}

int abs(int v) {
  return v * ( (v<0) * (-1) + (v>0));
}

int xorshift() {
    signed int seed_t = seed_a ^ (seed_a << 4);
    seed_a=seed_b;
    seed_b=seed_c;
    seed_c=seed_y;
    return seed_y = seed_y ^ (seed_y >> 2) ^ seed_t ^ (seed_t >> 8);
}

void setup(){
    // Wait for PLL lock
    __delay_ms(2);
    
    // PortB and PortD as output
    TRISB = 0;
    TRISD = 0;
    // PortA as input;
    TRISA = 1;

    // Timer0 setup
    TMR0ON = 1;  // Start Timer0
    T08BIT = 1; // Timer0 8 bit mode
    T0CS = 0; // Internal Clock Source
    PSA = 0;  // Prescaler enabled
    T0PS0 = 0; // Prescaler
    T0PS1 = 1;
    T0PS2 = 1;

    // ADC setup
    PCFG3=0;
    PCFG2=0;
    PCFG1=0;
    PCFG0=0;
    CHS0=0; // Select channel 0
    CHS1=0;
    CHS2=0;
    ADCS2=0; // Select frequency
    ADCS1=0;
    ADCS0=1;
    ADFM=1; // Left Justified
    ADON=1; // Enable Conversion


    // Interrupt setup
    IPEN = 0; // Interrupt Priority Disabled
    GIE = 1;  // General Interrupt Enable
    PEIE = 1; // Periferical Intterupt Enabled
    TMR0IE = 1; // Timer 0 Interrupt Enabled
    TMR0IF=0; // Timer 0 Interrupt Flag
    
    // Reads EEPROM to keep randmomness
    seed_a=readEEP(ADDR_A);
    seed_b=readEEP(ADDR_B);
    seed_c=readEEP(ADDR_C);
    seed_y=readEEP(ADDR_Y);

    // If data is
    if(seed_a==0xFF && seed_b==0xFF && seed_c==0xFF && seed_y==0xFF){
        seed_a=0x17;
        seed_b=0x8D;
        seed_c=0xA9;
        seed_y=0x33;
    }

    enableUpdate();
    startScreen();
    clearScreen();
}

void loop(void){
    if(reset==1){
        paddle_position[0]=28;
        paddle_position[1]=28;
        ball_position.x=64;
        ball_position.y=32;
        ball_speed.x=(CHECK_BIT(seed_y, 7)?-1:1)*((xorshift()%4)+3);
        ball_speed.y=(CHECK_BIT(seed_y, 7)?-1:1)*((xorshift()%4)+3);
        ball_wait.x=EVENT_WAIT_RESET;
        ball_wait.y=EVENT_WAIT_RESET;
        drawBall();
        drawPaddle();
        drawField();
        drawScore(score[0],score[1]);
        refreshScreen();        

        for(int i=0; i<WAIT_RESET; i++)
            __delay_ms(1);
        
        reset=0;
    }

    drawBall();
    drawPaddle();
    drawScore(score[0],score[1]);

    smartRefresh();

    if(ball_position.x%16==0){
        writeEEP(ADDR_A, seed_a);
        writeEEP(ADDR_B, seed_b);
        writeEEP(ADDR_C, seed_c);
        writeEEP(ADDR_Y, seed_y);
    }

    return;
}

void main(void) {
    setup();

    while(1){
        loop();
    }

    return;
}

void interrupt T0Interrupt(void){
    if(TMR0IF){
        TMR0IF=0;
        TMR0=TIMER_PRESET;
        if(reset==0){
            // Increase randomness
            xorshift();

            struct coord ball_next = ball_position;

            ball_wait.x-=abs(ball_speed.x);
            ball_wait.y-=abs(ball_speed.y);

            if(ball_wait.x<=0){
                ball_wait.x=EVENT_WAIT_RESET;
                if(ball_speed.x>0){
                    ball_next.x=ball_position.x+1;
                }else if(ball_speed.x<0){
                    ball_next.x=ball_position.x-1;
                }else{
                    ball_next.x=ball_position.x;
                }
            }
            if(ball_wait.y<=0){
                ball_wait.y=EVENT_WAIT_RESET;
                if(ball_speed.y>0){
                    ball_next.y=ball_position.y+1;
                }else if(ball_speed.y<0){
                    ball_next.y=ball_position.y-1;
                }else{
                    ball_next.y=ball_position.y;
                }
            }

            // Check collision else update normally
            if(ball_next.x<1){
                    ball_next.x=PADDLE_SIZE_X+1;
                    ball_speed.x*=-1;
                if(ball_position.y>=paddle_position[0] && ball_position.y<paddle_position[0]+PADDLE_SIZE_Y){
                    // Paddle HIT!
                }else{
                    score[1]++;
                    reset=1;
                    return;
                }
            }
            if(ball_next.x>126){
                    ball_next.x=127-PADDLE_SIZE_X-1;
                    ball_speed.x*=-1;
                if(ball_position.y>=paddle_position[1] && ball_position.y<paddle_position[1]+PADDLE_SIZE_Y){
                    // Paddle HIT!
                }else{
                    score[0]++;
                    reset=1;
                    return;
                }
            }

            if(ball_next.y<=0){
                ball_next.y=1;
                ball_speed.y*=-1;
            }
            if(ball_position.y>=63-BALL_SIZE){
                ball_next.y=62-BALL_SIZE;
                ball_speed.y*=-1;
            }

            ball_position.x=ball_next.x;
            ball_position.y=ball_next.y;

            // Move paddles
            // ADC
            /*if(ADCON0bits.GODONE==0){
                int signed data = ADRES;
                paddle_speed[0]=-2*(data/100-5);
                ADCON0bits.GODONE=1;
            }*/
            // IA
            if(ball_position.x<IA_TRIGGER){
                if(ball_speed.x<0){
                    // Follow ball only if getting closer
                    int hit_y=ball_position.y-(ball_speed.y*(ball_position.x))/ball_speed.x;
                    // Be sure that the ball isn't going to bounce
                    if(hit_y>0 && hit_y<127){
                        if(paddle_position[0]<hit_y && paddle_position[0]+PADDLE_SIZE_Y>hit_y){
                            paddle_speed[0]=0;
                        }else if(paddle_position[0]+PADDLE_SIZE_Y<hit_y){
                            paddle_speed[0]=10;
                        }else if(paddle_position[0]>hit_y){
                            paddle_speed[0]=-10;
                        }
                    }
                }
            }
            if(ball_position.x>127-IA_TRIGGER){
                if(ball_speed.x>0){
                    // Follow ball only if getting closer
                    int hit_y=ball_position.y+(ball_speed.y*(127-ball_position.x))/ball_speed.x;
                    if(hit_y>0 && hit_y<127){
                        if(paddle_position[1]<hit_y && paddle_position[1]+PADDLE_SIZE_Y>hit_y){
                            paddle_speed[1]=0;
                        }else if(paddle_position[1]+PADDLE_SIZE_Y<hit_y){
                            paddle_speed[1]=10;
                        }else if(paddle_position[1]>hit_y){
                            paddle_speed[1]=-10;
                        }
                    }
                }
            }

            int paddle_next[2];
            for(int k=0; k<2; k++){
                paddle_wait[k]-=abs(paddle_speed[k]);
                if(paddle_wait[k]<=0){
                    paddle_wait[k]=EVENT_WAIT_RESET;
                    if(paddle_speed[k]>0){
                        paddle_next[k]=paddle_position[k]+1;
                    }else if(paddle_speed[k]<0){
                        paddle_next[k]=paddle_position[k]-1;
                    }else{
                        paddle_next[k]=paddle_position[k];
                    }
                }
            }
            for(int k=0; k<2; k++){
                if(paddle_next[k]<1){
                    paddle_next[k]=1;
                    paddle_speed[k]==0;
                }
                if(paddle_next[k]>=63-PADDLE_SIZE_Y){
                    paddle_next[k]=63-PADDLE_SIZE_Y;
                    paddle_speed[k]==0;
                }
                paddle_position[k]=paddle_next[k];
            }
        }
    }
}
