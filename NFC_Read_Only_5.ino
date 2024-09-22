#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN         D0          // Configurable, see typical pin layout above
#define SS_PIN          D8          // Configurable, see typical pin layout above

MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance.

MFRC522::MIFARE_Key key;   // NFC를 읽기 위한 Key 값 - 아래에서 패스워드 6자리를 넣음.

char Read_Data[16*6*16];    // 전체 Data를 하나의 배열에 모두 넣어 놓는 곳.
int data_count = 0;         // 전체 Data의 사이즈.
char each_data[10][350];    // 전체 데이터를 각각의 Data로 분리 해 넣는 곳. 
int word_count = 0;         // 분리한 데이터의 갯수.
/**
 * Initialize.
 */
void setup() {
    Serial.begin(115200); // Initialize serial communications with the PC
    while (!Serial);    //  Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
    SPI.begin();        //  Init SPI bus
    mfrc522.PCD_Init(); //  Init MFRC522 card     // 초기화.

    key.keyByte[0] = 0xD3;    // Data 블럭의 일반적인 Reading 패스워드  6자리 
    key.keyByte[1] = 0xF7;
    key.keyByte[2] = 0xD3;
    key.keyByte[3] = 0xF7;
    key.keyByte[4] = 0xD3;
    key.keyByte[5] = 0xF7;
    clear_data();
}

/**
 * Main loop.
 */
void loop() {
    // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
    if ( ! mfrc522.PICC_IsNewCardPresent()){  
        setup();
    }
    // Select one of the cards  
    if ( ! mfrc522.PICC_ReadCardSerial()){
        return;
    }
    MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
    // Check for compatibility   Mifare 인지 확인 하고 아니면 리턴함.
    if (    piccType != MFRC522::PICC_TYPE_MIFARE_MINI
        &&  piccType != MFRC522::PICC_TYPE_MIFARE_1K
        &&  piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
        Serial.println(F("This sample only works with MIFARE Classic cards."));
        return;
    }

    // In this sample we use the second sector,
    // that is: sector #1, covering block #4 up to and including block #7
    byte sector         = 1;   // 총 16섹터 중에서 0 번 쎅터를 읽고 나서, Data Block은 sector 1 부터 읽음.
    byte blockAddr      = 4;   // 각 섹터는 4 개의 블럭으로 이루어져 있음.

    byte trailerBlock   = 7;   // Data Block의 passwd 가 저장 되는 블럭의 위치 중에서 첫번째.
    MFRC522::StatusCode status;
    byte buffer[18]; 
    byte size = sizeof(buffer);

    // Authenticate using key A
    int i;
    bool break_status = false;     // 이중 loop 를 빠져 나가기 위해서 만들어 놓은 status 변수
    for (i=0;i<16;i++) {           // 총 16개 블럭 중에서 15개의 데이터 블럭을 한 블럭씩 15번 읽기 위해서 만든 루프
      status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock + i*4, &key, &(mfrc522.uid)); // trailerBlock + i*4 는 각 4블럭 마다( 1 섹터 마다) 있는 패스워드 블럭으로 이동 하기 위한 연산.
      if (status != MFRC522::STATUS_OK) {   // 제대로 읽었으면 패스 . 못읽었으면 아래 에러 메세지 수행.
        Serial.print(F("PCD_Authenticate() failed: "));
        Serial.println(mfrc522.GetStatusCodeName(status));
        return;
      }

    // Read data from the block      // 제대로 읽었을때 수행 되는 루틴.
      for (blockAddr = 4+ i*4 ; blockAddr < 7 + i*4 ; blockAddr ++ ) {    // 한 섹터 마다, 3개의 데이터 블럭을 읽기 위함.  - 이 루프는 3번 돌도록 되어 있음.  4 부터 7 까지.
        status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(blockAddr, buffer, &size);   // 각각의 블럭이 16 바이트인데, 왜 18 바이트를 읽는지는 잘 모르겠음. ㅜ.ㅜ 일단 각 블럭을 읽는 루틴.
        if (status != MFRC522::STATUS_OK) {
          Serial.print(F("MIFARE_Read() failed: "));
          Serial.println(mfrc522.GetStatusCodeName(status));
        }
        if ( dump_byte_array2(buffer, 16) == 0 ) {  // 각 블럭의 읽어들인 데이터를 Read_data로 합치기 위해 dump_byte_array2를 호출 함.
          break_status = true;     // break 로 현재 loop를 빠진 다음에도 다음 loop를 빠져 나가기 위해서 True로 지정.
          break;
        }
      }
      if (break_status == true ) break;  // loop 한꺼번에 빠져 나가기.
    }

    print_each_data_1();  // Read_data에 들어 있는 모든 데이터를 파싱 해서, 프린트 하기.

    mfrc522.PICC_HaltA();
    // Stop encryption on PCD
    mfrc522.PCD_StopCrypto1();
    print_data();    // Read_data에 들어 있는 모든 데이터를 파싱 해서, each_data로 나누어 저장 하는 루틴.
   delay(3000);      // 계속 스크롤 되는 것이 보기 싫어서 잠시 웨이팅 하도록 넣은 딜레이
   SPI.begin();        // Init SPI bus     // 기기 초기화
   mfrc522.PCD_Init(); // Init MFRC522 card  // 기기 초기화.
   clear_data();   // data 초기화.
}

