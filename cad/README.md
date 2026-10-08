# CAD – instrumentpanel for 1977 Camaro

FreeCAD 1.1-modell av et 3D-printet instrumentpanel som holder
Waveshare ESP32-S3-Touch-LCD-7 i midten og fire runde 1,28" GC9A01-skjermer
(to på hver sidevinge).

- `Camaro.FCStd` – modellen. Alle mål styres fra `Spreadsheet`.
  Deler: midtdel (`Body`), `SideVenstre`, `SideHoyre`, `Bakbrakett`, samt
  `RundSkjermV`/`RundSkjermH` (referansemodeller av skjermmodulene, ikke til print).
  - Sidevingene står 38° mot midtplata (`SideVinkel`) og har øvre kant parallell med midtdelen.
  - Rundskjermene ligger felt inn i vingen, vridd til 23° mot midtplata (`SkjermVinkelMotMidt`),
    med trakt (`TraktVinkel`) og rundt hus bak. Mål på modulen (`ModulDia`, `ModulLengde`,
    `ModulTappBredde`, `ModulTykkelse`) er foreløpige og må måles på kortet.
- `Eddie_Nokkelring.FCStd` – egen, urelatert modell (nøkkelring).
- `print/` – STL-eksport av ferdige deler.
- `print/tynn/` – tynne prototyper og rammer for å sjekke mål. `*_test_stodig.stl` er
  stive prøveprint av vingens omriss (6 mm bred ramme, 3 mm høy, to tverrribber).
- `referanse/` – skannede tegninger av originalpanelet og Waveshares måltegning.

`*.FCStd` og `*.stl` lagres med Git LFS. FreeCADs sikkerhetskopier (`*.FCBak`) ignoreres.
