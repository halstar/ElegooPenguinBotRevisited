#include "IRremote.h"
#include "Oscillator.h"
#include "NeoSWSerial.h"

// #define DEBUG

#ifdef DEBUG
    #define LOG(string)       Serial.print(string)
    #define LOG_LINE(string)  Serial.println(string)
#else
    #define LOG(X)       
    #define LOG_LINE(X)  
#endif

#define BTN_FORWARD  16736925
#define BTN_BACKWARD 16754775
#define BTN_LEFT     16761405
#define BTN_RIGHT    16720605
#define BTN_DANCE    16718055
#define BTN_MUSIC    16724175
#define BTN_MODE     16734885
#define BTN_UP       16716015
#define BTN_DOWN     16726215
#define BTN_IDLE     16712445
#define BTN_VOL      16743045

//               ---------------
//              |     O   O     |
//              |---------------|
// UP_LEFT  ==> |               | <== UP_RIGHT
//               ---------------
//                  ||     ||
//                  ||     ||
// LOW_LEFT ==>  -----    ------  <== LOW_RIGHT
//              |-----    ------|

#define UP_RIGHT_PIN  10 // 3
#define UP_LEFT_PIN    9 // 2
#define LOW_RIGHT_PIN 12 // 1
#define LOW_LEFT_PIN   6 // 0

#define DEFAULT_TEMPO 450

#define ST188_L_PIN         A0
#define ST188_R_PIN         A1
#define SOFTWARE_RXD        A2
#define SOFTWARE_TXD        A3
#define VOLTAGE_MEASURE_PIN A4
#define INDICATOR_LED_PIN   A5

#define RECV_PIN   3
#define ECHO_PIN   4
#define TRIG_PIN   5
#define HT6871_PIN 7

#define VREF 1.1
#define RES1 10000
#define RES2 2000

#define NB_SERVOS         4
#define INTERVAL_TIME  10.0
#define CENTER           90
#define AMPLITUDE        30
#define ULTRA_HIGH_RATE 0.3
#define HIGH_RATE       0.5
#define MID_RATE        0.7
#define LOW_RATE        1.0
#define ULTRA_LOW_RATE  1.5

NeoSWSerial mp3Serial(SOFTWARE_RXD, SOFTWARE_TXD);
Oscillator  servo[NB_SERVOS];
int         oldPosition[NB_SERVOS] = {CENTER, CENTER, CENTER, CENTER};
IRrecv      irRecv(RECV_PIN);

int            danceIndex         = 1;
int            musicIndex         = 2;
bool           ledFlag            = true;
unsigned long  ledBlinkTime       = 0;
unsigned long  voltageMeasureTime = 0;

enum MODE
{
    IDLE,
    IR_CONTROL,
    OBSTACLES_AVOIDANCE,
    FOLLOW,
    MUSIC,
    DANCE,
    VOLUME
} mainMode = IDLE, oldMainMode = IDLE;

enum DIRECTION
{
    FORWARD,
    BACKWARD,
    TURN_RIGHT,
    TURN_LEFT,
    STOP,
} direction = STOP;

class Mp3Player
{
public:
    String playStatus[5] = {"0", "1", "2", "3", "4"}; // STOP PLAYING PAUSE FF FR

    void play(unsigned char trackIndex)
    {
        stop();
        LOG("Playing #");
        LOG_LINE(trackIndex);
        setMode(4);
        CmdSelect[4] = trackIndex;
        checkCode(CmdSelect);
        mp3Serial.write(CmdSelect, 7);
        delay(10);
    };

    String getPlayStatus()
    {
        String mp3Status = "";

        mp3Serial.write(CmdGetPlayStatus, 5);
        delay(10);
        while (mp3Serial.available() != 0)
        {
            mp3Status += (char)mp3Serial.read();
        }
        return mp3Status;
    }

    void stop()
    {
        setMode(4);
        mp3Serial.write(CmdStop, 5);
        delay(10);
    };

    void setVolume()
    {
        CmdVolumeSet[3] = volume;
        checkCode(CmdVolumeSet);
        mp3Serial.write(CmdVolumeSet, 6);
        delay(10);
    };

    void volumeUp()
    {
        volume++;
        if (volume >= 25)
        {
            volume = 25;
        }
        setVolume();
        delay(10);
    };

    void volumeDown()
    {
        volume--;
        if (volume <= 0)
        {
            volume = 0;
        }
        setVolume();
        delay(10);
    };

    void setMode(unsigned char mode)
    {
        CmdPlayMode[3] = mode;
        checkCode(CmdPlayMode);
        mp3Serial.write(CmdPlayMode, 6);
        delay(10);
    };

