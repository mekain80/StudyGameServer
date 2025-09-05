이름 : 최수영
개발환경: Visual Studio 2022 (v143), C++17

🎮 플레이 방법

 - 메인 메뉴
   - 게임 시작 : 'A'
   - 게임 종료 : 'ESC'

 - 플레이
   - 캐릭터 이동: 화살표 키
   - 공격: 'Z' 
   - 나가기: 'Q'
   - 플레이어는 데미지를 입은 직후 잠시 무적 판정이 있습니다.

 - 게임 오버/클리어
   - 나가기: 'Q'


📁 소스 파일 구성

 - config.h : 상수/설정 값 정의

 - entities.h : 게임 객체 구조체 정의 (Player, Enemy, Bullet 등)

 - csvLoader.h, csvLoader.cpp : CSV 외부 데이터 로더 (stage_info / stageN / enemy_info)

 - main.cpp : 메인 루프


📂 외부 파일

 - stage_info.csv
   - 스테이지 목록 및 정보 관리

 - stage1.csv, stage2.csv, stage3.csv
   - 각 스테이지의 적 배치 정보

 - enemy_info.csv
   - 적군 속성/능력치 정보