# GRVBOX — groovebox pour M-VAVE FM-1

Firmware alternatif qui transforme le FM-1 en groovebox sur le modèle de la Roland MC-303 : 8 parties (R rythmique + 7 mélodiques), séquenceur à patterns enregistré en temps réel ou pas à pas (TR-REC), arpégiateur, réglages « realtime modify », effets reverb / delay / chorus et Low Boost, afficheur LED rouge à 7 segments. Projet indépendant, sans lien avec Roland ni M-VAVE ; tous les sons sont synthétisés (aucun échantillon de la ROM d'origine).

Bâti sur [foma](https://github.com/konsumer/foma) (plateforme Felucca) : installeur web, mode secours et retour au firmware d'origine inclus.

## Ce qu'il y a dedans

| Élément | Contenu |
|---|---|
| Parties | R (batterie) et 1 à 7 ; chacune a son son, son niveau, son panoramique, son octave, ses réglages temps réel et ses envois d'effets, enregistrés avec le pattern (comme sur l'original) |
| Sons | 24 sons : ACID BASS, ACID SQR, 101 BASS, 808 BASS, FM BASS, SUB BASS, SAW LEAD, SQR LEAD, FM LEAD, SYN BRASS, HOUSE STAB, ORGAN STAB, E.PIANO, CLAV, BELLS, STRINGS, WARM PAD, CHOIR, SUPERSAW, PLUCK, ZAP, NOISE SWEEP, SINE, ORGAN. Les notes accentuées (vélocité ≥ 110) ouvrent davantage le filtre, comme une ligne acid |
| Kits | 808, 909, 606, CR-78, 16 sons chacun |
| Polyphonie | 12 voix de synthé partagées par les parties 1 à 7, plus 8 voix de batterie |
| Séquenceur | 8 patterns de 1, 2 ou 4 mesures, résolution double croche, shuffle jusqu'au ternaire, 40 à 240 BPM, tap tempo |
| Enregistrement | temps réel en boucle par superposition, quantifié à la double croche ; TR-REC pour la partie R (les 16 premières touches = les 16 pas) ; l'arpège est enregistré tel qu'il joue |
| Arpégiateur | 10 styles : UP, DOWN, UP&DOWN, RANDOM, AS PLAYED, OCTAVES (ligne de basse), ALBERTI, RHYTHM GTR, REGGAE, WALKING ; 1 à 4 octaves ; 1/8, 1/16, 1/16T, 1/32 ; accent |
| Effets | reverb, delay pointé synchronisé au tempo, chorus (envois par partie), Low Boost |

8 patterns d'usine : HOUSE, TECHNO, HIP HOP, DRUM BASS, ELECTRO, ACID, TRANCE, BREAKBEAT.

## Commandes

| Commande FM-1 | Fonction |
|---|---|
| SELECT | partie courante : R, 1 à 7 |
| ALGORITHM | son de la partie (partie R : le kit ; en TR-REC : le son de batterie à programmer) |
| PRESETS | pattern P1 à P8 (en lecture : à la fin du pattern) |
| PLAY | lecture / arrêt |
| REC | enregistrement temps réel (à l'arrêt : démarre aussi la lecture) ; métronome pendant l'enregistrement |
| SEQ | TR-REC sur la partie R : les 16 premières touches allument / éteignent les pas, OCT− / OCT+ choisissent la mesure ; maintenu 1 s : efface la partie courante |
| ARP | arpégiateur sur la partie courante |
| SCL | TAP tempo |
| GLO | mute de la partie courante ; maintenu + touches 1 à 8 : mute de ces parties |
| HOME | REALTIME : CUTOFF, RESO, ENV, PORTA |
| ENV | PART : TONE, LEVEL, PAN, OCTAVE |
| LFO | LFO : RATE, PITCH, FILTER |
| FX | EFFECTS : REVERB, DELAY, CHORUS, BOOST |
| EDIT | ARPEGGIO : STYLE, RANGE, RATE, ACCENT (re-presser : PATTERN : TEMPO, SHUFFLE, BARS, KIT) |
| SAVE | garde les 8 patterns en mémoire flash |
| Clavier | joue la partie courante (partie R : les 16 sons de batterie) ; OCT− / OCT+ : octave |
| MASTER | volume |
| MIDI | canal 10 : partie R (note 36 = premier son) ; canaux 1 à 7 : parties 1 à 7 |

L'afficheur LED rouge montre le pattern et le tempo (P1.124), ou la dernière valeur touchée (CU. 12 pour le cutoff, t. 5 pour un son, etc.). Dessous : les 8 parties (orange : la partie courante, vert : elle joue, rouge : en mute), le son de la partie, puis les 16 pas de la mesure avec la tête de lecture.

## Obtenir et installer le firmware

- **GitHub** : pousse ce dossier sur la branche `main` d'un dépôt. Le workflow compile tout ; le `.fwsc` est dans l'artefact `grvbox-fwsc` et sur la branche `firmware`. Avec Settings → Pages → Source : GitHub Actions, l'émulateur et l'installeur sont publiés sur GitHub Pages.
- **Docker Desktop** : `./fm1 build` ou `./fm1 dev` (http://localhost:8000).

Installation depuis la page Install (Chrome ou Edge, FM-1 en USB). **Secours** : OCT− maintenu à l'allumage ; l'onglet Rescue remet le firmware M-VAVE V15. Installer un firmware non officiel se fait à vos risques.

## Développement

- `src/voice.c` : voix de synthé et de batterie, effets (entiers uniquement) ; `src/tones.c` : les 24 sons
- `src/seq.c` : horloge à 96 pas par noire, patterns, enregistrement, TR-REC, arpégiateur (dans `audio()`)
- `src/main.c` : afficheur, parties, pages, LEDs, MIDI, sauvegarde
- `tools/gen_patterns.py` : les patterns d'usine écrits en grilles → `src/patterns.h`
- `tests/run.sh` : rend les 8 patterns en WAV, vérifie la quantification, le TR-REC et les 10 styles d'arpège, puis fuzz
- Identité du paquet : `FM-1_73` (dans `firmware.json`)

## Licence

GPL-3.0-only. Crédits : [LICENSING.md](LICENSING.md).
