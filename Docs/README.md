# 기능별 문서

요청에 해당하는 문서만 읽습니다. 소유 파일·직접 호출부·회귀 검사는
[ChangeImpactMap](ChangeImpactMap.md), 공통 검증은 [Verification](Verification.md)에서 찾습니다.

| 작업 | 기준 문서 |
| --- | --- |
| 작업 위치·현재 제약·미구현 설계 | [SessionHandoff](SessionHandoff.md) |
| 시작·Scene 전환·종료·소유권 | [ExecutionFlow](ExecutionFlow.md) |
| 곡 선택·검색·정렬·배경·난이도 정보 | [SongSelect](SongSelect.md) |
| 목록 캐시·비동기 세션/자원 준비·포맷 변경 시 무효화 | [ResourceLoading](ResourceLoading.md) |
| 새 난이도·osu 가져오기 UI | [EditorSongSelect](EditorSongSelect.md) |
| 독립 변환기·격자·OD·BG·Client 검증 | [OsuTaikoConverter](OsuTaikoConverter.md) |
| 에디터 조작·메뉴·선택·편집 이력·PCM/FFT | [ChartEditor](ChartEditor.md) |
| YMM/YMP/YME·타이밍·진단 | [ChartFormats](ChartFormats.md) → 해당 형식 |
| 노트·정확도·POOR·Buzz 틱 | [Rules](Gameplay/Rules.md) |
| 배속·스크롤·Random·로컬 기록·조건식 | [PlayOptions](Gameplay/PlayOptions.md) |
| 노트·Lane·키빔·판정 표시·진행바 | [Presentation](Gameplay/Presentation.md) |
| Clip·사운드 정책·공통 PCM·DSP 예약 | [Audio](Gameplay/Audio.md) |
| ESC 메뉴·재개·되감기·Retire | [Pause](Gameplay/Pause.md) |
| 정확도 랭크·통계·그래프·입력 로그·Retry | [Results](Gameplay/Results.md) |
| Debug 정지·탐색·전체 노트/Buzz 재현 | [Debugging](Gameplay/Debugging.md) |
| 표시 이름·번역·글꼴·옵션·Taiko 키·Option.ini | [TextManagement](TextManagement.md) |
| 스킨·Songs/BG·내장 자산·배포 | [BuiltInAssets](BuiltInAssets.md) |
| 로고·타이틀 | [YOUMU](YOUMU.md) |
| Canvas·PNG·입력·Z-order | [Visual2DGuide](Visual2DGuide.md) |
| Penpot 페이지·보드·자산 제작 | [DesignReferences](DesignReferences.md) |
| HP 설계(미구현) | [YMP](Formats/Ymp.md#hp-게이지-설계-미구현) |
| 공용 미디어·랭크 무결성 제안(미구현) | [RankedContentProposal](RankedContentProposal.md) |
| 엔진 API/수명 자체 변경 | 엔진 [EngineOverview](../Dependencies/PhantomCore/Docs/EngineOverview.md) |

현재 계약은 기능 문서 한 곳에 유지합니다. 완료 내역·과거 진단·세션별 상태는 Git 이력으로
확인하며, 미구현 제안이나 Penpot 시안을 새 구현 지시로 해석하지 않습니다.
