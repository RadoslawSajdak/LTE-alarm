/* ============================================================================================== */
/* 
 *       Filename:  main.c
 *
 *    Description:  jeep-lte
 *
 *        Created:  12/03/2024
 *
 *         Author:  Radosław Sajdak
 *   Organization:  nd
 * 
 *        License: #TODO!!!
 */
/* ============================================================================================== */
/*                                            INCLUDES                                            */
#include <SoftwareSerial.h>
#include <DFRobot_SIM7000.h>
/* ============================================================================================== */
/*                                        DEFINES/TYPEDEFS                                        */
#define LOGS_ENABLED                1
#define TARGET_NUMBER               "+48"
#define HELLO_MESSAGE               "JeepProtect just woke up!"
#define PIN_TX                      7
#define PIN_RX                      8
/* ============================================================================================== */
/*                                   PRIVATE FUNCTION DEFINITIONS                                 */
static int      send_sms(char *msg);
static void     log(char *msg);
static void     log(int msg);
/* ============================================================================================== */
/*                                        PRIVATE VARIABLES                                       */
static SoftwareSerial               mySerial(PIN_RX, PIN_TX);
static DFRobot_SIM7000              sim7000(&mySerial);

/* ============================================================================================== */
/*                                        PUBLIC FUNCTIONS                                        */
void setup() {
    Serial.begin(115200);
    mySerial.begin(19200);

    log("Turn ON SIM7000...\n");
    if(!sim7000.turnON())
    {
        log("Can't turn on SIM module\n");
        while(1);
    }

   log("Set baud rate for SIM...");
    if(!sim7000.setBaudRate(19200))
    {
        log("Failed to set baud rate\n");
        while(1);
    }
    
    log("Sending init message\n");
    if(send_sms(HELLO_MESSAGE))
    {
        log("Can't send hello message!!!\n");
        while(1);
    }
}

void loop()
{
    delay(20);
}

/* ============================================================================================== */
/*                                        PRIVATE FUNCTIONS                                       */
static void log(char *msg)
{
#ifdef LOGS_ENABLED
    Serial.print(msg);
#endif
}
static void log(int msg)
{
#ifdef LOGS_ENABLED
    Serial.print(msg);
#endif
}

static int send_sms(char *msg)
{
    int err;
    Serial.print("Sending >> "); Serial.print(msg); Serial.println(" << To: " TARGET_NUMBER);
    if(sim7000.checkSendCmd("AT+CMGF=1\r\n","OK"))
        delay(100);
    else
    {
        sim7000.closeCommand();
        return -1;
    }

    if(sim7000.checkSendCmd("AT+CMGS=\"" TARGET_NUMBER "\"\r", ">"))
        delay(100);
    else
    {
        sim7000.closeCommand();
        return -1;
    }

    sim7000.sendCmd(msg);

    if(sim7000.checkSendCmd("", "+CMGS"))
        return 0;
    else return -2;

}
/* ============================================================================================== */