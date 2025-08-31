// csvLoader.cpp
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "csvLoader.h"

/* ---- 설정 ---- */
#define LINE_BUF  4096
#define MAX_COLS  1024

CsvTable csvTables[CSV_TABLE_MAX];
char* csvTableNames[CSV_TABLE_MAX];
static int CSV_COUNT = 0;

/* ---- 내부 헬퍼(간단) ---- */
/* 문자열을 복제해서 새 메모리에 할당 */
static char* dupStr(const char* source) {
    size_t length = strlen(source);           // 문자열 길이
    char* copy = (char*)malloc(length + 1);   // 널 종료 포함해서 메모리 할당
    if (copy) {
        memcpy(copy, source, length + 1);     // 문자열 복사
    }
    return copy;
}

/* 문자열 끝의 개행(\r\n) 제거 */
static void rstripCrlf(char* str) {
    size_t length = strlen(str);

    if (length && str[length - 1] == '\n') {
        str[--length] = '\0';
    }
    if (length && str[length - 1] == '\r') {
        str[--length] = '\0';
    }
}

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

/* ---- 로드: 파일 전체를 메모리에 올림 ----
   path 의 CSV를 읽어 CsvTable *table 에 채운다.
   성공 1, 실패 0 */
int csvLoadAll(const char* path) {

    FILE* fp;
    errno_t err = fopen_s(&fp, path, "rb");
    if (err != 0) {
        perror("fopen_s");
        return 0;
    }

    char lineBuf[LINE_BUF];

    /* 1) 헤더 */
    if (!fgets(lineBuf, sizeof(lineBuf), fp)) { 
        fclose(fp); 
        return 0;
    }
    rstripCrlf(lineBuf);

    char* headerFields[MAX_COLS];
    int colCount = splitSimpleCsv(lineBuf, headerFields, MAX_COLS);

    CsvTable table;
    table.colCount = colCount;
    table.header = (char**)malloc(colCount * sizeof(char*));
    for (int colIndex = 0; colIndex < colCount; colIndex++)
        table.header[colIndex] = dupStr(headerFields[colIndex]);

    /* 2) 데이터 행 수 파악 */
    long afterHeaderPos = ftell(fp);
    int rowCount = 0;
    while (fgets(lineBuf, sizeof(lineBuf), fp)) rowCount++;
    table.rowCount = rowCount;

    /* 3) 메모리 할당 & 재파싱 */
    table.id = (int*)malloc(rowCount * sizeof(int));
    table.cell = (char***)malloc(rowCount * sizeof(char**));

    fseek(fp, afterHeaderPos, SEEK_SET);
    int rowIndex = 0;
    while (rowIndex < rowCount && fgets(lineBuf, sizeof(lineBuf), fp)) {
        rstripCrlf(lineBuf);

        char* rowFields[MAX_COLS];
        int fieldCount = splitSimpleCsv(lineBuf, rowFields, MAX_COLS);
        if (fieldCount < colCount) { /* 부족하면 빈칸 보충 */
            for (int k = fieldCount; k < colCount; k++) rowFields[k] = (char*)"";
            fieldCount = colCount;
        }

        table.cell[rowIndex] = (char**)malloc(colCount * sizeof(char*));
        for (int colIndex = 0; colIndex < colCount; colIndex++) {
            table.cell[rowIndex][colIndex] = dupStr(rowFields[colIndex]);
        }

        /* 첫 컬럼을 id로 */
        {
            unsigned char* u = (unsigned char*)table.cell[rowIndex][0];
            table.id[rowIndex] = atoi(table.cell[rowIndex][0]);
        }
        rowIndex++;
    }


    size_t len = strlen(path);
    char* buffer = (char*)malloc(len + 1);
    if (buffer) {
        strcpy_s(buffer, len + 1, path);
    }
    csvTableNames[CSV_COUNT] = buffer;
    csvTables[CSV_COUNT] = table;
    ++CSV_COUNT;

    fclose(fp);
    return 1;
}

/* ---- 조회: (메모리에 이미 로드된 테이블에서) id + header명으로 값 얻기 ----
   성공 1(문자열 out으로 복사), 실패 0 */
int csvGetValueInTable(const char* path, int wantedId,
    const char* headerName,
    char* out, size_t outSize)
{
    if (!path || !headerName || !out || outSize == 0) return 0;

    // 1) 테이블 찾기
    const CsvTable* t = NULL;
    for (size_t i = 0; i < CSV_COUNT; ++i) {
        if (strcmp(path, csvTableNames[i]) == 0) {  // 같으면 0
            t = &csvTables[i];                      // 포인터로 직접 참조
            break;
        }
    }
    if (!t) return 0;  // 못 찾음

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
        if (t->id[rowIndex] == wantedId) {
            const char* src = t->cell[rowIndex][targetCol];
            if (!src) src = "";
            // 목적지 크기를 outSize로 넘기고, 자동 널종료에 맡김
            strncpy_s(out, outSize, src, _TRUNCATE);
            return 1;
        }
    }
    return 0;
}

/* ---- 해제: 로드한 테이블의 동적 메모리 해제 ---- */
void csvFreeTable(CsvTable* table) {
    if (!table) return;
    if (table->header) {
        for (int colIndex = 0; colIndex < table->colCount; colIndex++)
            free(table->header[colIndex]);
        free(table->header);
    }
    if (table->cell) {
        for (int rowIndex = 0; rowIndex < table->rowCount; rowIndex++) {
            if (!table->cell[rowIndex]) continue;
            for (int colIndex = 0; colIndex < table->colCount; colIndex++)
                free(table->cell[rowIndex][colIndex]);
            free(table->cell[rowIndex]);
        }
        free(table->cell);
    }
    free(table->id);
}


/* ---- 예시 ----
#define DEMO
#ifdef DEMO
int main(void){
    CsvTable table;
    if (!csvLoadAll("stages.csv", &table)) {
        printf("load fail\n");
        return 1;
    }

    char buf[256];
    if (csvGetValueInTable(&table, 2, "stage_name", buf, sizeof(buf))) {
        printf("id=2 -> %s\n", buf);  // 기대: stage2.csv
    } else {
        printf("not found\n");
    }

    csvFreeTable(&table);
    return 0;
}
#endif
*/
