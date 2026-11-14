# 진행바 PNG 자산

경로: `YOUMU.Assets/Assets/Skins/Default Skin/InGame/Progress`.
네 요소는 투명 배경의 RGBA PNG이며 시간 문구는 런타임 텍스트로 표시합니다.

| 파일 | 크기 | 형태·색상 |
| --- | --- | --- |
| Background.png | 1920×54 | 화면 아래 끝 전체 폭의 옅은 띠 |
| Track.png | 1670×9 | 옅은 남색 레일 |
| Fill.png | 1670×9 | 코발트 채움(잘라 보임) |
| Marker.png | 21×21 | 흰 원형 마커 |

UI 시안에서 1080p 기준(논리 × 1.5)으로 뽑은 부품입니다.

교체 시 비율·투명 여백·요소 분리를 유지하고 문구·그림자·장식을 이미지에 합치지 않습니다.
색상은 제작 기준이며 모든 픽셀을 강제하는 값은 아닙니다.
배치·진행률·현재/전체 시간 계약은 [Presentation](Presentation.md#하단-플레이-진행바)을 따릅니다.
