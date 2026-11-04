The default song-selection UI uses procedural panels and buttons.
Set the optional image keys in ../skin.ini to replace their backgrounds.
Text, layout, input and song data remain runtime UI elements.

PanelFill.png is an internal opaque white texel for the three main panels' 90%
fills. It selects the renderer's existing alpha-blended image material; it is
not a song image or a configurable skin key. Custom panel PNGs still replace it.
