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
#include <Arduino.h>
#include <LowPower.h>
#include <SoftwareSerial.h>
#include <DFRobot_SIM7000.h>
#include <Adafruit_ADXL345_U.h>

/* ============================================================================================== */
/*                                        DEFINES/TYPEDEFS                                        */
#define LOGS_ENABLED                1
#define TARGET_NUMBER               "+48664059986"

#define HELLO_MESSAGE               "JeepProtect just woke up!"
#define TRIG_BY_ACCEL_MSG           "Jeep is moving!"
#define TRIG_BY_IGNITION_MSG        "Somebody tried to start the Jeep!"
#define UNLOCK_MSG                  "Drive"
#define POSITION_MSG                "Position"

#define PIN_TX                      7
#define PIN_RX                      8
#define ACCEL_INT_PIN               2
#define IGNITION_INT_PIN            3
#define BUTTON_0_PIN                9
#define BUTTON_1_PIN                10
#define ALARM_RELAY_PIN             4
#define PUMP_RELAY_PIN              5
#define IGNITION_RELAY_PIN          6

#define ACCEL_DATARATE              ADXL345_DATARATE_200_HZ //More is more precisious but takes more energy
#define ACTIVITY_THRESHOLD          100
#define MAX_MSG_LEN                 32
#define ALARM_INTERVAL_MS           1000
#define IGNITION_DEBOUNCE_MS        5000 //After this time we start accel again or fire an alarm

typedef union
{
    struct
    {
        uint8_t motion_detected     :1;
        uint8_t ignition_detected   :1;
        uint8_t got_sms_msg         :1;
        uint8_t got_sms_pos         :1;
        uint8_t got_sms_unlock      :1;
        uint8_t sms_sent            :1;
        uint8_t lock                :1;
        uint8_t unused              :1;
    };
    uint8_t val;
    
}app_events_t;

/* ============================================================================================== */
/*                                   PRIVATE FUNCTION DEFINITIONS                                 */
static void     accel_power(bool on);
static void     accel_int_cb(void);
static void     accel_handler(void);
static int      accel_init(void);
static void     alarm_cb(void);
static void     unlock_the_car(void);
static void     lock_the_car(void);
static void     ignition_sense_handler(void);
static int      ignition_sense_init(void);
static void     ignition_sense_cb(void);
static int      send_sms(char *msg);
static void     log(char *msg);
static void     log(int msg);
/* ============================================================================================== */
/*                                        PRIVATE VARIABLES                                       */                     
static SoftwareSerial               mySerial(PIN_RX, PIN_TX);
static DFRobot_SIM7000              sim7000(&mySerial);
static Adafruit_ADXL345_Unified     accel(2137);
static app_events_t                 app_events = {.val = 0};
static bool                         acc_int_event = false;
static bool                         ignition_event = false;
static bool                         button_0_pressed = false;
static bool                         button_1_pressed = false;
static bool                         movement_detected = false;
static char                         sms_rcv_msg[MAX_MSG_LEN] = {0};

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

    ignition_sense_init();
    pinMode(BUTTON_0_PIN, INPUT_PULLUP);
    pinMode(BUTTON_1_PIN, INPUT_PULLUP);

    log("Everything is configured... Starting app...\n");
}

void loop()
{
    bool tmp_button_0_pressed = 0;
    bool tmp_button_1_pressed = 0;
    if (acc_int_event)
        accel_handler();

    if (ignition_event)
        ignition_sense_handler();

    // Any event detected and should be handled
    if (app_events.val > 0)
    {
        if (app_events.motion_detected)
        {
            alarm_cb();
            if (!app_events.sms_sent)
                send_sms(TRIG_BY_ACCEL_MSG);            
        }
        if (app_events.ignition_detected)
        {
            alarm_cb();
            if (!app_events.sms_sent)
                send_sms(TRIG_BY_IGNITION_MSG);            
        }
        if (!digitalRead(BUTTON_0_PIN))
        {
            unlock_the_car();
        }
        if (!digitalRead(BUTTON_1_PIN))
            ;//TODO: Unused. Instructions are not clear.
        if (app_events.got_sms_msg)
            ;//TODO: Read SMS
        if (app_events.got_sms_pos)
            ;//TODO: Get position
        if (app_events.got_sms_unlock)
            ;//TODO: Disable alarms
        if (app_events.lock)
            lock_the_car();
        
    }
    else
    {
        log("Going to sleep...\n");
        delay(2000); //Needed for logs :)
        LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF); 
        log("Woke up!\n");
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

static void unlock_the_car(void)
{
    accel_power(false);
    digitalWrite(PUMP_RELAY_PIN, HIGH);
    digitalWrite(IGNITION_RELAY_PIN, HIGH);
    app_events.val = 0;
}

static void lock_the_car(void)
{
    accel_power(true);
    digitalWrite(PUMP_RELAY_PIN, LOW);
    digitalWrite(IGNITION_RELAY_PIN, LOW);
    app_events.val = 0;
}

static void alarm_cb(void)
{
    static unsigned long previous_millis = 0;
    unsigned long current_milis = millis();
    if (current_milis - previous_millis >= ALARM_INTERVAL_MS)
    {
        int state = digitalRead(ALARM_RELAY_PIN);
        digitalWrite(ALARM_RELAY_PIN, !state);
    }
}

static void accel_power(bool on)
{
    if (on)
        accel.writeRegister(ADXL345_REG_POWER_CTL, 0x18);
    else
        accel.writeRegister(ADXL345_REG_POWER_CTL, 0x10);
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

    pinMode(ACCEL_INT_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(ACCEL_INT_PIN), accel_int_cb, RISING);

    accel.readRegister(ADXL345_REG_INT_SOURCE); //Dummy read to clear interrupts
    accel_power(true);

    return 0;
}

static int send_sms(char *msg)
{
    return 0;
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
    app_events.sms_sent = 1;
    return 0;
}

static void accel_handler(void)
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

static void accel_int_cb(void)
{
    acc_int_event = true;
}

static void ignition_sense_handler(void)
{
    static unsigned long previous_millis = 0;
    unsigned long current_milis = millis();
    if (current_milis - previous_millis >= IGNITION_DEBOUNCE_MS)
    {
        int state = !digitalRead(IGNITION_INT_PIN);
        if (state)
        {
            app_events.ignition_detected;
        }
        else
        {
            app_events.lock = true;
        }

        ignition_event = false;
    }
}

static int ignition_sense_init(void)
{
    pinMode(IGNITION_INT_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(IGNITION_INT_PIN), ignition_sense_cb, CHANGE);
    return 0;
}

static void ignition_sense_cb(void)
{
    ignition_event = true;
}
/* ============================================================================================== */