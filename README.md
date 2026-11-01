# SoulKut

Plugin (VST3 / Standalone) qui découpe automatiquement un sample en slices de 1/4, 1/8, 1/16, 1/32 ou 1/64 de mesure,
d'après son BPM et sa gamme détectés automatiquement.

## Compilation
Logiciels requis : Git + CMake (3.22+) + Visual Studio 2022 (Windows) ou Xcode (Mac).
Raccourcis : build_windows.bat (Windows) / build_mac.sh (Mac).
VST2 (si tu as le SDK) : ajoute -DSOULKUT_VST2_SDK="chemin/vstsdk2.4" à la première commande cmake.

Détail :
Windows (Visual Studio 2022) :
    cmake -B build -G "Visual Studio 17 2022"
    cmake --build build --config Release
macOS / Linux :
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build
Le VST3 est copié automatiquement dans le dossier VST3 du système. JUCE est téléchargé au premier build.

## Utilisation
1. Charge SoulKut comme instrument (piste MIDI) dans ton logiciel.
2. Glisse un sample dans la fenêtre : BPM, gamme et premier temps sont détectés.
3. Corrige si besoin : BPM (/2 et x2 pour une erreur d'octave), décalage (bouton Auto = recale sur la 1re attaque), découpe.
4. Slice 1 = note MIDI 36, slice 2 = 37, etc. Clic sur la forme d'onde = pré-écoute.
5. Mode gate : la slice s'arrête au relâchement de la note (sinon one-shot, la slice joue en entier).
6. Accorder : choisis une tonique cible, le sample est transposé par le chemin le plus court ; "Transpo" ajoute des demi-tons.
   Attention : c'est un changement de vitesse de lecture (comme un sampler), la durée des slices change avec le pitch.
7. Glisser-déposer : glisse une slice (ou le bouton "Glisser toutes les slices") vers une piste/playlist de ton logiciel.
   Selon le logiciel, cela peut ne pas être accepté ; l'export vers un dossier reste le plan B.
8. "Exporter les slices" : WAV 24 bits, nom du type sample_120bpm_A_1-16_001.wav (sans pitch appliqué).

## Compilation automatique dans le cloud (sans rien installer)
1. Crée un dépôt GitHub (gratuit) et envoie-y tout le contenu de ce dossier (le dossier .github compris).
2. Onglet "Actions" > "Build SoulKut" : GitHub compile le plugin sur Windows et sur Mac.
3. Quand c'est vert, ouvre l'exécution et télécharge "SoulKut-Windows-VST3" ou "SoulKut-macOS-VST3" (section Artifacts).
4. Copie le dossier SoulKut.vst3 dans C:\Program Files\Common Files\VST3\ (Windows) ou ~/Library/Audio/Plug-Ins/VST3/ (Mac).
Sur Mac, le plugin n'est pas signé : au premier lancement, clic droit > Ouvrir, ou autorise-le dans Réglages Système > Confidentialité et sécurité.