    void checkCode(unsigned char *vs)
    {
        int val = vs[1];
        int i;
        for (i = 2; i < vs[1]; i++)
        {
            val = val ^ vs[i];
        }
        vs[i] = val;
    };

    void init()
    {
        pinMode     (HT6871_PIN, OUTPUT);
        digitalWrite(HT6871_PIN, HIGH);
        stop();
        volume = 8;
        setVolume();
    }

private:
    int  volume;
    byte CmdPlay[5]          = {0x7E, 0x03, 0x11, 0x12, 0xEF};
    byte CmdStop[5]          = {0x7E, 0x03, 0x1E, 0x1D, 0xEF};
    byte CmdNext[5]          = {0x7E, 0x03, 0x13, 0x10, 0xEF};
    byte CmdPrev[5]          = {0x7E, 0x03, 0x14, 0x17, 0xEF};
    byte CmdVolumeUp[5]      = {0x7E, 0x03, 0x15, 0x16, 0xEF};
    byte CmdVolumeDown[5]    = {0x7E, 0x03, 0x16, 0x15, 0xEF};
    byte CmdVolumeSet[6]     = {0x7E, 0x04, 0x31, 0x00, 0x00, 0xEF};
    byte CmdPlayMode[6]      = {0x7E, 0x04, 0x33, 0x00, 0x00, 0xEF};
    byte CmdSelect[7]        = {0x7E, 0x05, 0x41, 0x00, 0x00, 0x00, 0xEF};
    byte CmdGetPlayStatus[5] = {0x7E, 0x03, 0x20, 0x23, 0xEF};
} mp3Player;

void oscillate(int A[NB_SERVOS], int O[NB_SERVOS], int T, double phaseDiff[NB_SERVOS])
{
    for (int i = 0; i < NB_SERVOS; i++)
    {
        servo[i].setO(O[i]);
        servo[i].setA(A[i]);
        servo[i].setT(T);
        servo[i].setPh(phaseDiff[i]);
    }
    double ref = millis();
    for (double x = ref; x < T + ref; x = millis())
    {
        for (int i = 0; i < NB_SERVOS; i++)
        {
            servo[i].refresh();
        }
    }
}

void defaultPosition()
{
    int move[] = {90, 90, 90, 90};
    moveNServos(DEFAULT_TEMPO, move);
    delay(DEFAULT_TEMPO);
}

void moveNServos(int time, int newPosition[])
{
    unsigned long finalTime;
    unsigned long intervalTime;
    int           oneTime;
    int           iteration;
    float         increment[NB_SERVOS];

    for (int i = 0; i < NB_SERVOS; i++)
    {
        increment[i] = ((newPosition[i]) - oldPosition[i]) / (time / INTERVAL_TIME);
    }
    finalTime = millis() + time;
    iteration = 1;

    while (millis() < finalTime)
    {
        intervalTime = millis() + INTERVAL_TIME;
        oneTime = 0;
        while (millis() < intervalTime)
        {
            if (oneTime < 1)
            {
                for (int i = 0; i < NB_SERVOS; i++)
                {
                    servo[i].setPosition(oldPosition[i] + (iteration * increment[i]));
                }
                iteration++;
                oneTime++;
            }
        }
    }

    for (int i = 0; i < NB_SERVOS; i++)
    {
        oldPosition[i] = newPosition[i];
    }
}

void walk(int steps, int T, bool isForward)
{
    int A[NB_SERVOS] = {30, 30, 30, 30};
    int O[NB_SERVOS] = {0, 0, 0, 0};
    
    double phaseDiff[NB_SERVOS];

    servoAttach();

    phaseDiff[0] = DEG2RAD(0);
    phaseDiff[1] = DEG2RAD(0);

    if (isForward == true)
    {
        phaseDiff[2] = DEG2RAD(90);
        phaseDiff[3] = DEG2RAD(90);
    }
    else
    {
        phaseDiff[2] = DEG2RAD(-90);
        phaseDiff[3] = DEG2RAD(-90);
    }

    for (int i = 0; i < steps; i++)
    {
        oscillate(A, O, T, phaseDiff);
    }

    servoDetach();
}

