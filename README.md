# AeroSeed FX — code source (JUCE 7)

Plugin d'**effet** (VST3) pour FL Studio : il traite le son d'un instrument existant, il n'en génère pas.
Idée originale : **Omyll** (crédits cachés dans le logo).

## 1. Compiler le plugin (pas à pas)

1. Installe **JUCE 7** (gratuit) depuis juce.com, puis ouvre **Projucer**.
2. *New Project → Plug-In → Basic*. Nom : `AeroSeedFX`.
3. Dans les réglages du projet (icône engrenage) :
   - **Plugin Formats** : coche `VST3` (et `Standalone` pour tester sans FL Studio).
   - **Plugin Characteristics** : ne coche rien (c'est un effet, pas un instrument, sans MIDI).
   - **C++ Language Standard** : `C++17` (ou plus récent).
4. Dans l'onglet **Modules**, vérifie que `juce_dsp` est présent (sinon : bouton `+` → `juce_dsp`).
5. Dans l'onglet **File Explorer**, supprime les fichiers d'exemple du dossier `Source`, puis fais glisser **tous les fichiers du dossier `Source/`** fourni ici.
   ⚠️ N'ajoute **pas** le dossier `Tests/` au projet (c'est un programme à part).
6. Clique sur l'icône de ton IDE (Visual Studio sur Windows, Xcode sur Mac), puis compile en **Release**.
7. Le fichier `AeroSeedFX.vst3` est copié dans le dossier VST3 du système. Dans FL Studio : *Options → Manage plugins → Find plugins*, puis place AeroSeed FX sur l'insert de mixage de ton instrument.

Si la compilation affiche des erreurs, copie-les telles quelles : elles indiquent le fichier et la ligne.

## 2. Organisation des fichiers

| Fichier | Rôle |
|---|---|
| `AeroDSP.h` | Cœur audio en C++ pur, **testé** : seed, gammes, FFT, moteur harmonique façon Chroma (Spectral Morph / Spectral Gate), crossover 3 bandes, analyse du spectre, détection de tonalité |
| `PluginProcessor.*` | Paramètres automatisables, chaîne d'effets, latence, sauvegarde du projet |
| `PluginEditor.*` | Fenêtre 1280×720 : intro, choix du mode, disposition, menus, glisser-déposer |
| `AeroUI.h` | Style « liquid glass », polices, curseurs vivants (feu, plante, éclair, glace, eau), barre de trigger, gouttes pixelisées, clavier de notes, cartes de modules, crédits |
| `AeroScene.h` | 8 fonds, bulles, pluie liée au Dry/Wet, animation d'ouverture (vague + éléments) |
| `SpectrumView.h` | Analyseur 20 Hz – 20 kHz en lecture seule : tendance, tonalité entrée/sortie, infobulle (dB, Hz, note, cents) |
| `EditPanel.h` | Éditeur d'effets plein écran flouté, 5 onglets |
| `IntroLayer.h` | Écran noir de démarrage, cartes de choix, vague d'ouverture |
| `LogoData.h`, `BgData.h` | Logo et 3 photos de fond, intégrés au code (rien à ajouter dans Projucer) |
| `Tests/DSPTests.cpp` | Tests automatiques du cœur audio |

**Chaîne audio** : Harmonique → (Filtre avant/après) → Distortion → Bitcrusher (global ou 3 bandes) → Chorus → mélange Dry/Wet avec la fenêtre d'activation synchronisée.

## 3. Ce qui a été vérifié

Le cœur audio (`AeroDSP.h`) a été compilé et testé (`g++ -O2 -std=c++17 Tests/DSPTests.cpp`) :

- reconstruction parfaite quand le Morph est à 0 (erreur 3,6e-7), latence mesurée de 2048 échantillons, déclarée à FL Studio ;
- 460 Hz en Do majeur ressort à 440 Hz (La), avec un niveau conservé ;
- le Spectral Gate coupe un signal sous le seuil, et n'agit pas quand le Morph est à 0 ;
- les attaques (transitoires) sont détectées et gardent leur son d'origine ;
- le crossover 3 bandes est parfaitement plat (écart 0,0000 dB) ;
- la tonalité est détectée correctement (Do-Mi-Sol → C majeur, La-Do-Mi → A mineur) ;
- la même seed donne exactement les mêmes réglages que dans la maquette ;
- le moteur harmonique consomme environ 5 % d'un cœur en stéréo.

La partie JUCE (interface et liaison avec FL Studio) a été **relue ligne par ligne mais n'a pas pu être compilée** (JUCE n'est pas disponible dans mon environnement). Des erreurs de compilation restent possibles : envoie-les et je les corrige.

## 4. Rapport de relecture (problèmes trouvés et corrigés)

1. **Latence mal déclarée** (trouvé par les tests) : 1536 annoncés au lieu de 2048 réels. Cela aurait décalé le dry/wet et la compensation de FL Studio.
2. **Volume +35 % sur les notes recalées** : tout un pic spectral était tassé dans une seule case. Le pic est maintenant déplacé en bloc.
3. **Son « phasé » du vocodeur** : ajout du verrouillage de phase autour des pics (Laroche & Dolson), niveau exact.
4. **Clics pendant l'automation des fréquences de coupure** : les filtres étaient remis à zéro à chaque changement.
5. **2 erreurs de compilation** (listes `{…}` non typées) et un constructeur de carte appelé avec le mauvais nombre d'arguments.
6. **Fondus inopérants** dans l'animation d'ouverture (opacité annulée par `setColour`).
7. **Caractères spéciaux** (♪ ● ↺…) qui s'afficheraient en carrés vides : remplacés par des dessins.
8. **Spectre de l'éditeur** sans l'image de fond : corrigé.
9. **Tonalité sur silence** : affiche « -- » au lieu de « C majeur 0 % ».
10. **Fenêtre d'activation** : décalée de la latence, pour rester calée sur la mesure.

## 5. Limites connues

- **Latence** : 2048 échantillons (≈ 46 ms à 44,1 kHz), compensée automatiquement par FL Studio. Le vrai Chroma annonce zéro latence, ce qui demande une technique beaucoup plus complexe.
- **Police** : Segoe UI (celle de Windows Vista, présente sur tous les PC Windows). Sur Mac, une police proche est utilisée.
- **Logo** : redessiné avec la police libre Poppins, car Lilita One (celle de la maquette) n'était pas disponible hors ligne.
- **Performance** : l'interface est animée 30 fois par seconde. Si FL Studio rame, remplace `startTimerHz (30)` par `startTimerHz (20)` dans `PluginEditor.cpp`.
- **Droits** : les 3 photos de fond (colline Windows XP, dauphins, image Freepik) ne sont pas libres de droits. Remplace-les avant toute distribution publique.
