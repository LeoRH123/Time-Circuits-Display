#include "tc_time.h"

bool autoTrack = false;
int8_t minPrev;  // track previous minute

bool x;               // for tracking second change
bool y;               // for tracking second change

struct tm _timeinfo; //for NTP

RTC_DS3231 rtc; //for RTC IC

// The displays
clockDisplay destinationTime(DEST_TIME_ADDR, DEST_TIME_EEPROM);  // i2c address, eeprom save location, 8 bytes are needed
clockDisplay presentTime(PRES_TIME_ADDR, PRES_TIME_EEPROM);
clockDisplay departedTime(DEPT_TIME_ADDR, DEPT_TIME_EEPROM);

// Automatic times
dateStruct destinationTimes[8] = {
    //YEAR, MONTH, DAY, HOUR, MIN
    {1985, 10, 26, 1, 21},
    {1985, 10, 26, 1, 24},
    {1955, 11, 5, 6, 0},
    {1985, 10, 27, 11, 0},
    {2015, 10, 21, 16, 29},
    {1955, 11, 12, 6, 0},
    {1885, 1, 1, 0, 0},
    {1885, 9, 2, 12, 0}};

dateStruct departedTimes[8] = {
    {1985, 10, 26, 1, 20},
    {1955, 11, 12, 22, 4},
    {1985, 10, 26, 1, 34},
    {1885, 9, 7, 9, 10},
    {1985, 10, 26, 11, 35},
    {1985, 10, 27, 2, 42},
    {1955, 11, 12, 21, 44},
    {1955, 11, 13, 12, 0}};

int8_t autoTime = 0;  // selects the above array time

const uint8_t monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

const char* ssid = "linksys";
const char* password = "jhn003021";

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -18000;
const int daylightOffset_sec = 3600;

void time_setup() {
    pinMode(SECONDS_IN, INPUT_PULLDOWN);  // for monitoring seconds
    pinMode(STATUS_LED, OUTPUT);  // Status LED

    // initialize EEPROM with predefined size
    EEPROM.begin(EEPROM_SIZE);

    // RTC setup
    if (!rtc.begin()) {
        //something went wrong with RTC IC
        Serial.println("Couldn't find RTC");
        while (1);
    }

    if (rtc.lostPower() && WiFi.status() != WL_CONNECTED) {
        // Lost power and battery didn't keep time, so set current time to compile time
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    RTCClockOutEnable();  // Turn on the 1Hz second output

    bool validLoad = true;

    // Start the displays by calling begin()
    presentTime.begin();
    destinationTime.begin();
    departedTime.begin();

    presentTime.setRTC(true);  // configure as a display that will hold real time

    if (getNTPTime()) {
        //set RTC with NTP time
    }
     

    if (!destinationTime.load()) {
        validLoad = false;
        // Set valid time and set if invalid load
        // 10/26/1985 1:21
        destinationTime.setMonth(10);
        destinationTime.setDay(26);
        destinationTime.setYear(1985);
        destinationTime.setHour(1);
        destinationTime.setMinute(21);
        destinationTime.setBrightness(15);
        destinationTime.save();
    }

    if (!departedTime.load()) {
        validLoad = false;
        // Set valid time and set if invalid load
        // 10/26/1985 1:20
        departedTime.setMonth(10);
        departedTime.setDay(26);
        departedTime.setYear(1985);
        departedTime.setHour(1);
        departedTime.setMinute(20);
        departedTime.setBrightness(15);
        departedTime.save();
    }

    if (!presentTime.load()) {  // Time isn't saved here, but other settings are
        validLoad = false;
        presentTime.setBrightness(15);
        presentTime.save();
    }

    if (!loadAutoInterval()) {  // load saved settings
        validLoad = false;
        Serial.println("BAD AUTO INT");
        EEPROM.write(AUTOINTERVAL_ADDR, 1);  // default to first option
    }

    if (autoTimeIntervals[autoInterval]) {                    // non zero interval, use auto times
        destinationTime.setFromStruct(&destinationTimes[0]);  // load the first one
        departedTime.setFromStruct(&departedTimes[0]);
    }

    if (!validLoad) {
        // Show message
        destinationTime.showOnlySettingVal("RE", -1, true);
        presentTime.showOnlySettingVal("SET", -1, true);
        delay(2000);
        allOff();
        delay(1000);
    }

    Serial.println("Update Present Time - Setup");
    presentTime.setDateTime(rtc.now());                 // Load the time for initial animation show
    presentTime.setBrightness(15);  // added
    animate();
}

void time_loop() {
// time display update
    DateTime dt = rtc.now();
    presentTime.setDateTime(dt);  // Set the current time in the display

    y = digitalRead(SECONDS_IN);
    if (y != x) {      // different on half second
        if (y == 0) {  // flash colon on half seconds, lit on start of second
            /////////////////////////////
            //
            // auto display some times
            Serial.print(dt.minute());
            Serial.print(":");
            Serial.println(dt.second());

            // Do this on previous minute:59
            if (dt.minute() == 59) {
                minPrev = 0;
            } else {
                minPrev = dt.minute() + 1;
            }

            // only do this on second 59, check if it's time to do so
            if (dt.second() == 59 && autoTimeIntervals[autoInterval] &&
                (minPrev % autoTimeIntervals[autoInterval] == 0)) {
                Serial.println("DO IT");
                if (!autoTrack) {
                    autoTrack = true;  // Already did this, don't repeat
                    // do auto times
                    autoTime++;
                    if (autoTime > 4) {  // currently have 5 times
                        autoTime = 0;
                    }

                    // Show a preset dest and departed time
                    destinationTime.setFromStruct(&destinationTimes[autoTime]);
                    departedTime.setFromStruct(&departedTimes[autoTime]);

                    destinationTime.setColon(true);
                    presentTime.setColon(true);
                    departedTime.setColon(true);

                    allOff();

                    // Blank on second 59, display when new minute begins
                    while (digitalRead(SECONDS_IN) ==
                           0) {  // wait for the complete of this half second
                                 // Wait for this half second to end
                    }
                    while (digitalRead(SECONDS_IN) == 1) {  // second on next low
                                                          // Wait for the other half to end
                    }

                    Serial.println("Update Present Time 2");
                    dt = rtc.now();               // New time by now
                    presentTime.setDateTime(dt);  // will be at next minute
                    animate();                    // show all with month showing last

                    // end auto times
                }
            } else {
                autoTrack = false;
            }

            //////////////////////////////
            destinationTime.setColon(true);
            presentTime.setColon(true);
            departedTime.setColon(true);
        } else {  // colon
            destinationTime.setColon(false);
            presentTime.setColon(false);
            departedTime.setColon(false);
        }  // colon

        digitalWrite(STATUS_LED, !y);  // built-in LED shows system is alive, invert
                                      // to light on start of new second
        x = y;                        // remember it
    }

    presentTime.show();  // update display with object's time
    destinationTime.show();
    // destinationTime.showOnlySettingVal("SEC", dt.second(), true); // display
    // end, no numbers, clear rest of screen
    departedTime.show();

    delay(10);
}

bool getNTPTime() {
    // connect to WiFi if available
    if (connectToWifi()) {
        // if connected to wifi, get NTP time and set RTC
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        if (!getLocalTime(&_timeinfo)) {
            Serial.println("Couldn't get NTP time");
            //TODO: Timeout after x amount of retries, default to RTC
            return false;
        } else {
            Serial.println(&_timeinfo, "%A, %B %d %Y %H:%M:%S");
            byte byteYear = (_timeinfo.tm_year + 1900) % 100;   // adding to get full YYYY from NTP year format
                                                                // and keeping YY to set DS3232
            presentTime.setDS3232time(_timeinfo.tm_sec, _timeinfo.tm_min,
                                        _timeinfo.tm_hour, 0, _timeinfo.tm_mday,
                                        _timeinfo.tm_mon, byteYear);
            Serial.println("Set Time with NTP");
            return true;
        }
    } else {
        return false;
    }
}

bool connectToWifi() {
    if (ssid != 0 && password != 0) {
        Serial.printf("Connecting to %s ", ssid);
        WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(),
                    IPAddress(8, 8, 8, 8));
        WiFi.begin(ssid, password);
        WiFi.enableSTA(true);
        delay(100);
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
            //TODO: Timeout after x amount of retries, default to RTC
        }
        Serial.println(" CONNECTED");
        return true;
    } else {
        Serial.println("No WiFi credentials set");
        return false;
    }
}

