#pragma once
#include <stddef.h> /* size_t */
#define CSV_TABLE_MAX		10

/* ---- 데이터 테이블 ----
   rowCount : 데이터 행 수(헤더 제외)
   colCount : 열 수
   header   : [colCount] 헤더 문자열 배열(힙 할당)
   id       : [rowCount] 각 행의 첫 열을 정수화한 값
   cell     : [rowCount][colCount] 각 셀 문자열(힙 할당)
*/
typedef struct {
    int    rowCount;
    int    colCount;
    char** header;
    int* id;
    char*** cell;
} CsvTable;

/* -----------------------------------------------------------
   csvLoadAll
   - path의 CSV 파일을 읽어서 CsvTable을 힙 메모리에 채움
   - 성공 시 1, 실패 시 0 반환
   - 규칙: 첫 행=헤더, 첫 열=id(정수)
   - 메모리는 csvFreeTable로 해제해야 함
----------------------------------------------------------- */
int csvLoadAll(const char* path);

/* -----------------------------------------------------------
   csvGetValueInTable
   - 이미 메모리에 로드된 테이블에서 (wantedId, headerName)으로 값을 조회
   - out/outSize: 결과 문자열을 복사받을 버퍼/크기
   - 성공 시 1, 실패 시 0
----------------------------------------------------------- */
int csvGetValueInTable(const char* path,
    int            wantedId,
    const char* headerName,
    char* out, 
    size_t outSize);

/* -----------------------------------------------------------
   csvFreeTable
   - csvLoadAll로 확보된 모든 동적 메모리 해제
   - 호출 후 table 필드들은 무효화됨(포인터는 해제됨)
----------------------------------------------------------- */
void csvFreeTable(CsvTable* table);
