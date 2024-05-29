#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <math.h>
#include "definitions.h"

// C++에서 정의된 함수 선언
#ifdef __cplusplus
extern "C" {
#endif
    //void detectQRCode(struct QRCodeInfo *qr_info, bool *qr_detected);
#ifdef __cplusplus
}
#endif

// 글로벌 변수와 뮤텍스 정의
DGIST global_dgist;
int sock;
pthread_mutex_t dgist_mutex = PTHREAD_MUTEX_INITIALIZER;
Point current, next;
Robot robot;

// 디버깅용
void print_received_map(Node map[ROW][COL]) {
    for (int i = ROW-1; i >= 0; i--) {
        for (int j = 0; j < COL; j++) {
            if (map[i][j].item.score > 0 && map[i][j].item.score < 5) {
                printf("%d ", map[i][j].item.score);
            } else {
                printf("- ");
            }
        }
        printf("\n");
    }
    printf("\n");
}

Point find_next_destination(Node map[ROW][COL]) {
    int directions[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    Point best_point = {robot.x, robot.y};
    int best_score = -1;

    // 거리가 1인 곳 확인
    for (int i = 0; i < 4; i++) {
        int new_x = robot.x + directions[i][0];
        int new_y = robot.y + directions[i][1];

        if (new_x >= 0 && new_x < ROW && new_y >= 0 && new_y < COL) {
            int score = map[new_x][new_y].item.score;
            if (score > best_score) {
                best_score = score;
                best_point.x = new_x;
                best_point.y = new_y;
            }
        }
    }
    return best_point;
}

// 로봇의 이동 명령을 결정하는 함수
int order(Point destination) {
    // 목적지가 로봇의 현재 위치에 상대적으로 어디에 있는지 계산
    int dx = destination.x - robot.x; // 동서 방향
    int dy = destination.y - robot.y; // 남북 방향

    switch (robot.direction) {
        case NORTH:
            if (dy > 0 && dx == 0) return 1; // 앞으로
            if (dx < 0 && dy == 0) return 2; // 왼쪽으로 회전
            if (dx > 0 && dy == 0) return 3; // 오른쪽으로 회전
            break;
        case SOUTH:
            if (dy < 0 && dx == 0) return 1; // 앞으로
            if (dx > 0 && dy == 0) return 2; // 왼쪽으로 회전
            if (dx < 0 && dy == 0) return 3; // 오른쪽으로 회전
            break;
        case EAST:
            if (dx > 0 && dy == 0) return 1; // 앞으로
            if (dy < 0 && dx == 0) return 2; // 왼쪽으로 회전
            if (dy > 0 && dx == 0) return 3; // 오른쪽으로 회전
            break;
        case WEST:
            if (dx < 0 && dy == 0) return 1; // 앞으로
            if (dy > 0 && dx == 0) return 2; // 왼쪽으로 회전
            if (dy < 0 && dx == 0) return 3; // 오른쪽으로 회전
            break;
    }

    return 0; // 이동하지 않음
}

int should_place_bomb(DGIST* dgist, int my_index, double elapsed_time) {
    int opponent_index = (my_index == 0) ? 1 : 0;
    int opponent_x = dgist->players[opponent_index].row;
    int opponent_y = dgist->players[opponent_index].col;

    // 적과의 거리가 2 이하일 경우에 폭탄 설치
    if (sqrt(pow(robot.x - opponent_x, 2) + pow(robot.y - opponent_y, 2)) <= 2) {
        return 1;
    }

    if (elapsed_time >= 90) { // 게임 시작 후 90초 이후에는 폭탄 설치
        return 1;
    }

    return 0;
}

void* qr_thread(void* arg) {
    struct QRCodeInfo qr_info;
    bool qr_detected = false;
    time_t start_time = *(time_t*)arg;

    while (true) {
        //detectQRCode(&qr_info, &qr_detected);
        
        if (qr_detected) {
            printf("QR Code Detected:\n");
            robot.x = qr_info.x;
            robot.y = qr_info.y;
            printf("Current location: (%d, %d)\n", robot.x, robot.y);
            qr_detected = false; // Reset the flag for the next detection

            // 뮤텍스를 사용하여 글로벌 데이터 접근
            pthread_mutex_lock(&dgist_mutex);
            double elapsed_time = difftime(time(NULL), start_time);

            // 폭탄 설치 여부 결정
            if (should_place_bomb(&global_dgist, 1, elapsed_time)) {
                printf("Place a bomb at (%d, %d)\n", robot.x, robot.y);
                send_action(sock, robot.x, robot.y, setBomb);
            } else {
                printf("Do not place a bomb at (%d, %d)\n", robot.x, robot.y);
                send_action(sock, robot.x, robot.y, move);
            }

            // 다음 목적지 선택
            next = find_next_destination(global_dgist.map);
            int order = order(next);
            printf("Next destination: (%d, %d)\n", next.x, next.y);

            pthread_mutex_unlock(&dgist_mutex);
        }

        usleep(100000); // 0.1초 대기
    }

    return NULL;
}

void* server_thread(void* arg) {
    DGIST dgist;

    while (true) {
        // 서버로부터 DGIST 구조체 수신
        receive_dgist(sock, &dgist);

        // 뮤텍스를 사용하여 글로벌 데이터 업데이트
        pthread_mutex_lock(&dgist_mutex);
        global_dgist = dgist;
        print_received_map(global_dgist.map);
        pthread_mutex_unlock(&dgist_mutex);

        usleep(100000); // 0.1초 대기
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <server_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);

    pthread_t qr_tid, server_tid;
    time_t start_time;
    time(&start_time);

    // 소켓 생성
    sock = create_socket(server_ip, server_port);

    int robot_index = 1; // TODO: 경기에 따라 다름 이거 설정할 수 있게 하기

    if (robot_index == 1) {
        robot = {0, 0, 0, EAST};
    } else {
        robot = {4, 4, 0, WEST};
    }
    
    // QR 코드 인식 스레드 시작
    if (pthread_create(&qr_tid, NULL, qr_thread, &start_time) != 0) {
        perror("Failed to create QR thread");
        exit(1);
    }

    // 서버 통신 스레드 시작
    if (pthread_create(&server_tid, NULL, server_thread, NULL) != 0) {
        perror("Failed to create server thread");
        exit(1);
    }

    // 스레드 종료 대기
    pthread_join(qr_tid, NULL);
    pthread_join(server_tid, NULL);

    // 메인 함수 종료 시 소켓 닫기
    close(sock);
    printf("Socket closed in main.\n");

    return 0;
}