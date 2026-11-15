# Render prompts matching the final spec

Paste one prompt at a time into ChatGPT (or any image model). Each is written to match docs/electronics-spec.md and docs/designer-handoff.md rather than the original concept renders. Ask for a few variations of each and keep the ones that respect the "not included" list.

## Prompt 1: front view, lights on, at the keyboard

```
Photorealistic concert photo, dark stage, a male keyboard player standing behind a single Korg Kronos workstation keyboard, playing with both hands, seen from the front at audience eye level.

He wears a floor-length open-front black stage cape in matte stretch velvet over a plain black shirt and black trousers. The cape hangs from the shoulders and a tall stiff stand-up collar. Thin metallic gold piping along the edges.

Lighting built into the cape, all behind a fine black mesh so the light looks soft and slightly diffused:
- Collar: a single thin continuous glowing line of light, like neon, outlining the tall collar: up each straight front edge and along the entire top rim around the back of the neck. Cyan-blue. The face of the collar is black; only the edge line glows.
- Lapels: on each lapel facing, a vertical glowing bar about 3 cm wide and 80 cm long, made of two parallel rows of small LED dots about 1.7 cm apart, softened by the mesh. The bars show an equalizer-style pattern rising from the bottom, blue at the base fading to magenta at the top.
- Shoulders: a short 20 cm lit segment on each soft, slightly curved shoulder piece, same two-row dotted look, magenta.
- Lower front edges: a single row of small round glowing dots, about 10 cm apart, running from the waist down along each front edge, stopping 15 cm above the hem, blue and violet, slightly sparkling.
- Outer side seams: a single row of about 12 small glowing dots, 10 cm apart, along the outer seam of each swinging side panel, starting 15 cm above the hem and climbing toward the waist.

LEDs at moderate brightness, not blinding; individual dots are visible through the mesh on the lapels and edges. The bottom 15 cm of the cape, all the way around, is plain dark velvet with no lights. Colors limited to cyan, blue, magenta and violet. Stage haze, a single warm backlight, shallow depth of field, 35 mm lens, realistic fabric texture and folds.

Do not include: LED grid or matrix panels, glowing bracers or gauntlets on the wrists or forearms, lights on the arms, lit circuit patterns or traces on the back or side panels, any lights along or near the hem, dots scattered across the panels, hard armored epaulets, visible wires or batteries, any text or logos, a second keyboard.
```

## Prompt 2: three-quarter side view, lights on, mid-performance

```
Photorealistic concert photo, dark stage, a male keyboard player behind a single Korg Kronos keyboard, seen from the front-left at a three-quarter angle, cape swinging slightly as he moves.

Floor-length open-front black velvet stage cape over a plain black shirt, tall stiff collar, thin gold piping. All lights are behind black mesh and look softly diffused:
- One thin continuous neon-like cyan line outlining the collar: up each straight front edge and along the whole top rim around the neck. The collar face is black.
- A vertical 3 cm wide bar of two rows of small LED dots down each lapel facing, about 80 cm long, blue rising to magenta in an equalizer pattern.
- A short lit segment on each soft curved shoulder piece, magenta.
- A single row of small glowing dots about 10 cm apart down each lower front edge, stopping 15 cm above the hem.
- A single row of about 12 glowing dots, 10 cm apart, along the outer side seam of the panel nearest the camera, from 15 cm above the hem up toward the waist, catching the movement of the fabric.
- The back of the cape is plain black with no lights. The bottom 15 cm of the cape is unlit all the way around.

Moderate LED brightness, cyan, blue, magenta and violet only, stage haze, warm rim light from behind, 50 mm lens, real fabric weight and drape.

Do not include: LED matrix panels, glowing wrist or forearm pieces, lit circuit traces on the back or sides, any lights at or near the hem, dots scattered across the panels, rigid armored epaulets, visible wires, batteries or electronics, text, a second keyboard.
```

## Prompt 3: lights off, for the designer

```
Photorealistic studio photo on a plain dark grey backdrop, full length, front view, of a floor-length open-front black stage cape in matte stretch velvet on a male model standing still, plain black shirt and trousers underneath, arms relaxed.

Tall stiff stand-up collar with straight front edges. Soft, slightly curved shoulder pieces that follow the shoulder line without armor. Lapel facings about 6 cm wide running from the collar to below the waist. Thin metallic gold piping along the collar, lapels and front edges. Along the collar's front edges and top rim, the lapels, the shoulders, the lower front edges and the outer side seams there are subtle matte black mesh strips, barely distinguishable from the velvet; nothing is lit, and the bottom 15 cm of the cape is plain velvet. The garment reads as a strong, elegant plain black cape with a dramatic collar.

Even soft lighting, 50 mm lens, true fabric texture.

Do not include: any glowing lights, LEDs, visible electronics, wires, armor, gauntlets, circuit patterns, text or logos.
```

## Prompt 3b: lights off, zone map version for the designer

Use after prompt 3 produced a good silhouette. This one makes the lit zones visible as mesh and fixes the facing width and hem.

```
Photorealistic studio photo on a plain dark grey backdrop, full length, front view, of a floor-length open-front black stage cape in matte stretch velvet on a male model standing still, plain black shirt and trousers underneath, arms relaxed. The hem clears the floor by about 4 cm; it does not touch the ground.

Tall stiff stand-up collar, about 9 cm high, with straight front edges. Soft, slightly padded shoulder pieces that follow the shoulder line, no armor. Each front edge has a flat lapel facing clearly 6 cm wide, running from the collar down to below the waist. Thin metallic gold piping along the outer edge of the collar, the lapel facings and the front edges.

Panels of fine matte black mesh, slightly lighter than the velvet so they are just visible as a charcoal texture: a 14 mm wide mesh strip just inside the gold piping along both front edges of the collar and all the way around its top rim; a single continuous 30 mm wide mesh strip down the centre of each 6 cm front facing, running from the collar all the way down to 15 cm above the hem; a 14 mm mesh strip, 20 cm long, on each shoulder piece; and a 20 mm mesh strip along each outer side seam from 15 cm above the hem up toward the waist. Nothing is lit. The bottom 15 cm of the cape is plain velvet all the way around.

Even soft lighting, 50 mm lens, true fabric texture.

Do not include: any glowing lights, LEDs, visible electronics, wires, armor, gauntlets, circuit patterns, text or logos, a hem touching the floor, a narrow piped edge in place of the 6 cm facing.
```

## Tips

- Image models love to over-light. If a result shows bright panels, add "the LEDs are small individual dots at low brightness, most of the cape is dark velvet" and regenerate.
- If the collar comes out as a lit panel rather than a line, add "the collar light is a single 1 cm wide line only along the edge, the collar face itself is black."
- If the lapel bars come out solid, add "the lapel bars are clearly made of two rows of separate small dots."
- Ask for a crop of the collar and upper lapel at close range; that is the view the designer needs most.
- Keep the three "do not include" lists; they carry most of the work.