void doGetAutoTimes() {
    // Set the auto times setting
    destinationTime.showOnlySettingVal("INT", autoTimeIntervals[autoInterval], true);

    presentTime.on();
    if (autoTimeIntervals[autoInterval] == 0) {
        presentTime.showOnlySettingVal("CUS", -1, true);
        departedTime.showOnlySettingVal("TOM", -1, true);
        departedTime.on();
    } else {
        departedTime.off();
        presentTime.showOnlySettingVal("MIN", -1, true);
    }

    while (!checkTimeOut() && !digitalRead(ENTER_BUTTON)) {
        autoTimesButtonUpDown();
        delay(100);
    }

    if (!checkTimeOut()) {  // only if there wasn't a timeout
        presentTime.off();
        departedTime.off();
        destinationTime.showOnlySave();
        saveAutoInterval();
        delay(1000);
        waitForButtonSetRelease();
    }
}

bool checkTimeOut() {
    // Call frequently while waiting for button press, increments timeout each
    // second, returns true when maxtime reached.
    y = digitalRead(SECONDS_IN);
    if (x != y) {
        x = y;
        digitalWrite(STATUS_LED, !y);  // update status LED
        if (y == 0) {
            timeout++;
        }
    }

    if (timeout >= maxTime) {
        return true;  // timed out
    }
    return false;
}

void RTCClockOutEnable() {
    // enable the 1Hz RTC output
    uint8_t readValue = 0;
    Wire.beginTransmission(DS3232_I2CADDR);
    Wire.write((byte)0x0E);  // select control register
    Wire.endTransmission();

    Wire.requestFrom(DS3232_I2CADDR, 1);
    readValue = Wire.read();
    readValue = readValue & B11100011;  // enable squarewave and set to 1Hz,
    // Bit 2 INTCN - 0 enables OSC
    // Bit 3 and 4 - 0 0 sets 1Hz

    Wire.beginTransmission(DS3232_I2CADDR);
    Wire.write((byte)0x0E);  // select control register
    Wire.write(readValue);
    Wire.endTransmission();
}

bool isLeapYear(int year) {
    // Determine if provided year is a leap year.
    if (year % 4 == 0) {
        if (year % 100 == 0) {
            if (year % 400 == 0) {
                return true;
            } else {
                return false;
            }
        } else {
            return true;
        }
    } else {
        return false;
    }
}  // isLeapYear

int daysInMonth(int month, int year) {
    // Find number of days in a month
    if (month == 2 && isLeapYear(year)) {
        return 29;
    }
    return monthDays[month - 1];
}  // daysInMonth
