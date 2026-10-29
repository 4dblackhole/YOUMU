# 목록·패턴·자원 로딩

목록용 메타데이터와 실제 플레이 세션을 분리합니다. 목록 표시를 위해 모든 노트를
상주시킬 필요가 없으며, 플레이 직전에는 선택한 원본을 다시 검증합니다.

## 소유권과 스레드

| 소유자 | 책임 |
| --- | --- |
| `SongCatalogLoader` | 단일 worker에서 파일 발견·변경 문서 파싱, 최신 Songs 루트 결과만 게시 |
| `SongCatalog::MetadataIndex` | 목록용 메타데이터·노트 수·유효 BPM 범위·진단; 일반 `Load`는 전체 문서 API |
| `SongCatalogIndex` | 버전·루트·체크섬을 검증하는 재생성 가능한 디스크 캐시 |
| `PlaySessionLoader` | 선택 YMP/YME 파싱·검증·모드 세션 생성; 완료 future만 Poll로 이동 |
| `MusicSelectScene` | 선택/루트/Scene 변경 시 오래된 진입 결과 제외, 성공 세션을 launch store로 전달 |
| `GameplaySessionController` | 검증 세션 재사용, 직접 진입/Retry의 비동기 로딩, 자원 준비 후 clock 시작 |
| `RhythmTestScene` | 오디오/이미지 준비 병행, 실제 Gameplay 전 플레이 Canvas 노출 방지 |

worker는 경로와 데이터를 소유하며 Scene·노드·엔진 observer를 참조하지 않습니다.
GPU 등록과 Canvas 조립은 Scene 스레드에서 수행합니다. 메인 Update는 파일 읽기 완료를
기다리지 않습니다. 게시 취소 후에도 순수 데이터 작업은 완료될 수 있으며 future 정리와
프로그램 종료는 완료를 기다릴 수 있습니다.

검색에서 얻은 크기/수정 시각과 문서별 정렬 키를 재사용합니다. 캐시를 읽은 뒤에도
추가/삭제 및 음원·YME 의존성을 확인합니다. 화면 인근 카드만 만들고 선택한 패턴만
플레이 세션으로 구성합니다. 오디오 준비 실패는 오류/Pause/Retry 경로로 전달합니다.
첫 입력음이 준비 전에 버려지지 않도록 샘플·음악·이미지가 준비되고 공통 QPC/DSP
시작 시점에 도달한 뒤 플레이 화면을 표시합니다. [오디오 계약](Gameplay/Audio.md)을 참고합니다.

## 캐시와 포맷 변경

캐시: `%LOCALAPPDATA%/YOUMU/SongCatalog/<곡 루트 해시>.bin`.
원본 YMM/YMP/YME와 독립적이며 손상·쓰기 불가·버전 불일치는 원본 로딩으로 복구합니다.
`YOUMU.Chart/Catalog/SongCatalogCacheVersion.h`의 두 값을 구분합니다.

- `SongCatalogParserRevision`: 문법·기본값·진단·노트 수·목록 해석 변경 시 증가.
  허용 문법 추가도 이전 실패 진단이 캐시에 남을 수 있으므로 포함합니다.
- `SongCatalogCacheSchema`: 캐시 필드 또는 바이너리 배치 변경 시 증가.

불일치 캐시는 migration 없이 재구축합니다. 원본 `Version:`은 별도 계약입니다.
수정 시각/크기가 바뀐 문서와 YMM/YME 의존성만 다시 확인하며, 플레이는 항상 원본으로
검증합니다. 수정 시각과 크기를 모두 강제로 보존한 외부 수정은 목록 갱신에서 누락될 수 있습니다.
포맷 수정에는 parser revision 증가와 캐시 재구축 회귀를 포함합니다.

로컬 기록 DB는 재생성 캐시가 아니므로 삭제/재구축하지 않습니다.
기록 스키마와 복구 정책은 [PlayOptions](Gameplay/PlayOptions.md#로컬-기록)가 기준입니다.

## 참고와 검사

목록 메타데이터 저장과 플레이 데이터 지연 로딩을 분리하는 접근은 osu!의
[stable DB 설명](https://github.com/ppy/osu/wiki/Legacy-database-file-structure#osudb),
lazer의 [BeatmapInfo](https://github.com/ppy/osu/blob/master/osu.Game/Beatmaps/BeatmapInfo.cs)와
[WorkingBeatmap](https://github.com/ppy/osu/blob/master/osu.Game/Beatmaps/WorkingBeatmap.cs)을 참고했습니다.
YOUMU는 자체 파생 캐시를 사용합니다. 최초 생성/파서 revision 변경에는 전체 검증 비용이 들고,
현재 오디오의 전체 PCM 준비·최초 GPU 등록/Canvas 조립 비용도 남습니다.

회귀는 캐시 재사용·원본/의존성 변경·삭제·버전/체크섬 손상·쓰기 불가,
선택 변경 중 세션 게시, 첫 화면의 자원 준비와 최초 Don/Kat 출력을 검사합니다.
실행기는 [변경 영향 지도](ChangeImpactMap.md)와 [Presentation tests](../Tests/Presentation/README.md)가 기준입니다.
성능 수치는 데이터 규모·하드웨어·OS 파일 캐시 조건을 함께 기록하고 실행 시간 보장으로 사용하지 않습니다.