void turn(int steps, int T, bool isRight)
{
    int A[NB_SERVOS] = {30, 30, 0, 0};
    int O[NB_SERVOS] = {0, 0, 0, 0};

    double phaseDiff[NB_SERVOS] = {DEG2RAD(0), DEG2RAD(0), DEG2RAD(90), DEG2RAD(90)};

    servoAttach();

    if (isRight == true)
    {
        A[2] = 30;
        A[3] = 10;
    }
    else
    {
        A[2] = 10;
        A[3] = 30;
    }

    for (int i = 0; i < steps; i++)
    {
        oscillate(A, O, T, phaseDiff);
    }

    servoDetach();
}

void stop()
{
    servoAttach();
    defaultPosition();
    servoDetach();
}

void moonWalk(bool isRight, int steps, int T)
{
    int A[NB_SERVOS] = {25, 25, 0, 0};
    int O[NB_SERVOS] = { -15, 15, 0, 0};
 
    double phaseDiff[NB_SERVOS];
    
    phaseDiff[0] = DEG2RAD(0);
    phaseDiff[2] = DEG2RAD(90);
    phaseDiff[3] = DEG2RAD(90);

    if (isRight == true)
    {
        phaseDiff[1] = DEG2RAD(180 + 120);
    }
    else
    {
      phaseDiff[1] = DEG2RAD(180 - 120);
    }

    for (int i = 0; i < steps; i++)
    {
        oscillate(A, O, T, phaseDiff);
    }
}

void swing(int steps, int T)
{
    int A[NB_SERVOS] = {25, 25, 0, 0};
    int O[NB_SERVOS] = { -15, 15, 0, 0};

    double phaseDiff[NB_SERVOS] = {DEG2RAD(0), DEG2RAD(0), DEG2RAD(90), DEG2RAD(90)};

    for (int i = 0; i < steps; i++)
    {
        oscillate(A, O, T, phaseDiff);
    }
}

void upDown(int steps, int T)
{
    int A[NB_SERVOS] = {25, 25, 0, 0};
    int O[NB_SERVOS] = { -15, 15, 0, 0};

    double phaseDiff[NB_SERVOS] = {DEG2RAD(180), DEG2RAD(0), DEG2RAD(270), DEG2RAD(270)};

    for (int i = 0; i < steps; i++)
    {
        oscillate(A, O, T, phaseDiff);
    }

    defaultPosition();
}

void flapping(int steps, int T) {
    int A[NB_SERVOS] = {15, 15, 8, 8};
    int O[NB_SERVOS] = { -A[0], A[1], 0, 0};

    double phaseDiff[NB_SERVOS] = {DEG2RAD(0), DEG2RAD(180), DEG2RAD(-90), DEG2RAD(90)};

    for (int i = 0; i < steps; i++)
    {
        oscillate(A, O, T, phaseDiff);
    }
}

void run(int steps, int T)
{
    int A[NB_SERVOS] = {10, 10, 10, 10};
    int O[NB_SERVOS] = {0, 0, 0, 0};

    double phaseDiff[NB_SERVOS] = {DEG2RAD(0), DEG2RAD(0), DEG2RAD(90), DEG2RAD(90)};

    for (int i = 0; i < steps; i++)
    {
        oscillate(A, O, T, phaseDiff);
    }
}

void runFast(int steps, int T)
{
    int A[NB_SERVOS] = {25, 25, 30, 30};
    int O[NB_SERVOS] = { -15, 15, 0, 0};

    double phaseDiff[NB_SERVOS] = {DEG2RAD(0), DEG2RAD(180 + 120), DEG2RAD(90), DEG2RAD(90)};

    for (int i = 0; i < steps; i++)
    {
        oscillate(A, O, T, phaseDiff);
    }

    defaultPosition();
}

void backward(int steps, int T)
{
    int A[NB_SERVOS] = {15, 15, 30, 30};
    int O[NB_SERVOS] = {0, 0, 0, 0};

    double phaseDiff[NB_SERVOS] = {DEG2RAD(0), DEG2RAD(0), DEG2RAD(-90), DEG2RAD(-90)};

    for (int i = 0; i < steps; i++)
    {
        oscillate(A, O, T, phaseDiff);
    }
}

void backwardSlow(int steps, int T)
{
    int A[NB_SERVOS] = {15, 15, 30, 30};
    int O[NB_SERVOS] = {0, 0, 0, 0};

    double phaseDiff[NB_SERVOS] = {DEG2RAD(0), DEG2RAD(0), DEG2RAD(-90), DEG2RAD(-90)};

    for (int i = 0; i < steps; i++)
    {
        oscillate(A, O, T, phaseDiff);
    }
}

void goingUp(int tempo)
{
    int move[] = {50, 130, 90, 90};
    moveNServos(tempo * HIGH_RATE, move);

    delay(tempo / 2);
    defaultPosition();
}

