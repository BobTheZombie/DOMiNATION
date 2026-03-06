# Civilization Architecture Packs

Themes are stored in `content/civilization_themes.json` and contain:
- silhouette language
- roof and wall material cues
- accent colors, banner motifs, decorative props
- biome preferences
- `building_family_mappings` from gameplay family to variant asset id

Implemented packs: Rome, China, Europe, Middle East, Steppe, African Sahel.

Runtime keeps gameplay building families authoritative while selecting visual variant IDs via civilization theme mappings.
