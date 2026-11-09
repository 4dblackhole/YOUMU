# 작업 인수인계

현재 구현 상태는 Git, 유지할 설계는 [설계 기준](Design.md)과
[설계 결정](DesignDecisions.md) 및 기능 문서가 기준입니다. 완료 기록은 Git 이력을 사용하며,
이 문서에는 다음 작업에 필요한 위치·제약만 유지합니다.

## 작업 위치와 시작

- Client: `D:\Projects\c++\asdf\YOUMU`, 솔루션 `YOUMU.sln`.
- 독립 엔진: `../PhantomCore`, Client 고정본: `Dependencies/PhantomCore`.
- 변환기: `../OsuTaikoConverter`. 구현은 기존 전용 채팅에서 담당하고
  실제 변환 결과의 플레이 가능 여부는 Client가 검증합니다.
- 루트 [AGENTS.md](../AGENTS.md) → [설계 기준](Design.md) →
  [관련 결정 ID](DesignDecisions.md) → [기능 문서](README.md) →
  [소유 코드·테스트](ChangeImpactMap.md) 순서로 필요한 부분만 읽습니다.
- 작업 전 세 저장소의 필요한 status/upstream과 엔진 gitlink를 확인합니다.
  과거 세션의 미커밋 목록이나 특정 커밋을 현재 상태로 가정하지 않습니다.

## 현재 계약과 미구현 설계

- 지원 모드는 Taiko, 편집 형식은 YMP/YME입니다. 플레이 옵션과 로컬 기록은
  [PlayOptions](Gameplay/PlayOptions.md), Debug 전용 정지·탐색은
  [Debugging](Gameplay/Debugging.md)이 기준입니다.
- 기본 HP 게이지와 YMP `HP:` 필드는 구현되어 있습니다. 기본값·가중치·빈 채보·
  세션 수명과 되감기 계약은 [YMP의 HP](Formats/Ymp.md#hp-게이지)를 따릅니다.
  실제 HP 곡선·사용 요약은 구현돼 있습니다. 일시정지 메뉴/Rewind 이후 HP는 비활성화하며
  이전 곡선은 보존합니다. 추가 게이지·등급 순서는 미정입니다.
- [랭크 콘텐츠 제안](RankedContentProposal.md)은 미구현 제안입니다.
  SCORE는 정확도 × 100,000,000 × 처리한 노트 비율이며 종료 점수를 리절트에 전달합니다.
  리플레이 재생은 미구현입니다. 실제 제한은
  [Results](Gameplay/Results.md)를 따릅니다. 제안은 추가 구현 지시가 아닙니다.

## 데이터 보존과 마무리

- 사용자 Songs/음원/스킨, `Option.ini`, 실제 기록 DB를 보존합니다.
  검증용 데이터·캡처·로그는 ignored `build/` 또는 테스트 데이터 경로에 둡니다.
- 목록 캐시는 재생성 가능하지만 기록 DB는 사용자 데이터입니다.
  포맷 변경 시 [캐시 무효화](ResourceLoading.md)를 함께 확인합니다.
- [Verification](Verification.md)에 따라 변경 종류에 맞게 검사하고 동기화합니다.
  정확한 검증 범위·한계와 최종 커밋을 보고합니다.