void drunk(int tempo)
{
    int move1[] = {70, 70, 90, 90};
    int move2[] = {110, 110, 90, 90};
    int move3[] = {70, 70, 90, 90};
    int move4[] = {110, 110, 90, 90};

    moveNServos(tempo * MID_RATE, move1);
    moveNServos(tempo * MID_RATE, move2);
    moveNServos(tempo * MID_RATE, move3);
    moveNServos(tempo * MID_RATE, move4);

    defaultPosition();
}

void noGravity(int tempo)
{
    int move1[] = {120, 140, 90, 90};
    int move2[] = {120, 30, 90, 90};
    int move3[] = {120, 120, 90, 90};
    int move4[] = {120, 30, 120, 120};
    int move5[] = {120, 30, 60, 60};

    moveNServos(tempo * MID_RATE, move1);
    delay(tempo);

    moveNServos(tempo * MID_RATE, move2);
    moveNServos(tempo * MID_RATE, move3);
    moveNServos(tempo * MID_RATE, move2);
    delay(tempo);

    moveNServos(tempo * LOW_RATE, move4);
    delay(tempo);

    moveNServos(tempo * LOW_RATE, move5);
    delay(tempo);

    moveNServos(tempo * LOW_RATE, move4);
    delay(tempo);
    defaultPosition();
}

void kickLeft(int tempo)
{
    int move1[] = {120, 140, 90, 90};
    int move2[] = {120, 90, 90, 90};
    int move3[] = {120, 120, 90, 90};
    int move4[] = {120, 90, 120, 120};
    int move5[] = {120, 120, 60, 60};

    moveNServos(tempo * MID_RATE, move1);
    delay(tempo);

    moveNServos(tempo * MID_RATE, move2);
    delay(tempo / 4);

    moveNServos(tempo * MID_RATE, move3);
    delay(tempo / 4);

    moveNServos(tempo * LOW_RATE, move4);
    delay(tempo / 4);

    moveNServos(tempo * LOW_RATE, move5);
    delay(tempo / 4);
    defaultPosition();
}

void kickRight(int tempo)
{
    int move1[] = {40, 60, 90, 90};
    int move2[] = {90, 60, 90, 90};
    int move3[] = {60, 60, 90, 90};
    int move4[] = {90, 60, 120, 120};
    int move5[] = {60, 60, 60, 60};

    moveNServos(tempo * MID_RATE, move1);
    delay(tempo);

    moveNServos(tempo * MID_RATE, move2);
    delay(tempo / 4);

    moveNServos(tempo * MID_RATE, move3);
    delay(tempo / 4);

    moveNServos(tempo * LOW_RATE, move4);
    delay(tempo / 4);

    moveNServos(tempo * LOW_RATE, move5);
    delay(tempo / 4);
    defaultPosition();
}

void legRaise(int tempo, bool isRight)
{
    if (isRight == true)
    {
        int move[] = {70, 70, 60, 60};
        moveNServos(tempo * MID_RATE, move);
        delay(tempo);
    }
    else
    {
        int move[] = {110, 110, 120, 120};
        moveNServos(tempo * MID_RATE, move);
        delay(tempo);
    }

    defaultPosition();
}

void legRaise1(int tempo, bool isRight)
{
    if (isRight == true)
    {
        int move1[] = {50, 60, 90, 90};
        int move2[] = {60, 60, 120, 90};
        int move3[] = {60, 60, 60, 90};

        moveNServos(tempo * MID_RATE, move1);
        delay(tempo);

        moveNServos(tempo * LOW_RATE, move2);
        delay(tempo / 4);

        moveNServos(tempo * LOW_RATE, move3);
        delay(tempo / 4);

        moveNServos(tempo * LOW_RATE, move2);
        delay(tempo / 4);

        moveNServos(tempo * LOW_RATE, move3);
        delay(tempo / 4);
    }
    else
    {
        int move1[] = {120, 130, 90, 90};
        int move2[] = {120, 120, 90, 60};
        int move3[] = {120, 120, 90, 120};

        moveNServos(tempo, move1);
        delay(tempo);

        moveNServos(tempo * MID_RATE, move2);
        delay(tempo / 4);

        moveNServos(tempo * MID_RATE, move3);
        delay(tempo / 4);

        moveNServos(tempo * MID_RATE, move2);
        delay(tempo / 4);

        moveNServos(tempo * MID_RATE, move3);
        delay(tempo / 4);
    }

    defaultPosition();
}

