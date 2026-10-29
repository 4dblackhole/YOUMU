# 판정 PNG 자산

경로: `YOUMU.Assets/Assets/Skins/Default Skin/InGame/Judgements`.
각 PNG는 1254×1254 RGBA이며 중심과 바깥 여백이 투명합니다.
동일한 원형 링의 중심·반지름·선 두께와 위쪽 문구 위치를 공유합니다.

| 파일 | 색상 |
| --- | --- |
| MAX.png | 흰색 |
| PERFECT.png | 밝은 청록색(cyan) |
| GREAT.png, OK.png | 초록 |
| GOOD.png | 노랑 |
| BAD.png | 보라 |
| MISS.png | 회색 |
| POOR.png | 빨강 |

교체 시 정확한 대문자 문구, 짙은 남색 글자 윤곽과 투명 알파를 유지합니다.
런타임은 이미지 크기를 읽어 판정원에 비율을 맞춥니다.
표시 조건·크기·수명은 [Presentation](Presentation.md#판정-인디케이터와-링문구)이 기준입니다.
