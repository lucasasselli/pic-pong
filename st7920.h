#include <p18f452.h>

#define _XTAL_FREQ 80000000

#define DATA LATB
#define RST LATDbits.LATD4
#define RS LATDbits.LATD5
#define RW LATDbits.LATD6
#define EN LATDbits.LATD7

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))
#define SET_BIT(var,pos) ((var) | (1 << pos))
#define UNSET_BIT(var,pos) ((var) & ~(1 << pos))

volatile int update_enabled;
char display_data[16][64];
char update_flag[64];

static const char number_0[4]={0xF9, 0x99, 0x99, 0xF0};
static const char number_1[4]={0x11, 0x11, 0x11, 0x10};
static const char number_2[4]={0xF1, 0x1F, 0x88, 0xF0};
static const char number_3[4]={0xF1, 0x1F, 0x11, 0xF0};
static const char number_4[4]={0x99, 0x9F, 0x11, 0x10};
static const char number_5[4]={0xF8, 0x8F, 0x11, 0xF0};
static const char number_6[4]={0xF8, 0x8F, 0x99, 0xF0};
static const char number_7[4]={0xF1, 0x11, 0x11, 0x10};
static const char number_8[4]={0xF9, 0x9F, 0x99, 0xF0};
static const char number_9[4]={0xF9, 0x9F, 0x11, 0xF0};

void sendCommand(unsigned char command){
    RW=0;
    RS = 0;
    __delay_us(15);
    EN = 1;
    DATA = command;
    __delay_us(15);
    EN = 0;
}

void sendData(unsigned char data){
    RS = 1;
    DATA= data;
    __delay_us(15);
    EN = 1;
    __delay_us(15);
    EN = 0;
 }

void startScreen(){
    int i;
    RST=0;
    for(i=0; i<10; i++)
        __delay_ms(1);
    RS=0;
    RW=0;
    __delay_us(400);
    RST = 1;
    for(i=0; i<30; i++)
        __delay_ms(1);              // Short delay after resetting.
    sendCommand(0b00110000) ;    // 8-bit mode.
    __delay_us(100);
    sendCommand(0b00110000) ;     // 8-bit mode again.
    __delay_us(110);
    sendCommand(0b00001100);     // display on
    __delay_us(100);
    sendCommand(0b00000001);  // Clears screen.
    __delay_ms(2);
    sendCommand(0b00000110);  // Cursor moves right, no display shift.
    __delay_us(80);
    sendCommand(0b00110100); // Extended instuction set, 8bit
    __delay_us(100);
    sendCommand(0b00110110);   // Repeat instrution with bit1 set
    __delay_us(100);
}

void resetUpdateFlags(){
    for(int i=0; i<64; i++){
        update_flag[i]=0x00;
    }
}

void refreshScreen(){
   unsigned char x, y;
   for(y = 0; y < 64; y++)
   {
       if(y < 32)
       {
         sendCommand(0x80 | y);
         sendCommand(0x80);
       }
       else
       {
         sendCommand(0x80 | (y-32));
         sendCommand(0x88);
       }
       for(x = 0; x < 16; x++)
       {
         sendData(display_data[x][y]);
       }
   }
   resetUpdateFlags();
}

void smartRefresh(){
    unsigned int i, j, k, l;
        for(j=0; j<64; j++){
            if(update_flag[j]!=0){
                // Slice changed
                for(k=0; k<8; k++){
                    if(CHECK_BIT(update_flag[j],k)){
                        int screen_y=j>=32?j-32:j;

                        if(CHECK_BIT(update_flag[j],k-1)!=1){
                            sendCommand(0x80 | (screen_y));
                            sendCommand(0x80 | (j>=32?8:0)+k);
                        }

                        for(l=0; l<2; l++){
                            sendData(display_data[k*2+l][j]);
                        }
                    }
            }
        }
    }

    resetUpdateFlags();
}

void clearScreen(){
    unsigned int x, y;
    for(y=0; y<64; y++)
        for(x=0; x<17; x++)
            display_data[x][y]=0x00;
}

void setPixel(int x, int y, int value){
    int x_char = x/8;
    int index = 7-x%8;
    char buf = display_data[x_char][y];

    if(value==0){
        buf=UNSET_BIT(buf,index);
    }else{
        buf=SET_BIT(buf,index);
    }

    display_data[x_char][y]=buf;    
    update_flag[y]=SET_BIT(update_flag[y], x/16);
}

void drawNumber(int pos_x, int pos_y, int a){
    int x, y;
    for(y=pos_y; y<pos_y+8; y++){
        for(x=pos_x; x<pos_x+4; x++)
        {
            int block = (y-pos_y)/2;
            int pos = 7-(((y-pos_y)%2)*4+(x-pos_x));
            
            char buf;

            switch(a){
                case 0:
                buf=number_0[block];
                break;

                case 1:
                buf=number_1[block];
                break;

                case 2:
                buf=number_2[block];
                break;

                case 3:
                buf=number_3[block];
                break;

                case 4:
                buf=number_4[block];
                break;

                case 5:
                buf=number_5[block];
                break;

                case 6:
                buf=number_6[block];
                break;

                case 7:
                buf=number_7[block];
                break;

                case 8:
                buf=number_8[block];
                break;

                case 9:
                buf=number_9[block];
                break;

                default:
                buf=number_0[block];
                break;
            }

            int value = CHECK_BIT(buf,pos);
            
            setPixel(x,y, value);
        }
    }
}

void enableUpdate(){
    update_enabled = 1;
}

void disableUpdate(){
    update_enabled = 0;
}