void legRaise2(int steps, int tempo, bool isRight)
{
    if (isRight == true)
    {
        int move1[] = {20, 60, 90, 90};
        int move2[] = {20, 90, 120, 90};

        for (int i = 0; i < steps; i++)
        {
            moveNServos(tempo * 0.7, move1);
            delay(tempo / 4);

            moveNServos(tempo * 0.7, move2);
            delay(tempo / 4);
        }
    }
    else
    {
        int move1[] = {120, 160, 90, 90};
        int move2[] = {90, 160, 90, 60};

        for (int i = 0; i < steps; i++)
        {
            moveNServos(tempo * 0.7, move1);
            delay(tempo / 4);

            moveNServos(tempo * 0.7, move2);
            delay(tempo / 4);
        }
    }

    defaultPosition();
}

void legRaise3(int steps, int tempo, bool isRight)
{
    if (isRight == true)
    {
        int move1[] = {20, 60, 90, 90};
        int move2[] = {20, 90, 90, 90};

        for (int i = 0; i < steps; i++)
        {
            moveNServos(tempo * 0.5, move1);
            delay(tempo / 4);

            moveNServos(tempo * 0.5, move2);
            delay(tempo / 4);
        }
    }
    else
    {
        int move1[] = {120, 160, 90, 90};
        int move2[] = {90, 160, 90, 90};

        for (int i = 0; i < steps; i++)
        {
            moveNServos(tempo * 0.5, move1);
            delay(tempo / 4);

            moveNServos(tempo * 0.5, move2);
            delay(tempo / 4);
        }
    }

    defaultPosition();
}

void legRaise4(int tempo, bool isRight)
{
    if (isRight == true)
    {
        int move1[] = {20, 60, 90, 90};
        int move2[] = {20, 90, 90, 90};

        moveNServos(tempo * MID_RATE, move1);
        delay(tempo / 4);

        moveNServos(tempo * MID_RATE, move2);
        delay(tempo / 4);
    }
    else
    {
        int move1[] = {120, 160, 90, 90};
        int move2[] = {90, 160, 90, 90};

        moveNServos(tempo * MID_RATE, move1);
        delay(tempo / 4);

        moveNServos(tempo * MID_RATE, move2);
        delay(tempo / 4);
    }

    defaultPosition();
}

void sitDown()
{
    int move1[] = {150, 90, 90, 90};
    int move2[] = {150, 30, 90, 90};

    moveNServos(DEFAULT_TEMPO * ULTRA_LOW_RATE, move1);
    delay(DEFAULT_TEMPO / 2);

    moveNServos(DEFAULT_TEMPO * ULTRA_LOW_RATE, move2);
    delay(DEFAULT_TEMPO / 2);

    defaultPosition();
}

void raiseFoot(bool isRight, int tempo)
{
    if (isRight == true)
    {
        int move1[] = {CENTER - 2 * AMPLITUDE, CENTER - AMPLITUDE, CENTER, CENTER};
        int move2[] = {CENTER + AMPLITUDE, CENTER - AMPLITUDE, CENTER, CENTER};
        int move3[] = {CENTER - 2 * AMPLITUDE, CENTER - AMPLITUDE, CENTER, CENTER};

        moveNServos(tempo * LOW_RATE, move1);
        delay(tempo * 2);

        moveNServos(tempo * ULTRA_HIGH_RATE, move2);
        delay(tempo / 2);

        moveNServos(tempo * ULTRA_HIGH_RATE, move3);
        delay(tempo);
    }
    else
    {
        int move1[] = {CENTER + AMPLITUDE, CENTER + 2 * AMPLITUDE, CENTER, CENTER};
        int move2[] = {CENTER + AMPLITUDE, CENTER - AMPLITUDE, CENTER, CENTER};
        int move3[] = {CENTER + AMPLITUDE, CENTER + 2 * AMPLITUDE, CENTER, CENTER};

        moveNServos(tempo * LOW_RATE, move1);
        delay(tempo * 2);

        moveNServos(tempo * ULTRA_HIGH_RATE, move2);
        delay(tempo / 2);

        moveNServos(tempo * ULTRA_HIGH_RATE, move3);
        delay(tempo);
    }

    defaultPosition();
}

void shakeIt()
{
    double pause;
  
    int move1[NB_SERVOS] = {90, 90, 80, 100};
    int move2[NB_SERVOS] = {90, 90, 100, 80};

    for (int i = 0; i < 9; i++)
    {
        pause = millis();
        moveNServos(DEFAULT_TEMPO * 0.15, move1);
        moveNServos(DEFAULT_TEMPO * 0.15, move2);
        while (millis() < (pause + DEFAULT_TEMPO));
    }

    defaultPosition();
}

