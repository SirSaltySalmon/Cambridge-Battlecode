# v151-tc

v151-tc = v151 (the ladder bot: v149-cap code + evotune numbers) with only these build tables changed (src/constants.h);
Varg, Cassi/Issac, Ren, Stellar, Vendetta unchanged. Notation: family:stat, o = the offensive stat (Strength for
Vesper/Marcella/Ryth, Focus for Prism/HoR/Willow), h = Health. Theorycraft from v151 per-game data: Monster early for
everyone, Hunter/Skirmish only on Vesper, Treasure only on the gold earners, Collectors at 7 families, Jungle only on
the camp tanks, Marcella on Strength, no Greed.

- VESPER: treasure:o monster:o hunter:o skirmish:o home:o durability:h collectors:o bruiser:h jungle:o tribute:h capture:o | then repeat: power:h power:o power:o
- MARCELLA: treasure:o monster:o durability:h tribute:o jungle:h skirmish:o collectors:o prospector:h | then repeat: power:o power:h
- PRISM: treasure:o monster:o durability:h prospector:o home:h tribute:o collectors:o jungle:h | then repeat: power:o power:h
- HOR: capture:focus jungle:focus monster:h durability:h | then repeat: bruiser:focus
- RYTH: jungle:h monster:h durability:h | then repeat: bruiser:h
- WILLOW: monster:focus durability:focus | then repeat: power:health power:focus

Result vs v151 (70 maps x 2 sides, release): 3 seeds 208-195-17, banked +4.6%, collected +3.1%, gold +8.7%; every seed
positive (+7.7 / +3.1 / +3.0%). Fuel on seed 0 34.2k vs 33.9k a turn (+1.2%). Details: results/results-builds-vs-v151.txt.
