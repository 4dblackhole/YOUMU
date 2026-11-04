# HUD 숫자 이미지

제작본: [HudDigits.png](Assets/HudDigits.png).

흰색 채움·짙은 남색 윤곽의 0–9 숫자 시트입니다. 상단 행은 0–4, 하단 행은 5–9이며
배경과 숫자 내부 공간은 투명합니다. 이 파일은 새 HUD용 디자인 원본이며 현재 런타임에
연결하지 않았습니다. 기존 NumberImage 자산은 유지합니다. 게임 적용은 사용자 요청에 따라
구현 전 브리핑과 답변 후 진행합니다. 콤보·점수 규칙과 화면 배치는 아직 확정하지 않았습니다.

제작 도구는 기본 image_gen입니다. 첫 생성에서 보인 옅은 구분선을 편집 호출로 제거했습니다.

## 최초 생성 프롬프트

```text
Use case: ui-mockup. Asset type: one production-ready transparent numeric sprite atlas for the HUD of an existing rhythm game. Create exactly ten glyphs: 0 1 2 3 4 in the top row, 5 6 7 8 9 in the bottom row. Five equal-width columns and two equal-height rows, each glyph centered in its own equally spaced cell with generous transparent padding; no glyph crosses a cell. Typography: upright, bold, slightly condensed geometric sans-serif, clean simple shapes, high readability at a small game HUD size. Pure white flat fill with a crisp solid very dark navy outline of consistent thickness. Every digit has identical cap height, baseline, weight and overall optical size. The zero is a plain unslashed zero. Keep generous open counters in 0,6,8,9. Only the numeric glyphs, isolated on genuine transparent alpha. No extra text, no labels, no grid lines, no panel, no shadows, no bevel, no metallic treatment, no glow, no decorative marks, no perspective. Aim for a wide rectangular 5:2 atlas, 1600 by 640 pixels if possible, each cell 320 by 320 pixels. Exact digit sequence and consistency matter most. The result must be an actual transparent PNG suitable for importing into a game, not a presentation mockup.
```

## 정리 프롬프트

```text
Edit the attached numeric sprite atlas. Preserve exactly the ten digits and their order, their white fill, dark navy outline, clean upright bold type, equal cap heights and two-row five-column arrangement. Remove ALL faint vertical or horizontal grid lines, separator lines, edge speckles and table artifacts so the ONLY nontransparent pixels belong to the outlines and fills of the ten digits. Increase the fully transparent padding around each glyph, including between rows and at the outer image edges; make the outer padding at least 10 percent of a cell. Ensure true transparent background, transparent spaces and counters. Do not add labels, cells, rules, frames, shadows, glow, texture, new marks or decorative elements. This is a clean usable game sprite atlas consisting solely of 0 1 2 3 4 / 5 6 7 8 9 floating on clear alpha, not a drawn grid.
```