void dance1()
{
    raiseFoot(true , DEFAULT_TEMPO);
    raiseFoot(false, DEFAULT_TEMPO);

    shakeIt();

    moonWalk(false, 4, DEFAULT_TEMPO * 2);
    moonWalk(true , 4, DEFAULT_TEMPO * 2);

    drunk(DEFAULT_TEMPO * 4);

    kickLeft (DEFAULT_TEMPO);
    kickRight(DEFAULT_TEMPO);

    drunk(DEFAULT_TEMPO / 2);

    run(4, DEFAULT_TEMPO * 4);
    defaultPosition();

    backward(2, DEFAULT_TEMPO * 2);
    defaultPosition();

    goingUp  (DEFAULT_TEMPO * 1);
    defaultPosition();
}

void dance2()
{
    sitDown();

    raiseFoot(true , DEFAULT_TEMPO);
    raiseFoot(false, DEFAULT_TEMPO);

    drunk(DEFAULT_TEMPO / 2);
    drunk(DEFAULT_TEMPO);

    kickLeft (DEFAULT_TEMPO);
    kickRight(DEFAULT_TEMPO);

    runFast(4, DEFAULT_TEMPO * 4);
    defaultPosition();

    backward(2, DEFAULT_TEMPO * 4);
    noGravity(DEFAULT_TEMPO);

    raiseFoot(true , DEFAULT_TEMPO);
    raiseFoot(false, DEFAULT_TEMPO);

    shakeIt();
    upDown(5, DEFAULT_TEMPO);
}

void dance3()
{
    flapping(1, DEFAULT_TEMPO);
    drunk(DEFAULT_TEMPO);
    kickLeft(DEFAULT_TEMPO);
    runFast(4, DEFAULT_TEMPO * 4);
    defaultPosition();

    raiseFoot(false, DEFAULT_TEMPO);
    sitDown();
  
    legRaise(DEFAULT_TEMPO, true);
    swing(5, DEFAULT_TEMPO);
    backward(2, DEFAULT_TEMPO * 4);
  
    goingUp(DEFAULT_TEMPO);
    noGravity(DEFAULT_TEMPO);
    upDown(5, DEFAULT_TEMPO);
 
    legRaise1(DEFAULT_TEMPO, true);
    legRaise2(4, DEFAULT_TEMPO, false);

    kickRight(DEFAULT_TEMPO);
    goingUp(DEFAULT_TEMPO);
    legRaise3(4, DEFAULT_TEMPO, true);
 
    kickLeft(DEFAULT_TEMPO);
    legRaise4(DEFAULT_TEMPO, true);

    shakeIt();
    sitDown();
}

void startDance(unsigned char danceIndex)
{
    LOG("Dancing #");
    LOG_LINE(danceIndex);
  
    servoAttach();

    switch (danceIndex)
    {
    case 1:
        dance1();
        break;
    case 2:
        dance2();
        break;
    case 3:
        dance3();
        break;
    default:
        break;
    }

    servoDetach();
}

int getDistance()
{
    int distance;

    servoDetach();
      
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);

    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);

    digitalWrite(TRIG_PIN, LOW);
    
    distance = (int)pulseIn(ECHO_PIN, HIGH) / 58;

    LOG("Distance: ");
    LOG(distance);

    return distance;
}

void obstacleMode()
{
    bool turnRight = true;
    int  distance;
    int  st188ValueLeft;
    int  st188ValueRight;

    distance = getDistance();

    if (distance >= 1 && distance <= 300)
    {
        st188ValueLeft  = analogRead(ST188_L_PIN);
        st188ValueRight = analogRead(ST188_R_PIN);

        LOG("\tst188ValueLeft: ");
        LOG(st188ValueLeft);
        LOG("\tst188ValueRight: ");
        LOG_LINE(st188ValueRight);

        if (st188ValueLeft >= 1000 && st188ValueRight >= 1000)
        {
            LOG_LINE("GO BACKWARD 1");
            walk(3, DEFAULT_TEMPO * 4, false);

            if (turnRight == true)
            {
                turn(3, DEFAULT_TEMPO * 4, true);
            }
            else
            {
                turn(3, DEFAULT_TEMPO * 4, false);
            }
        }
        else if (st188ValueLeft >= 1000 && st188ValueRight < 1000)
        {
            LOG_LINE("TURN RIGHT 2");
            turnRight = true;
            turn(3, DEFAULT_TEMPO * 4, true);
        }
        else if (st188ValueLeft < 1000 && st188ValueRight >= 1000)
        {
            LOG_LINE("TURN LEFT 3");
            turnRight = false;
            turn(3, DEFAULT_TEMPO * 4, false);
        }
        else if (st188ValueLeft < 1000 && st188ValueRight < 1000)
        {
            if (distance < 5)
            {
                LOG_LINE("GO BACKWARD 4");
                walk(3, DEFAULT_TEMPO * 4, false);

                if (turnRight == true)
                {
                    turn(3, DEFAULT_TEMPO * 4, true);
                }
                else
                {
                    turn(3, DEFAULT_TEMPO * 4, false);
                }
            }
            else if (distance >= 5 && distance <= 20)
            {
                LOG_LINE("TURN RIGHT 5");

                if (turnRight == true)
                {
                    turn(1, DEFAULT_TEMPO * 4, true);
                }
                else
                {
                    turn(1, DEFAULT_TEMPO * 4, false);
                }
            }
            else
            {
                LOG_LINE("GO FORWARD 6");
                walk(1, DEFAULT_TEMPO * 4, true);
            }
        }
    }
    else
    {
        LOG_LINE("STOP 7");
        stop();
    }
}

