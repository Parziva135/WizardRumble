# Wizard Rumble 0.9.2 visual design

0.9.2 gives Expecto Patronum a **filled silver-blue Octane body** beneath its soft light, making it substantial rather than a collection of misty surface points. Dark glazing, separate tires, silver hubs and the raised rear wing preserve a readable silhouette. Its mist and wake are reduced. The existing 0.9.1 soft additive glow remains on Avada and the other spells. Accio now gathers icy-blue caster light during its new 0.5-second charge; Imperio gathers lavender light during its new one-second charge. Their full new voice clips begin with the charge. Other gameplay lengths, ranges, targeting and balance remain unchanged.

## Spell treatments

| Spell | Visual treatment |
| --- | --- |
| Accio | A 0.5-second icy-blue caster charge precedes release. Converging cyan and pearl ribbons connect caster and ball. Small inward light packets establish the pull direction; open crescents pick out the ball without enclosing it in a solid shell. The tether ends with the host's guidance, at most three active seconds. |
| Avada Kedavra | The locked car alone gains clean green soft light during its two-second charge. Release overlaps pale-hot cores, soft emerald envelopes and irregular mist around the branching path, with a luminous green contact burst at the captured hit position. The visible energy has area instead of relying on thin outlines. A brief local upper-view tint supports the impact. |
| Wingardium Leviosa | Violet and silver updraft ribbons taper around the actual rising car, with soft violet light accents and feather-shaped glints moving upward. The sustained effect follows the recorded body pose through ascent and roof release. |
| Bombarda Maxima | Orange pressure light and pale hot cores accompany the release. Soft amber light on the ball and at impact complements broken shock arcs, two-tone ejecta and curved ember detail during its ricochet effect. |
| Protego | A thin circular mint rim, gold calligraphic runes, broken arc detail and travelling highlights define the barrier. Impact energy stays at the edge. Its centre remains open so real scenery and the ball remain visible through it. |
| Stupefy | A red/white strike, soft scarlet light along the path, a hot impact envelope and short outward fragments distinguish the stun from the green killing curse. The release appears after its existing 0.65-second charge. |
| Petrificus Totalus | The impact supplies the binding flash; the sustained hold remains a thin light-blue car contour. Small white-blue highlights move along that contour, without a filled ice block or opaque box around the car. |
| Obliviate | A bright yellow laser has a pale core, a soft golden envelope and contact light. Lighter memory-like fragments identify the lingering status without turning the five-second inversion into a continuous attack beam. |
| Eat Slugs | Dark-green globs have irregular shaded skins, wet highlights and trailing mucus. They launch only after the existing 1.5-second charge. The victim's camera slugs have a curved foot, mantle, skin detail and eye stalks, then stretch narrow wet trails as they slide away. |
| Expecto Patronum | A filled, shaded silver-blue shell follows the supplied model's body profiles and wheel bounds. Dark windows, four solid tires, pale hubs and a separate rear wing give the miniature more mass. The original 176 surface samples and selective source-model contours add restrained soft light and detail; a short faint wake and separate contact flash keep its magical appearance. Flight speed, size, three-second charge and single hit stay unchanged. |
| Imperio | A one-second lavender caster charge precedes release. A purple/gold control link has smoother strands and light packets travelling from caster to victim. Compact marks at the linked bodies clarify the relationship while leaving both cars readable. |

These shapes and colours are mod design choices. The Octane Patronus, ball barrier, camera obstruction and control links are adaptations for this mode rather than claims that the films use those exact graphics or gameplay.

## Texture resources and loading

The sixteen runtime files are the DLL, settings, twelve WAVs and **two 128Ã—128 RGBA PNGs**. Install `glow_soft.png` and `glow_mist.png` under `data/wizardrumble/textures` on every PC alongside the same 0.9.2 DLL and audio assets. `scripts/generate-glow-textures.py` creates both original masks mathematically; RGB and alpha reach zero at every border. An additive path that ignores texture alpha therefore still receives black at the tile edge, avoiding a bright or dark rectangle.

