#include <stdio.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include "car_control.h"
#include "car_tracking.h"
#include "definitions.h"

int COMMAND;

void setup() {
    // Init. WiringPi
    if (wiringPiSetup() == -1) {
        printf("Failed to set up WiringPi.\n");
        exit(1);
    }
    printf("Setup #1 success!\n");

    // Init. WiringPiI2C
    fd = wiringPiI2CSetup(CAR_ADDRESS);
    if (fd == -1) {
        printf("Failed to initialize I2C\n");
        exit(1);
    }
    printf("Setup #2 success!\n");
    
    // Set INPUT Mode
    pinMode(Tracking_Left1, INPUT);
    pinMode(Tracking_Left2, INPUT);
    pinMode(Tracking_Right1, INPUT);
    pinMode(Tracking_Right2, INPUT);
    printf("Setup all clear!\n");
}

void move_forward(int Tracking_Left1Value, int Tracking_Left2Value, int Tracking_Right1Value, int Tracking_Right2Value) {
    // Pin status => Turn Right
    // 0 0 X 0
    // 1 0 X 0
    // 0 1 X 0
    if ((Tracking_Left1Value == LOW || Tracking_Left2Value == LOW) && Tracking_Right2Value == LOW) {
        Run_Car(70, -40);
        delay(200); // 0.2초 대기
    }
    // Pin status => Turn Left
    // 0 X 0 0
    // 0 X 0 1
    // 0 X 1 0
    else if (Tracking_Left1Value == LOW && (Tracking_Right1Value == LOW || Tracking_Right2Value == LOW)) {
        Run_Car(-40, 70);
        delay(200); // 0.2초 대기
    }
    // Leftmost
    else if (Tracking_Left1Value == LOW) {
        Run_Car(-70, 70);
        delay(50); // 0.05초 대기
    }
    // Rightmost
    else if (Tracking_Right2Value == LOW) {
        Run_Car(70, -70);
        delay(50); // 0.05초 대기
    }
    // 왼쪽 작은 커브 처리
    else if (Tracking_Left2Value == LOW && Tracking_Right1Value == HIGH) {
        Run_Car(-60, 60);
        delay(20); // 0.02초 대기
    }
    // 오른쪽 작은 커브 처리
    else if (Tracking_Left2Value == HIGH && Tracking_Right1Value == LOW) {
        Run_Car(60, -60);
        delay(20); // 0.02초 대기
    }
    // Straight
    else if (Tracking_Left2Value == LOW && Tracking_Right1Value == LOW) {
        Run_Car(70, 70);
    }
}

void move_left() {
    Run_Car(-35, 80);
    delay(200);
}

void move_right() {
    Run_Car(80, -35);
    delay(200);
}

int intersection_signal(int l1, int l2, int r1, int r2)
{
    // Return 1 if [LLLH] or [HLLL] or [LLLL]
    return (l1 == LOW && l2 == LOW && r1 == LOW) || (l2 == LOW && r1 == LOW && r2 == LOW);
}

void tracking_function()
{
    while (1) {
        // Set up Sensors
        int Tracking_Left1Value = digitalRead(Tracking_Left1);
        int Tracking_Left2Value = digitalRead(Tracking_Left2);
        int Tracking_Right1Value = digitalRead(Tracking_Right1);
        int Tracking_Right2Value = digitalRead(Tracking_Right2);

        // When it arrives at an intersection, stop_signal() returns 1.
        int INTERSECTION = intersection_signal(Tracking_Left1Value, Tracking_Left2Value, Tracking_Right1Value, Tracking_Right2Value);

        // INTERSECTION => Follow the COMMAND
        if (INTERSECTION) {
            switch (COMMAND) {

                // Left-Rotation
            case 2:
                while (1) {
                    // Left-Rotation until [HLLH]
                    // If [HLLH] => move_forward()
                    if (Tracking_Left1Value == HIGH && Tracking_Left2Value == LOW && Tracking_Right1Value == LOW && Tracking_Right2Value == HIGH) {
                        break;
                    }
                    move_left();
                }
                break;

                // Right-Rotation
            case 3:
                while (1) {
                    if (Tracking_Left1Value == HIGH && Tracking_Left2Value == LOW && Tracking_Right1Value == LOW && Tracking_Right2Value == HIGH) {
                        break;
                    }
                    move_right();
                }
                break;

                // Forward will use outer move_forward()
            default:
                break;
            }
        }

        // ~INTERSECTION => move_forward
        move_forward(Tracking_Left1Value, Tracking_Left2Value, Tracking_Right1Value, Tracking_Right2Value);
    }
}