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
#include <Adafruit_ADXL345_U.h>

/* ============================================================================================== */
/*                                        DEFINES/TYPEDEFS                                        */
#define LOGS_ENABLED                1
#define TARGET_NUMBER               "+48664059986"
#define HELLO_MESSAGE               "JeepProtect just woke up!"
#define PIN_TX                      7
#define PIN_RX                      8
#define ACCEL_INT                   2
#define ACCEL_DATARATE              ADXL345_DATARATE_200_HZ //More is more precisious but takes more energy
#define ACTIVITY_THRESHOLD          100
/* ============================================================================================== */
/*                                   PRIVATE FUNCTION DEFINITIONS                                 */
static void     accel_int_cb(void);
static int      accel_init(void);
static int      send_sms(char *msg);
static void     log(char *msg);
static void     log(int msg);
/* ============================================================================================== */
/*                                        PRIVATE VARIABLES                                       */
static SoftwareSerial               mySerial(PIN_RX, PIN_TX);
static DFRobot_SIM7000              sim7000(&mySerial);
static Adafruit_ADXL345_Unified     accel(2137);
static bool                         acc_int_event = false;

/* ============================================================================================== */
/*                                        PUBLIC FUNCTIONS                                        */
void setup() {
    int err;
    Serial.begin(115200);
    mySerial.begin(19200);

    log("Turn ON SIM7000...\n");
    err = !sim7000.turnON();
    if(err)
    {
        log("Can't turn on SIM module\n");
        while(1);
    }

    log("Set baud rate for SIM...");
    err = !sim7000.setBaudRate(19200);
    if(err)
    {
        log("Failed to set baud rate\n");
        while(1);
    }
    
    log("Sending init message\n");
    err = send_sms(HELLO_MESSAGE);
    if(err)
    {
        log("Can't send hello message!!!\n");
        log(err);
        while(1);
    }
    log("Started accel init\n");

    err = accel_init();
    if (err)
    {
        log("Can't find any accel...\n");
        while(1);
    }

    log("Everything is configured... Starting app...\n");
}

void loop()
{
    if (acc_int_event)
    {
        acc_int_event = false;
        delay(1000);
        uint8_t reg = accel.readRegister(ADXL345_REG_INT_SOURCE);
        if (reg & (1 << 4)) //Activity 
        {
            log("Motion detected\n");
            send_sms("Motion Detected!");
        }

    }
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

static int accel_init(void)
{
    int err;
    err = !accel.begin();
    if (err)
        return err;

    accel.setRange(ADXL345_RANGE_4_G); //I don't think you need more
    accel.setDataRate(ACCEL_DATARATE);
    accel.writeRegister(ADXL345_REG_THRESH_ACT, ACTIVITY_THRESHOLD);
    accel.writeRegister(ADXL345_REG_ACT_INACT_CTL, 0x70); //Every axis
    accel.writeRegister(ADXL345_REG_INT_ENABLE, 0x10);
    accel.writeRegister(ADXL345_REG_INT_MAP, 0xff & !(0x10));

    pinMode(ACCEL_INT, INPUT);
    attachInterrupt(digitalPinToInterrupt(ACCEL_INT), accel_int_cb, RISING);

    accel.readRegister(ADXL345_REG_INT_SOURCE); //Dummy read to clear interrupts

    return 0;
}

static int send_sms(char *msg)
{
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
    sim7000.sendCmd("");
    return 0;
}

static void accel_int_cb(void)
{
    acc_int_event = true;
}
/* ============================================================================================== */