/**
 * Helper routine to dump a byte array as hex values to Serial.
 */

int dump_byte_array2(byte *buffer, byte bufferSize) {
    for (byte i = 0; i < bufferSize; i++) {
        Read_Data[data_count++] = buffer[i];    // 각각의 블럭 데이터를 Read_data로 옮기는 것.
        if ( buffer[i] == 0xFE  ) return 0;     // 데이터의 맨 마지막이 FE 므로 빠져 나감.  이경우 마지막인 것을 알리기 위해서 return 0을 함.
    }
    return 1;
}


void print_each_data_1(){
   bool print_on = false;
   Serial.println();
   Serial.println("============= 데이터 출력하기 ====================");
   int i = 0;
   int word_length;

   int j = 0;   // 읽은 데이터의 갯수
   int k = 0;   // 각 데이터의 크기.
   while(i<data_count-1) {    // 데이터가 FE까지 포함 하므로, 하나를 뺐음.. -- 사실 밑에 FE와 같으면 break 하는 루틴은 없어도 됨.
          if ( (Read_Data[i-1] == 0x6E && Read_Data[i-2] == 0x65 ) &&  Read_Data[i] != 0x3D ) {     // 하나의 데이터 사이즈를 데이터로 가지고 있으나, 이를 계산 하기 힘들어서(조금씩 달라지고 큰 수의 경우 패턴이 달라져서. ㅜ.ㅜ ) "en" 이라는 것 다음에 데이터가 쓰여지는 것을 보고, 이를 체크 함. 
            print_on = true;     // en 이후에 0x11 까지는 데이터 이므로, 그때 까지 each_data에 옮기기 위해서 설정한 플래그
            k = 0;
          }
          
          if ( print_on == true && (Read_Data[i] == 0x11 || (Read_Data[i] == 0x51 && Read_Data[i+1] == 0x01)) ) {  // 다음 데이터의 시작이 꼭 0x11 부터 시작 되어서 그 전까지를 읽는 것으로 햇음.  2번째 ||로 체크 하는 것은 마지막 데이터는 0x51 로 시작함. - 왜 그러는지는 모르겠음. ㅜ.ㅜ
            Serial.println();   // 한줄 띄우기.  지워도 됨.
            print_on = false; 
            j = j +1;
          }
          if ( print_on == true ) {   // 정상적인 데이터 인 경우에는 each_data로 카피 하는 루틴.
            Serial.print(Read_Data[i]);  // 지워도 됨.
            each_data[j][k++] = Read_Data[i];
            each_data[j][k] = 0x00;
          }

          if ( Read_Data[i] == 0xFE ) break;
          i = i + 1;
   }

   word_count = j;  // j는 데이터 갯수를 의미 하므로, 전역 변수로 옮겨 놓음.
   Serial.println();   Serial.println("============= end ====================");
}

void print_data(){
   int j,k;
   Serial.println("============= 분리된 데이터 출력하기 ====================");
   for (j = 0; j<= word_count ; j ++ ) {   // 전역 변수 word_count  만큼의 데이터를 출력함.
      for ( k = 0; k < 350; k ++ ) {
        if ( each_data[j][k] == 0x00 ) break;    // 마지막에 Null 값일때 까지 출력.
        Serial.print(each_data[j][k]);
      }
      Serial.println();
   }
   Serial.println("============= 끝 ====================");
}
void clear_data() {
    for (int i = 0; i < sizeof(Read_Data); i++) {
        Read_Data[i] = 0x00;  // 데이터를 모두 0으로 초기화    - 혹시 모를 가비지 데이터를 없이기 위함.
    }
    data_count = 0;  // 데이터 카운터도 초기화
}
