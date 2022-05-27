/*
 * 코로나 테스터
 *
 * 2022.05. 06
 *
 * [포트설정]
 * 1. 상하 모터 깊이 : A0 (아날로그 리니어 저항)
 * 2. 상하 모터 제어 : D3, D5 (PWM으로 전력, 방향 제어 : 모터 정격전압 3V를 만들기 위함 / 정석은 아님)
 * 3. 회전 모터 제어 : D4 (단방향 회전 (정지=LOW, 회전=HIGH) / 5v 모터 사용시 일반 디지털핀으로 가능)
 * 4. 스위치 : A1 (디지털핀으로 활용)
 * 5. 상단 리밋 : A2 (디지털핀으로 활용 - 회로를 바꾸면 안되니까, 그냥 HIGH일 때 걸리는 것으로. mblock에서 해결이 안날 듯.)
 * 6. 하단 리밋 : A3
 * 7. MP3 제어 : 쉴드 사용
 * 
 * [사운드]
 * 1. 검체 채취를 하려면 스위치를 누르세요
 * 2. 왼쪽 검사 준비 후 스위치를 누르세요
 * 3. 올라갑니다. 긴장하지 마세요.
 * 4. 면봉이 회전합니다. 움직이지 마세요.
 * 5. 면봉이 내려옵니다.
 * 6. 반대편 준비 후 스위치를 누르세요.
 * 7. 끝났습니다.
 * 10. 중간 알람
 * 11. 시작 알람
 *  
 * 
 */

// MP3 사용관련 라이브러리 
#include <SPI.h>
#include <SdFat.h>
#include <FreeStack.h>
#include <SFEMP3Shield.h>

// 모터 핀 정의
#define vMotorA 5   // 수직이송용 상하모터 A단자
#define vMotorB 3   // 수직이송용 상하모터 B단자
#define rMotor 4    // 면봉회전용 회전모터

// 모터 방향 정의
#define UP 1
#define DOWN -1

// 핀 정의
#define Range A0   // 높이 조절용 포텐션미터 (analog)
#define SW  A1      // 단계 전환용 스위치 (digital)
#define topLimit A2 // 위쪽 리밋스위치 (digital)
#define bottomLimit A3  // 아래쪽 리밋스위치 (digital)

int vMotorPower = 240;      // 모터가 3v에서 동작하므로 5v핀에서 3v를 만들기 위해 편법으로 PWM 사용함
int rollDelay = 10*1000;  // 면봉 돌아가는 기본 시간 ms

// MP3 관련 
SdFat sd;
SFEMP3Shield MP3player;
uint8_t result; 

////////////////////////////////////////////
// 필요한 함수 정의
////////////////////////////////////////////

// 상하 모터 제어 함수 (방향, 시간_ms)
void vMotorControl(int _dir, int _delay)
{
    // 방향에 따라 이동
    if(_dir == UP)
    {        
        Serial.println("===vMotor UP");
        analogWrite(vMotorA, vMotorPower);
        analogWrite(vMotorB, 0);        

        // 일정 시간 이동하도록 대기하기
        int cnt = 0;
        // 대기 시간이 남아있고, 리밋이 걸리지 않을 경우 
        while( cnt<_delay && digitalRead(topLimit)!=0 )
        {
            delay(10);   // 1ms 대기 - 타이머 쓰지 않음 (따로 할 일 없으므로)
            cnt = cnt + 1;  // 카운트 증가
        }
        // 대기시간이 지났거나, 리밋이 걸렸을 때 나감
    }
    else if(_dir == DOWN)
    {
        Serial.println("===vMotor DOWN");
        analogWrite(vMotorA, 0);
        analogWrite(vMotorB, vMotorPower);       

        // 아래로 내릴 때는 항상 리밋센서에 닿을 때 까지 내리기
        while(digitalRead(bottomLimit)!=0){}
    }

    // 모터 정지
    Serial.println("===vMotor STOP");
    analogWrite(vMotorA, 0);
    analogWrite(vMotorB, 0);
}

// 회전 모터 제어 함수 (회전시간 설정 ms)
void rMotorControl(int _delay)
{
    Serial.println(">>>rMotor ROLLING");
    digitalWrite(rMotor, HIGH);
    delay(_delay);
    Serial.println(">>>rMotor STOP");
    digitalWrite(rMotor, LOW);
}

// sPOWERON 상태일 때
void do_POWERON()
{
    Serial.println("POWER");
    // 1. 시작 멜로디 (track011)
    playTrack(11);
    delay(1000);
    
    // 2. 안내 (track001 : 채취 시작하려면 스위치 누르세요.)
    playTrack(1);    
}

// sREADY 상태일 때
void do_READY()
{
    Serial.println("READY");
    // 1. 알람 멜로디 (track010)
    playTrack(10);
    delay(1000);

    // 2. 안내 (track002 : 왼쪽 채취 준비하고 스위치 누르세요.)
    playTrack(2);
}