The plugin preloads one shared `ImageWrapper` for each texture during `onLoad`, keeps it outside copied replay scenes and releases it during unload. Drawing uses the current callback's Canvas only, with clipped destination rectangles and proportional source UVs in texels. `DrawTile` receives an explicit additive blend and normalized tint; intensity is folded into RGB so fading does not rely solely on alpha. There is no texture load/retry inside the per-frame drawing path. The lifecycle follows the [SDK's image-wrapper guidance](https://bakkesplugins.com/wiki/bakkesmod-sdk/code-snippets/creating-image-wrapper); texel coordinates and additive drawing follow the [Epic UE3 Canvas guide](https://docs.unrealengine.com/udk/Three/CanvasTechnicalGuide.html).

`wr_status` reports `glow_textures=1` after both images load. If either image is unavailable, the renderer keeps its geometry fallback, which looks thinner. Patronus retains a filled body whenever its drawing allowance can fit a complete shell. Correct the installed PNGs and use `wr_glow_reload` outside an active cast; loading may briefly stutter. A successful image load does not by itself verify native blend appearance or performance.

## Preferences retained

- Avada's charge is the target car's green body glow only: no surrounding charge cloud, orbit rings, premature killing bolt or early explosion.
- Protego is circular with an open centre and narrow rune detail. It must remain readable even if a Canvas backend ignores line alpha.
- A frozen car receives the requested thin light-blue outline; its full body is not covered by ice geometry.
- Generic decorative world charge circles stay removed. Directional caster energy, the actual Protego wall, and brief impact arcs serve separate purposes.
- The compact HUD stays at the bottom-left. Spell names, countdown, selected target and **R** activation remain readable; this pass does not enlarge the interface or cover the native boost display.
- Slug flight, camera arrival and slide retain their separate phases. Cosmetic preferences do not disable a victim's gameplay obstruction.

## Motion, ownership and drawing priority

Curves use continuous functions of the supplied effect/frame clock. They do not choose a new random path every rendered frame. Replay sampling keeps its existing matching-actor pose interpolation and animation clock; pause, seek and repeated rendering at one recorded time should reproduce the same geometry. A snapshot joins the remaining effect phase instead of replaying the original cast or voice. Unique local car keys retain target ownership when neighbouring LAN bots share a numeric ID.

World effects have a **2,200-unit drawing budget**, counting a soft sprite as one drawing unit alongside existing geometry. Physical Protego walls draw first, then up to eight visible Patronus summons share a bounded reserve. Frozen-car feedback gets a small protected slice before fresh casts; sustained tethers and decorative statuses use what remains. Current per-effect slices cap a ghost at 500 units, a fresh cast at 520, a held-car contour at 24 strokes and a sustained status at 180 units. Projected sprite radius and cumulative visible tile area are bounded separately to avoid oversized near-camera tiles and excessive soft-light overlap. Patronus chooses a complete detailed or coarse filled shell when that shell and a small light reserve fit its allowance; it falls back to bounded surface/contour cues when neither whole shell fits. This avoids drawing a partial shell with visible holes. In crowded frames secondary detail can disappear before a wall, hold contour or new impact. Atmosphere, HUD and intentionally obstructive camera work remain separate screen-space layers; 2,200 is not a bound on every Canvas call or GPU triangle.

Geometry clips at the near plane and viewport. Very close or off-screen effects should not create enormous triangles, cover the entire pitch with one stroke or borrow a respawned target's position. Resolution checks cover 1280Ã—720, 1920Ã—1080 and 2560Ã—1440, with native camera and performance validation still required.

## Rendering limits and evidence

The renderer projects additive texture sprites, strokes and triangles into the local view. The soft energy has a volumetric appearance assembled from projected samples; it is not a native volumetric material, an engine particle system, an arena light source or guaranteed HDR bloom. The clean green charge is a pose-fitted projected car approximation. Native depth-buffer occlusion and exact body fit need in-game inspection, and light can remain visible through stadium scenery. The brief green upper-view effect is a local overlay rather than an arena sky/lighting change.

Protego keeps an unfilled centre and the frozen car keeps its thin outline. The Patronus now combines a stylised filled face shell with restrained additive surface light. Its source-profile loft is an approximation of the supplied car rather than the original full-resolution model or a native Rocket League car actor. Visible shell faces are culled and sorted within that summon, but there is no shared native depth buffer for stadium geometry, real cars, or other effects. Filled-face alpha depends on the Canvas backend; the body is intentionally substantial even when triangle alpha is ignored. The prior contour mesh remains available for resource/low-budget fallback. Camera slugs intentionally use opaque geometry to obscure the victim. Renderer previews may add a neutral car silhouette or field grid as context; additive compositing there models the intended sprite path and is not a Rocket League capture or proof of native GPU behavior.

Automated geometry, timing, identity, clipping, replay and workload results belong in [VALIDATION.md](VALIDATION.md). The native checklist is in [PLAYTEST.md](PLAYTEST.md). A successful compile or attractive preview does not establish live frame rate, native occlusion, audio output or multiplayer appearance.

## Reference direction

The official encyclopedia describes the Patronus as a silvery guardian form. That informs the pale spectral palette; choosing an Octane remains this mod's adaptation. [Official Patronus Charm](https://www.harrypotter.com/fact-file/spells/the-patronus-charm).

The official Shield Charm description presents protection through an invisible deflecting shield. That supports a clear-centred visual direction, while the circular runic ball barrier is this mode's design. [Official Shield Charm](https://www.harrypotter.com/fact-file/spells/the-shield-charm).

Framestore's account of its work across the films provides broader context for creature, silhouette and magical-effect craft. It is contextual inspiration, not a source specification for these procedural animations. No imagery, textures, models, animation footage or other assets from these reference pages are copied into the mod. [Framestore: Harry Potter â€” A History of Magic](https://www.framestore.com/work/harry-potter-history-magic).
