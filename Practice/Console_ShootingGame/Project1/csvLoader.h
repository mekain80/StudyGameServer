#pragma once
#include <stddef.h>
const int kCsvTableMax = 10;

// CsvTable
// fileName : 파일 이름
// rowCount : 데이터 행 수(헤더 제외)
// colCount : 열 수
// header   : 헤더 문자열 배열
// id       : 각 행의 첫 열을 정수화한 값(기본키) [rowCount]
// cell     : [rowCount][colCount] 각 셀 문자열
typedef struct {
    char* fileName;
    int    rowCount;
    int    colCount;
    char** header;
    int* id;
    char*** cell;
} CsvTable;

// <summary>
// path의 CSV를 읽어 전역 저장소에 로드
// 규칙: 첫 행=헤더, 첫 열=id
// </summary>
// <param name="path">파일 이름(상대 경로)</param>
// <returns>성공 시 1, 실패 시 0</returns>
int csvLoadAll(const char* path);

// <summary>
// 이미 메모리에 로드된 테이블에서 (id, headerName)으로 값을 조회
// </summary>
// <param name="path">파일 이름(상대 경로)</param>
// <param name="id">Id(기본키)</param>
// <param name="headerName">헤더 문자열</param>
// <param name="out">해당 하는 데이터</param>
// <param name="outSize">해당 하는 데이터 char 배열의 크기</param>
// <returns>성공 시 1, 실패 시 0</returns>
int csvGetValueInTable(const char* path,
    int id,
    const char* headerName,
    char* out, 
    size_t outSize);

// <summary>
// 파일 이름(상대 경로)에 해당하는 Id(기본키) 배열 반환
// </summary>
// <param name="path">파일 이름(상대 경로)</param>
// <param name="size">Id(기본키) 배열 사이즈</param>
// <returns>Id(기본키) 배열</returns>
int* getCsvIdArray(const char* path, size_t& size);