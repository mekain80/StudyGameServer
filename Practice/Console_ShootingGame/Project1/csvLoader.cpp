#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "csvLoader.h"

static const int kLineBuf = 4096;   // 한 줄 최대 길이
static const int kMaxCols = 1024;   // 최대 컬럼 수

CsvTable csvTables[kCsvTableMax];
static int g_csvCount = 0;          // 현재 로드된 테이블 개수

// 문자열을 복제해서 새 메모리에 할당 후 반환
static char* dupStr(const char* source) {
    size_t length = strlen(source);
    char* copy = (char*)malloc(length + 1);
    if (copy) {
        memcpy(copy, source, length + 1);
    }
    return copy;
}

// 문자열 끝의 개행(\r\n) 제거
static void rstripCrlf(char* str) {
    size_t length = strlen(str);

    if (length && str[length - 1] == '\n') {
        str[--length] = '\0';
    }
    if (length && str[length - 1] == '\r') {
        str[--length] = '\0';
    }
}

// 콤마(,) 기준 단순 분리: 따옴표/이스케이프 미지원(간단 CSV 전용)
// lineBuf를 제자리에서 자르며, fields에 각 토큰 시작 포인터를 채운다.
static int splitSimpleCsv(char* lineBuf, char** fields, int maxFields) {
    int fieldCount = 0;
    char* current = lineBuf;

    for (;;) {
        if (fieldCount >= maxFields) break;

        /* 현재 필드 시작 위치 저장 */
        fields[fieldCount++] = current;

        /* 다음 콤마 위치 찾기 */
        char* commaPos = strchr(current, ',');
        if (!commaPos) break;

        /* 콤마 → 문자열 끝으로 바꾸고, 다음 글자부터 다음 필드 시작 */
        *commaPos = '\0';
        current = commaPos + 1;
    }
    return fieldCount;
}

// 경로로 테이블 인덱스 찾기(없으면 -1)
static int findTableIndex(const char* path) {
    if (!path) 
        return -1;
    for (int i = 0; i < g_csvCount; ++i) {
        if (csvTables[i].fileName && strcmp(csvTables[i].fileName, path) == 0)
            return i;
    }
    return -1;
}

// 파일 전체 로드(이미 로드된 파일이면 재로딩하지 않고 성공 반환)
// 성공 1, 실패 0
int csvLoadAll(const char* path) {
    if (!path || !*path) return 0;
    if (findTableIndex(path) >= 0) return 1;

    // 저장소 꽉 찼는지 확인
    if (g_csvCount >= kCsvTableMax) {
        perror("csvLoadAll");
        return 0;
    }

    FILE* fp = nullptr;
    errno_t err = fopen_s(&fp, path, "rb");
    if (err != 0) {
        perror("fopen_s");
        return 0;
    }

    char lineBuf[kLineBuf];

    // 1) 헤더를 headerFields에 저장
    if (!fgets(lineBuf, sizeof(lineBuf), fp)) { 
        fclose(fp); 
        return 0;
    }
    rstripCrlf(lineBuf);

    char* headerFields[kMaxCols];
    int colCount = splitSimpleCsv(lineBuf, headerFields, kMaxCols);

    CsvTable table;
    table.colCount = colCount;
    table.header = (char**)malloc(colCount * sizeof(char*));
    for (int colIndex = 0; colIndex < colCount; colIndex++) {
        table.header[colIndex] = dupStr(headerFields[colIndex]);
    }

    // 2) 데이터 행 수 파악
    long afterHeaderPos = ftell(fp);
    int rowCount = 0;
    while (fgets(lineBuf, sizeof(lineBuf), fp)) rowCount++;
    table.rowCount = rowCount;

    // 3) 테이블 구조 + 메모리 할당
    table.id = (int*)malloc(rowCount * sizeof(int));
    table.cell = (char***)malloc(rowCount * sizeof(char**));

    // 헤더를 제외한 파일의 처음으로 이동
    fseek(fp, afterHeaderPos, SEEK_SET);
    int rowIndex = 0;
    while (rowIndex < rowCount && fgets(lineBuf, sizeof(lineBuf), fp)) {
        rstripCrlf(lineBuf);

        char* rowFields[kMaxCols];
        splitSimpleCsv(lineBuf, rowFields, kMaxCols);
        table.cell[rowIndex] = (char**)malloc(colCount * sizeof(char*));
        for (int colIndex = 0; colIndex < colCount; colIndex++) {
            table.cell[rowIndex][colIndex] = dupStr(rowFields[colIndex]);
        }

        // 첫 컬럼을 id로 설정
        unsigned char* u = (unsigned char*)table.cell[rowIndex][0];
        table.id[rowIndex] = atoi(table.cell[rowIndex][0]);

        rowIndex++;
    }

    // csv 파일 이름 설정
    size_t len = strlen(path);
    char* buffer = (char*)malloc(len + 1);
    if (buffer) {
        strcpy_s(buffer, len + 1, path);
    }
    table.fileName = buffer;
    csvTables[g_csvCount] = table;
    ++g_csvCount;

    fclose(fp);
    return 1;
}

// csv 테이블에서 원하는 값 찾기(id, header)
// 성공 1, 실패 0
int csvGetValueInTable(const char* path, int id, const char* headerName, char* out, size_t outSize)
{
    if (!path || !headerName || !out || outSize == 0) 
        return 0;

    int idx = findTableIndex(path);
    // 아직 csv가 로드 안 되어 있으면 로드 시도
    if (idx < 0) {
        if (!csvLoadAll(path)) 
            return 0;
        idx = findTableIndex(path);
        if (idx < 0) 
            return 0;
    }
    const CsvTable* t = &csvTables[idx];

    // 2) 열 인덱스 찾기
    int targetCol = -1;
    for (int colIndex = 0; colIndex < t->colCount; ++colIndex) {
        if (strcmp(t->header[colIndex], headerName) == 0) {
            targetCol = colIndex;
            break;
        }
    }
    if (targetCol < 0) return 0;

    // 3) 행(원하는 id) 찾기
    for (int rowIndex = 0; rowIndex < t->rowCount; ++rowIndex) {
        if (t->id[rowIndex] == id) {
            const char* src = t->cell[rowIndex][targetCol];
            if (!src) src = "";
            // 목적지 크기를 outSize로 넘기고, 자동 널종료에 맡김
            strncpy_s(out, outSize, src, _TRUNCATE);
            return 1;
        }
    }
    return 0;
}

// csv 파일의 Id 배열 반환
int* getCsvIdArray(const char* path, size_t &size) {
    for (size_t i = 0; i < g_csvCount; i++)
    {
        if (strcmp(csvTables[i].fileName, path) == 0) {
            size = csvTables[i].rowCount;
            return csvTables[i].id;
        }
    }
}