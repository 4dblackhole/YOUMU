# YOUMU

비공개 리듬게임 **Y's Orchestral Unison: Melodic Ultimatum**의 Client와 게임 자산입니다.
재사용 엔진은 `Dependencies/PhantomCore` submodule입니다. 표시 이름과 EXE 이름은
[GameIdentity.props](Client/Texts/GameIdentity.props)의 `GameDisplayName`에서 생성합니다.

## 시작과 빌드

작업은 [AGENTS.md](AGENTS.md), [인수인계](Docs/SessionHandoff.md),
[기능별 문서](Docs/README.md)에서 시작합니다. 필요한 코드·회귀 검사는
[변경 영향 지도](Docs/ChangeImpactMap.md)에서 찾습니다.

Visual Studio 2022/MSVC v143, Windows SDK, FMOD Studio API for Windows가 필요합니다.
전체 솔루션은 형제 폴더의 비공개 변환기 저장소도 참조합니다.

```powershell
git clone --recurse-submodules https://github.com/4dblackhole/YOUMU.git
cd YOUMU
git clone https://github.com/4dblackhole/OsuTaikoConverter.git ../OsuTaikoConverter
msbuild YOUMU.sln /m /p:Configuration=Debug /p:Platform=x64
```

FMOD 경로는 `FMOD_ROOT` 또는 MSBuild의 `FmodRoot`로 지정합니다.
기본 설치 경로는 `Directory.Build.props`가 찾습니다. 생성된 게임 EXE는
`bin/x64/<Configuration>/Y's Orchestral Unison - Melodic Ultimatum.exe`입니다.
Release/분석/도메인 CI·테스트 명령은 [Verification](Docs/Verification.md)에 있습니다.

## 코드와 기능

| 위치 | 책임 |
| --- | --- |
| `Client/` | 실행, Scene 조립, UI·오디오·전역 설정·로컬 기록 |
| `YOUMU.Rhythm/` | 판정·노트 규칙·Lane·스크롤·공통 타이머 |
| `YOUMU.Chart/` | YMM/YMP/YME, 타이밍, 곡 카탈로그·캐시, 문서 편집·저장 |
| `YOUMU.Modes/` | PlaySession, Taiko 노트 구성·사운드·결과 정책 |
| `YOUMU.Editor/` | 주입받은 PCM의 파형·스펙트럼 분석 |
| `YOUMU.Assets/` | 내장 기본 스킨·글꼴·기본 제공곡 |
| `Dependencies/PhantomCore/` | Win32·D3D12·Raw Input·오디오 엔진 |
| `../OsuTaikoConverter/` | 파일 형식과 Taiko 노트 계약을 사용하는 독립 변환기 |

- 로고에서 플레이/에디터 곡 선택으로 진입합니다. [곡 선택](Docs/SongSelect.md)은
  실제 메타데이터·배경·JudgeLevel/BPM 범위/길이를 표시하고 목록/세션을 비동기 준비합니다.
- Taiko의 [플레이 옵션과 영속 기록](Docs/Gameplay/PlayOptions.md)은 배속·스크롤·
  Mirror/Random·JudgeLevel·메트로놈·개별 음정 유지와 기록 조건식 조회를 지원합니다.
- [ESC 메뉴](Docs/Gameplay/Pause.md)는 Resume/Rewind/Retry/Retire를 제공합니다.
  노트는 입력 없이도 [GOOD 우선 포커스 규칙](Docs/Gameplay/Rules.md#lane과-배드말림-방지)으로 갱신합니다.
  [Debug 조작](Docs/Gameplay/Debugging.md)은 타이머 표시, Space 정지 중 입력과 1~4 시간 탐색입니다.
- [에디터](Docs/ChartEditor.md)는 노트/타이밍/효과 편집, 선택·복사·붙여넣기,
  32단계 Undo/Redo, 고해상도 파형·비동기 스펙트로그램과 미리듣기를 제공합니다.
  [새 난이도·osu 가져오기](Docs/EditorSongSelect.md)도 곡 선택에서 시작합니다.
- [결과](Docs/Gameplay/Results.md)는 정확도·랭크·판정 통계·오차 그래프·옵션을 표시합니다.
  HP는 설계만 있으며 SCORE 계산·리플레이 재생은 미구현입니다.

## 설정과 자산

전역 설정은 `Option.ini`, UI 번역은 UTF-8 언어 INI를 사용합니다.
오디오 장치·해상도·Taiko 키 설정은 [TextManagement](Docs/TextManagement.md),
음악 재생과 PCM 시간축은 [Audio](Docs/Gameplay/Audio.md)를 참고합니다.
개발 자산은 `Client/Assets`, 배포 자산은 EXE 옆 `Asset`을 사용하며 Songs 루트는
옵션에서 변경할 수 있습니다. 스킨·기본본 대체·라이선스·배포 절차는
[BuiltInAssets](Docs/BuiltInAssets.md), 파일 문법은 [ChartFormats](Docs/ChartFormats.md)가 기준입니다.