// sLEFT 상태일 때
void do_LEFT()
{
    Serial.println("LEFT");
    // 1. 알람 멜로디 (track010)
    playTrack(10);
    delay(1000);

    // 2. 면봉 위로 올리고, 돌리고, 내리는 통합 과정
    getSample();
    delay(500);
    
    // 3. 안내 (track006 : 반대편 준비하고 스위치 누르세요.)
    playTrack(6);    
}

// sRIGHT 상태일 때
void do_RIGHT()
{
    Serial.println("RIGHT");
    // 1. 알람 멜로디 (track010)
    playTrack(10);
    delay(1000);

    // 2. 면봉 위로 올리고, 돌리고, 내리는 통합 과정
    getSample();
    delay(500);
    
    // 3. 안내 (track007 : 끝났습니다. 수고하셨습니다.)
    playTrack(7);   
}

// 검체 채취를 위한 모터 이동/회전 자동화 과정
void getSample()
{
    // 포텐션미터값을 읽어서 상하모터 회전시간 결정
    int upDelay = analogRead(Range);
    upDelay = map(upDelay, 0, 1024, 0, 1500);

    Serial.println("----Motor Process");
    // 1. 안내 (track003 : 올라갑니다 긴장하지 마세요.)
    playTrack(3);
    delay(500);

    // 2. 상하모터 상승    
    vMotorControl(UP, upDelay);
    delay(500);

    // 3. 안내 (track004 : 면봉이 회전합니다.)
    playTrack(4);
    delay(500);

    // 4. 회전모터 회전
    rMotorControl(rollDelay);
    delay(500);

    // 5. 안내 (track005 : 면봉이 내려옵니다.)
    playTrack(5);
    delay(500);

    // 4. 상하모터 하강    
    vMotorControl(DOWN, upDelay);    
    delay(500);
}

//////////////////////////////////////////////

void MP3setup()
{
    // SD카드 초기화
    if (!sd.begin(SD_SEL, SPI_FULL_SPEED))
        sd.initErrorHalt();
    // depending upon your SdCard environment, SPI_HAVE_SPEED may work better.
    if (!sd.chdir("/"))
        sd.errorHalt("sd.chdir");

    // MP3 쉴드 초기화
    result = MP3player.begin();
    // 에러나면 표시해 주기
    if (result != 0)
    {
        Serial.print(F("Error code: "));
        Serial.print(result);
        Serial.println(F(" when trying to start MP3 player"));
        if (result == 6)
        {
            Serial.println(F("Warning: patch file not found, skipping."));           // can be removed for space, if needed.
            Serial.println(F("Use the \"d\" command to verify SdCard can be read")); // can be removed for space, if needed.
        }
    }

    // 볼륨 최대로
    // 제일 큰 소리가 (2), 작은 소리가 (254)
    MP3player.setVolume(2, 2); 
}

void waitPushSW()
{
    while(!digitalRead(SW)){}
}


void playTrack(int _trackNumber)
{
    // 해당 트랙 재생 후 결과 반환
    uint8_t result = MP3player.playTrack(_trackNumber);

    // 트랙 재생에 에러일 경우 에러 표시
    if (result != 0)
    {
        Serial.print(F("Error code: "));
        Serial.print(result);
        Serial.println(F(" when trying to play track"));
    }
    else
    {
        Serial.print(F("Playing : "));
        Serial.println(_trackNumber);

        // 지금 재생되는 트랙이 끝날 때 까지 대기
        while(MP3player.isPlaying()){}
    }
}


/////////////////////////////////////////////
// 
//  SETUP 과 LOOP 
//
/////////////////////////////////////////////

void setup()
{
    // 시리얼 모니터 연결, 속도 115200
    Serial.begin(115200);
    Serial.println("START");

    // MP3 초기화
    MP3setup();

    // 모터 핀 설정
    pinMode(vMotorA, OUTPUT);
    pinMode(vMotorB, OUTPUT);
    pinMode(rMotor, OUTPUT);

    // 입력 핀 설정
    pinMode(topLimit, INPUT_PULLUP);
    pinMode(bottomLimit, INPUT_PULLUP);    

    // 모터 맨 아래로 이동
    vMotorControl(DOWN, 1);    
}

void loop()
{
    // 1단계, 시작하세요.
    do_POWERON();
    waitPushSW();   // 스위치 눌러 다음 단계로

    // 2단계, 왼쪽 준비
    do_READY();
    waitPushSW();   // 스위치 눌러 다음 단계로
    
    // 3단계, 왼쪽 검사 : 올라가기, 회전하기, 내려오기 > 오른쪽 준비
    do_LEFT();
    waitPushSW();   // 스위치 눌러 다음 단계로

    // 4단계, 오른쪽 검사 : 올라가기, 회전하기, 내려오기 > 끝
    do_RIGHT();

    // 5초 후 다시 시작으로
    delay(5000);
}