void followMode()
{
    int distance;
    int st188ValueLeft;
    int st188ValueRight;

    distance = getDistance();

    if (distance >= 1 && distance <= 300)
    {
        st188ValueLeft  = analogRead(ST188_L_PIN);
        st188ValueRight = analogRead(ST188_R_PIN);
        LOG("\tst188ValueLeft: ");
        LOG(st188ValueLeft);
        LOG("\tst188ValueRight: ");
        LOG_LINE(st188ValueRight);

        if (st188ValueLeft >= 1000 && st188ValueRight >= 1000)
        {
            LOG_LINE("GO FORWARD 1");
            walk(1, DEFAULT_TEMPO * 4, true);
        }
        else if (st188ValueLeft >= 1000 && st188ValueRight < 1000)
        {
            LOG_LINE("TURN LEFT 2");
            turn(1, DEFAULT_TEMPO * 4, false);
        }
        else if (st188ValueLeft < 1000 && st188ValueRight >= 1000)
        {
            LOG_LINE("TURN RIGHT 3");
            turn(1, DEFAULT_TEMPO * 4, true);
        }
        else if (st188ValueLeft < 1000 && st188ValueRight < 1000)
        {
            if (distance > 20)
            {
                LOG_LINE("STOP 4");
                stop();
            }
            else
            {
                LOG_LINE("GO FORWARD 5");
                walk(1, DEFAULT_TEMPO * 4, true);
            }
        }
    }
    else
    {
        LOG_LINE("STOP 6");
        stop();
    }
}

void voltageMeasure()
{
    if (millis() - voltageMeasureTime > 10000)
    {
        int    adcValue       = analogRead(VOLTAGE_MEASURE_PIN);
        double voltageMeasure = adcValue * VREF / 1024;
        double vcc            = voltageMeasure * (RES1 + RES2) / RES2;
        LOG("VCC = ");
        LOG(vcc);
        LOG_LINE(" V");

        if (vcc < 4.8)
        {
            ledFlag = false;
        }
        else
        {
            ledFlag = true;
        }
        voltageMeasureTime = millis();
    }

    if (ledFlag == true)
    {
        analogWrite(INDICATOR_LED_PIN, 255);
    }
    else
    {
        if (millis() - ledBlinkTime < 500)
        {
            analogWrite(INDICATOR_LED_PIN, 255);
        }
        else if (millis() - ledBlinkTime >= 500 && millis() - ledBlinkTime < 1000)
        {
            analogWrite(INDICATOR_LED_PIN, 0);
        }
        else
        {
            ledBlinkTime = millis();
        }
    }
}

bool getIrValue(unsigned long *irValue)
{
    decode_results results;

    if (irRecv.decode(&results) != 0)
    {
        *irValue = results.value;
        irRecv.resume();
        return true;
    }

    return false;
}

void servoAttach()
{
    servo[0].attach(LOW_LEFT_PIN);
    servo[1].attach(LOW_RIGHT_PIN);
    servo[2].attach(UP_LEFT_PIN);
    servo[3].attach(UP_RIGHT_PIN);
}

void servoDetach()
{
    servo[0].detach();
    servo[1].detach();
    servo[2].detach();
    servo[3].detach();
}

void setup()
{
    Serial.begin   (9600);
    mp3Serial.begin(9600);

    pinMode(ECHO_PIN, INPUT);
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(INDICATOR_LED_PIN  , OUTPUT);
    pinMode(VOLTAGE_MEASURE_PIN, INPUT);

    analogWrite(INDICATOR_LED_PIN, 255);

    mp3Player.init();
    irRecv.enableIRIn();

    analogReference(INTERNAL);

    servoAttach();
    stop();

    delay(2000);

    mp3Player.play(1);
    servoAttach();
    shakeIt();
    sitDown();            
    stop();
}

void loop()
{
    unsigned long irValue;
   
    voltageMeasure();

    if (getIrValue(&irValue) == true)
    {
        switch (irValue)
        {
        case BTN_FORWARD:
            mp3Player.play(13);
            mainMode  = IR_CONTROL;
            direction = FORWARD;
            break;
        case BTN_BACKWARD:
            mp3Player.play(14);
            mainMode  = IR_CONTROL;
            direction = BACKWARD;
            break;
        case BTN_LEFT:
            mp3Player.play(15);
            mainMode  = IR_CONTROL;
            direction = TURN_LEFT;
            break;
        case BTN_RIGHT:
            mp3Player.play(16);
            mainMode  = IR_CONTROL;
            direction = TURN_RIGHT;
            break;
        case BTN_MODE:
            if (mainMode == FOLLOW)
            {
                mp3Player.play(17);
                delay(1000);
                mainMode = OBSTACLES_AVOIDANCE;
            }
            else
            {
                mp3Player.play(18);
                delay(1000);
                mainMode = FOLLOW;
            }
            break;
        case BTN_IDLE:
            mp3Player.play(10);
            delay(1000);
            mainMode = IDLE;
            break;
        case BTN_MUSIC:
            mp3Player.play(11);
            delay(1000);
            mp3Player.play(musicIndex);
            mainMode = MUSIC;
            break;
        case BTN_DANCE:
            mp3Player.play(12);
            delay(1000);
            mp3Player.play(danceIndex + 1);
            startDance(danceIndex);
            mainMode = DANCE;
            break;
        case BTN_DOWN:
            if (mainMode == MUSIC)
            {
                musicIndex++;
                if (musicIndex > 4)
                {
                    musicIndex = 2;
                }
                mp3Player.play(19);
                delay(1000);
                mp3Player.play(musicIndex);
            }
            if (mainMode == DANCE)
            {
                danceIndex++;
                if (danceIndex > 3)
                {
                    danceIndex = 1;
                }
                mp3Player.play(22);
                delay(1000);
                mp3Player.play(danceIndex + 1);
                startDance(danceIndex);
            }
            if (mainMode == VOLUME)
            {
                mp3Player.volumeDown();
                mp3Player.play(23);
                delay(500);
            }
            break;
        case BTN_UP:
            if (mainMode == MUSIC)
            {
                musicIndex--;
                if (musicIndex < 2)
                {
                    musicIndex = 4;
                }
                mp3Player.play(20);
                delay(1000);
                mp3Player.play(musicIndex);
            }
            if (mainMode == DANCE)
            {
                danceIndex--;
                if (danceIndex < 1)
                {
                    danceIndex = 3;
                }
                mp3Player.play(21);
                delay(1000);
                mp3Player.play(danceIndex + 1);
                startDance(danceIndex);
            }
            if (mainMode == VOLUME)
            {
                mp3Player.volumeUp();
                mp3Player.play(23);
                delay(500);
            }
            break;
        case BTN_VOL:
            mainMode = VOLUME;
            mp3Player.play(23);
            delay(500);
            break;
        default:
            break;
        }
    }

    if ((oldMainMode == IR_CONTROL || oldMainMode == OBSTACLES_AVOIDANCE || oldMainMode == FOLLOW)
      && (mainMode != oldMainMode))
    {
        stop();
    }

    oldMainMode = mainMode;

    switch (mainMode)
    {
    case IR_CONTROL:
        switch (direction)
        {
        case FORWARD:
            walk(1, DEFAULT_TEMPO * 4, true);
            break;
        case BACKWARD:
            walk(1, DEFAULT_TEMPO * 4, false);
            break;
        case TURN_RIGHT:
            turn(1, DEFAULT_TEMPO * 4, true);
            break;
        case TURN_LEFT:
            turn(1, DEFAULT_TEMPO * 4, false);
            break;
        default:
            break;
        }
        break;
    case OBSTACLES_AVOIDANCE:
        obstacleMode();
        break;
    case FOLLOW:
        followMode();
        break;
    case MUSIC:
    case DANCE:
    case IDLE:
    case VOLUME:
    default:
        delay(10);
        break;
    }
}
