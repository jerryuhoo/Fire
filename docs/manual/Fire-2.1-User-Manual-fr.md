# Fire 2.1 Manuel d'utilisation

**Français / Édition de développement / 24 octobre 2026**

[English](Fire-2.1-User-Manual.md) | [简体中文](Fire-2.1-User-Manual-zh-CN.md) | [日本語](Fire-2.1-User-Manual-ja.md) | [Français](Fire-2.1-User-Manual-fr.md) | [Español](Fire-2.1-User-Manual-es.md)

Instructions détaillées avec des captures authentiques de l'interface. Révision de référence du produit : `9ea20d2`. Les noms des commandes et des préréglages restent en anglais pour faciliter leur repérage. Le PDF contient un sommaire et des signets cliquables. Conservez le dossier `images` à côté de ce fichier Markdown lorsque vous le partagez.

## Sommaire

- [01. Prise en main](#01-prise-en-main)
- [02. Interface et spectre](#02-interface-et-spectre)
- [03. Préréglages et Sound Library](#03-préréglages-et-sound-library)
- [04. A/B, égalisation de niveau Match et historique des modifications](#04-ab-égalisation-de-niveau-match-et-historique-des-modifications)
- [05. Chaînes, routage parallèle et Mid/Side](#05-chaînes-routage-parallèle-et-midside)
- [06. Drive et Shape](#06-drive-et-shape)
- [07. Compressor et OTT](#07-compressor-et-ott)
- [08. Stereo](#08-stereo)
- [09. EQ et EQ dynamique](#09-eq-et-eq-dynamique)
- [10. Chorus, Flanger et Phaser](#10-chorus-flanger-et-phaser)
- [11. Delay](#11-delay)
- [12. Reverb](#12-reverb)
- [13. Lo-Fi](#13-lo-fi)
- [14. Chord Resonator](#14-chord-resonator)
- [15. Granular, Freeze et fichiers d'échantillons](#15-granular-freeze-et-fichiers-déchantillons)
- [16. Routages de modulation et macros](#16-routages-de-modulation-et-macros)
- [17. Formes de LFO et lecture](#17-formes-de-lfo-et-lecture)
- [18. Enveloppes et sidechain externe](#18-enveloppes-et-sidechain-externe)
- [19. Contrôle MIDI](#19-contrôle-midi)
- [20. Recettes pratiques de création sonore](#20-recettes-pratiques-de-création-sonore)
- [21. Réglages, performances et dépannage](#21-réglages-performances-et-dépannage)
- [22. Référence rapide et assistance](#22-référence-rapide-et-assistance)

<!-- manual:body -->

## 01. Prise en main

Cette édition décrit **Fire 2.1.0 en cours de développement**, avec le système de modulation étendu, l'EQ dynamique, les chaînes parallèles, l'importation et l'exportation d'échantillons, ainsi que l'affichage OTT unifié. Elle a été préparée le **24 octobre 2026** à partir du code du produit à la révision **9ea20d2**. Consultez **Menu > Settings** pour identifier la version chargée par votre logiciel hôte. Une version téléchargeable publiquement peut proposer moins de fonctions.

Les exemples utilisent des captures authentiques de l'interface actuelle du plug-in, avec de l'audio de démonstration généré. Les réglages illustrés ne sont ni des préréglages étalonnés ni des valeurs imposées. La plupart des figures utilisent Ink ; Vintage illustre les affichages inspirés du matériel. L'apparence ne modifie pas le son.

### Installer et charger Fire

Fire est fourni dans des formats de plug-in utilisables avec les hôtes compatibles. Choisissez un format accepté par votre station audionumérique (DAW) ; Audio Unit est un format macOS. Conservez le paquet du plug-in intact lors de la copie.

| Plateforme | Format | Dossier d'installation habituel |
| --- | --- | --- |
| macOS | Audio Unit | `~/Library/Audio/Plug-Ins/Components/` |
| macOS | VST3 | `~/Library/Audio/Plug-Ins/VST3/` |
| macOS | CLAP | `~/Library/Audio/Plug-Ins/CLAP/` |
| Windows | VST3 | `C:\Program Files\Common Files\VST3\` |
| Windows | CLAP | `C:\Program Files\Common Files\CLAP\` |
| Linux | VST3 | `~/.vst3/` |
| Linux | CLAP | `~/.clap/` |

1. Téléchargez la version appropriée depuis les [versions officielles de Fire](https://github.com/jerryuhoo/Fire/releases). Vérifiez le système d'exploitation et l'architecture du processeur indiqués pour le paquet. Une compilation locale de développement peut ne contenir que l'architecture de la machine qui l'a produite.
2. Fermez votre DAW avant de remplacer une installation existante. Copiez le format nécessaire dans son dossier de plug-ins ; créez le dossier utilisateur s'il manque. Sous Windows, les dossiers système peuvent nécessiter des droits d'administrateur.
3. Redémarrez votre DAW et relancez l'analyse des plug-ins si nécessaire. Recherchez **Fire** parmi les **effets audio**, sous Blue Wings Music si l'hôte classe les plug-ins par fabricant.
4. Insérez-le sur une piste audio, après un instrument logiciel ou sur un bus audio. Vérifiez que la piste est dirigée vers une sortie audible.
5. Ouvrez Settings et confirmez la version chargée. Si une ancienne version apparaît, vérifiez le format sélectionné par l'hôte et ses dossiers de recherche avant de copier à nouveau le plug-in.

Sous macOS, une version de développement téléchargée ou signée de manière ad hoc peut nécessiter une autorisation de sécurité du système. Suivez les consignes de signature de la version et les indications du système d'exploitation. Si un paquet échoue à la validation, récupérez une nouvelle copie officielle ; modifier ses fichiers internes invalide sa signature.

<!-- pagebreak -->

### Une première session en cinq minutes

Fire est un effet audio. Il a besoin d'un signal audio entrant : insérez-le après un instrument logiciel, sur une piste audio ou sur un bus recevant de l'audio. Des notes MIDI seules ne lui font pas produire un son d'instrument. Le MIDI peut piloter la modulation lorsque l'hôte le transmet au plug-in.

1. Lisez ou mettez en boucle un court passage comprenant des moments doux et forts. Commencez à un niveau d'écoute confortable.
2. Cliquez sur le nom du son dans l'en-tête de Fire. La **Sound Library** s'ouvre en pleine page. Choisissez **Analog Drive**, puis cliquez sur une scène telle que **Valve Bloom**, **Tweed Touch**, **Iron Program Bus** ou **Tape Studio Print**. Un simple clic charge le son et laisse la bibliothèque ouverte.
3. Cliquez sur **Back to Reactor**. Fire sélectionne la page Shape pertinente pour la scène ; certaines scènes placent leur étage de coloration dans Master Lab, d'autres utilisent une ou plusieurs bandes.
4. Tournez les quatre potentiomètres de macro près des onglets des espaces de travail. Dans les scènes Analog Drive, ils portent les noms **Drive**, **Tone**, **Output trim** et **Dry blend**. Ces noms décrivent les affectations propres à la scène ; il ne s'agit pas de quatre processeurs d'effets supplémentaires.
5. Activez **Match** et lisez environ trois secondes de la boucle. Attendez l'apparition d'une valeur de compensation. Cela permet de comparer le timbre sans se laisser convaincre uniquement par un niveau de sortie plus fort.
6. Appuyez sur **Copy**, passez sur l'autre mémoire avec **A/B**, puis modifiez ce second son. Cliquez sur le contrôle de valeur/Learn de Match, puis relisez le même passage pour mesurer une nouvelle compensation. Passez d'une mémoire à l'autre avec A/B pour les comparer.
7. Cliquez sur **Save** pour conserver le résultat sous forme de préréglage utilisateur. Enregistrez également le projet du DAW pour conserver les réglages de comparaison propres au projet, les verrouillages et la session complète de l'hôte.

![Fire avec l'habillage Ink : commandes d'en-tête, trois bandes de fréquences, onglets des espaces de travail, macros et chaîne de bande sélectionnée.](images/01-overview-ink.png)

## 02. Interface et spectre

L'en-tête réunit la comparaison A/B, Copy, Match, le nom du préréglage et les commandes précédent/suivant, Save, le menu de l'application et HQ. Le spectre supérieur affiche l'audio d'entrée et l'audio traité, et permet de modifier les bandes de fréquences. En dessous, trois espaces de travail organisent les commandes.

| Espace de travail | Fonction |
| --- | --- |
| **Band Lab** | Modifier la chaîne de la bande de fréquences sélectionnée, ainsi que ses réglages Output et Mix. |
| **Mod Forge** | Modifier les formes de LFO, leur comportement de lecture et la configuration MIDI ; ouvrir la matrice de modulation. |
| **Master Lab** | Modifier la chaîne qui traite la somme des sorties de bandes. |

Chaque espace audio dispose d'une liste **CHAIN**. Cliquez sur une ligne pour ouvrir le module correspondant. Le petit interrupteur près d'une ligne de traitement active ou désactive le module ; son apparence dépend de l'habillage. Le module Shape d'origine d'une bande présente une exception de compatibilité en mode Serial, décrite au chapitre 06. Survolez une ligne ou donnez-lui le focus pour faire apparaître sa commande de suppression **-**. Utilisez **+** pour ajouter un module et **...** pour copier, coller ou gérer les fichiers de chaîne. Le bouton compact affichant **Stereo**, **Mid** ou **Side** ouvre Routing.

Quatre macros restent facilement accessibles près des onglets des espaces de travail. Leurs valeurs et leurs noms font partie du son. Double-cliquez sur le nom d'une macro pour la renommer, appuyez sur Enter pour valider ou sur Escape pour annuler. Les noms acceptent jusqu'à 32 caractères ; un nom vide rétablit « Macro 1 », etc. Un double-clic sur le potentiomètre d'une macro remet sa valeur à zéro. Le renommage ne modifie pas son identité d'automation dans le DAW.

Lorsqu'un graphique dispose d'un menu **VIEW**, **Auto** suit le module sélectionné. Les choix explicites Waveform, Transfer, Meters et Stereo permettent d'examiner un autre aspect du même signal. Par exemple, une courbe de transfert facilite le réglage de la distorsion, tandis que Meters permet de mieux évaluer les changements de niveau. OTT dispose de son propre affichage **LIFT / PRESS**, y compris les instances OTT ajoutées indépendamment dans Band et Master. Une icône d'agrandissement développe le graphique ; utilisez l'icône de retour pour retrouver les commandes.

### Commandes du spectre et modification des bandes

Cliquez dans la partie inférieure d'une bande du spectre pour la sélectionner dans Band Lab. Cliquez dans la bande supérieure du spectre à la fréquence souhaitée pour créer une séparation, jusqu'à quatre bandes au total. Faites glisser une ligne de séparation pour modifier sa fréquence, ou modifiez la valeur de fréquence affichée. Les commandes d'activation et **S** permettent de contourner ou d'écouter en solo chaque bande. La commande de suppression d'une bande retire cette bande ; il doit toujours en rester au moins une.

Lorsque le fond du spectre lui-même a le focus, **Return** ou **+** divise la bande sélectionnée, tandis que **Delete** ou **Backspace** la supprime. Ces raccourcis ne s'appliquent pas lorsqu'un contrôle enfant, tel qu'un champ de fréquence, a le focus.

La barre compacte du spectre regroupe le sélecteur de canal, l'agrandissement/le retour et le repli. Elle devient translucide au repos et parfaitement lisible au survol, lorsqu'elle a le focus clavier ou lorsque son menu est ouvert. Replier le spectre libère de l'espace dans les trois espaces de travail. Une petite commande de restauration près des commandes des espaces de travail permet de le réafficher.

| Mode du spectre | Mesure affichée |
| --- | --- |
| **Left / Right** | Un canal d'entrée/de sortie. |
| **Stereo** | Énergie combinée des canaux ; les canaux de polarités opposées ne s'annulent pas. |
| **Mono** | `(L + R) / 2`, avec les éventuelles annulations dans la somme mono. |
| **Side** | `(L - R) / 2`, révélant les différences stéréo. |

Ce choix ne modifie que l'affichage. Choisissez le canal Routing de la chaîne pour modifier le traitement audio. Un signal identique à gauche et à droite disparaît en Side ; un signal de polarités opposées disparaît en Mono. C'est normal. Pour un signal présent sur un seul canal, Stereo affiche environ 3 dB de moins que la trace de ce canal seul, et Mono/Side environ 6 dB de moins.

### Lire les commandes

La valeur d'un potentiomètre peut apparaître lorsque vous le faites glisser, lui donnez le focus ou l'éditez, plutôt que de rester visible en permanence. Une infobulle décrit la commande. La modulation ajoute une plage de la couleur de la source et un repère mobile ; la valeur de base et la valeur modulée sont distinctes. Le bouton de graphique plein écran agrandit la vue sélectionnée et masque temporairement les commandes situées dessous. Réduisez la vue pour rétablir ces commandes.

L'éditeur conserve un rapport de 2:1 et accepte des dimensions de 1000 x 500 à 2000 x 1000. Redimensionnez-le depuis son coin ; une fenêtre plus grande offre davantage d'espace aux libellés des paramètres et aux détails des graphiques.

## 03. Préréglages et Sound Library

Cliquez sur le nom du préréglage pour ouvrir la Sound Library. Les collections se trouvent à gauche et les sons individuels à droite. Le catalogue d'usine comprend 236 scènes : 20 dans chacune des collections Vocals, Synths, Drums, Bass, Guitar, Keys, Spaces, Lo-Fi, Rhythm et Effects, ainsi que 36 dans Analog Drive. Les sons utilisateur apparaissent dans **User**. Les sons d'usine servent de points de départ : enregistrez une scène modifiée comme préréglage utilisateur.

Saisissez du texte dans le champ de recherche pour filtrer les sons selon leurs informations descriptives. Cliquez sur une ligne pour l'écouter immédiatement. Double-cliquez sur une ligne, cliquez sur **Back to Reactor** ou appuyez sur Escape pour revenir au traitement. Lorsque le champ de recherche n'a pas le focus, les flèches Up/Down permettent d'écouter les sons voisins. Le son actuel reste surligné. Les flèches précédent/suivant de l'en-tête parcourent également les préréglages.

L'éditeur mémorise la collection, la recherche et la position de défilement actuelles pendant votre travail. **Recent** conserve jusqu'à 24 sons récemment écoutés. **Locate current** efface la recherche et révèle le préréglage en cours dans sa collection. L'historique récent et la position de navigation sont des aides propres à la session ; ils ne font pas partie des fichiers de sons ni des projets du DAW.

### Favoris, masquage et restauration

Cliquez sur l'étoile d'un son pour l'ajouter à **Favourites** ou l'en retirer. Les favoris sont mémorisés dans la bibliothèque locale. La commande de suppression d'un préréglage agit différemment selon son origine :

| Type de son | Action de suppression | Récupération |
| --- | --- | --- |
| Préréglage utilisateur | Déplace le fichier dans la **Recycle Bin** de Fire. | Le restaure dans sa collection d'origine. |
| Préréglage d'usine | Masque l'entrée d'usine. | Restaure le son masqué depuis Recycle Bin. |

Restaurer un son ne le charge pas et n'écrase aucun fichier utilisateur existant. Retirer une entrée est donc différent de réinitialiser le son actuellement joué. Utilisez **Menu  >  Init** pour retrouver l'état initial du son.

### Étiquettes et descriptions

![Les détails du préréglage permettent de modifier ses étiquettes et ses notes ; les modifications des sons d'usine restent des remplacements locaux de métadonnées.](images/21-preset-details.png)

Sélectionnez un son, puis cliquez sur l'icône de crayon/modification des détails pour modifier ses étiquettes et sa description. Séparez les étiquettes par des virgules ; placez une étiquette entre guillemets si elle contient elle-même une virgule. Fire accepte jusqu'à 16 étiquettes, 32 caractères par étiquette et une description de 2 048 caractères maximum.

Pour un préréglage utilisateur, ces informations sont intégrées à son fichier `.fire`. Modifier les détails préserve les réglages audio et tout enregistrement granulaire figé. Enregistrer par-dessus le préréglage conserve ses notes ; enregistrer une nouvelle version reprend les notes du son sélectionné. Les modifications d'un son d'usine sont propres à votre bibliothèque : **Restore original** rétablit les étiquettes et la description fournies. Restaurez un préréglage utilisateur archivé avant de modifier ses détails. Si le fichier a changé pendant que l'éditeur de détails était ouvert, rouvrez-le avant d'enregistrer.

### Conserver certaines valeurs pendant la navigation

> Lorsque plusieurs modules possèdent une commande du même nom, utilisez de préférence le menu contextuel du potentiomètre concerné pour verrouiller une valeur précise. Un verrou préserve la valeur de base lors du chargement d'un son ; il ne fige pas l'audio entrant, ne désactive pas la modulation et ne verrouille pas automatiquement tout un module.

Les verrouillages de paramètres de préréglage conservent les **valeurs de base des potentiomètres** choisies lorsque vous chargez ou parcourez les préréglages complets du plug-in. Ils sont utiles, par exemple, pour écouter des sons tout en conservant une valeur donnée d'Output ou de Mix.

1. Faites un clic droit sur un potentiomètre compatible et choisissez **Keep value when loading presets**. Vous pouvez aussi ouvrir **Menu  >  Preset parameter locks...**.
2. Dans la fenêtre de verrouillage, recherchez les paramètres et activez ceux à conserver. **Locked only** limite la liste aux verrouillages actuels ; **Unlock all** les supprime tous.
3. Chargez un autre préréglage. Les valeurs de base verrouillées restent inchangées. Le son peut afficher l'astérisque habituel `*` indiquant une modification, car il diffère du préréglage enregistré.

![Le gestionnaire de verrouillage des préréglages répertorie les commandes disponibles. Un clic droit sur le potentiomètre exact permet d'identifier sans ambiguïté le module répété que vous verrouillez.](images/20-preset-locks.png)

Les verrouillages ne désactivent ni les modifications manuelles, ni l'automation de l'hôte, ni la modulation. Les routages de modulation et les définitions de sources entrants sont toujours chargés ; une valeur de base verrouillée peut donc continuer à varier sous l'effet de la modulation. Les réglages structurels (nombre de bandes, séparations, présence/ordre des modules, mode de routage et affectation aux branches) ne peuvent pas être verrouillés. Les préférences de verrouillage appartiennent à l'état de l'instance dans le projet, pas aux sons `.fire`. Le changement de mémoire A/B, Undo/Redo et la restauration du projet rétablissent des états complets sans appliquer les verrouillages de navigation.

### Où les données sont-elles enregistrées ?

| Stockage | Contenu principal |
| --- | --- |
| Préréglage complet `.fire` | Paramètres sonores, chaînes, sources et affectations, noms des macros, audio granulaire figé et notes utilisateur. |
| Préréglage `.firechain` | Modules/réglages d'une chaîne, canal et routage parallèle ; éventuellement ses routages de modulation vers les destinations. |
| Projet du DAW | État complet de l'instance, dont les informations Match A/B et les préférences de verrouillage des préréglages. |
| Préférences/bibliothèque locales | Habillage préféré, favoris, entrées d'usine masquées et remplacements de métadonnées d'usine. |
| Session actuelle de l'éditeur | Recherche/défilement/historique récent de la bibliothèque ; écoute temporaire du détecteur. |

Utilisez **Menu  >  Open Preset Folder** pour localiser votre bibliothèque et **Rescan Preset Folder** après avoir géré les fichiers de sons en dehors de Fire. Sauvegardez vos propres préréglages et projets.

<!-- figure:landscape -->
![Sound Library : la collection Analog Drive contient 36 scènes d'usine. L'étoile, la commande de suppression et le crayon agissent sur les éléments de la bibliothèque.](images/19-preset-library.png)

## 04. A/B, égalisation de niveau Match et historique des modifications

Le bouton **A/B** de l'en-tête commute entre deux mémoires sonores complètes. **Copy** copie la mémoire actuelle dans l'autre. Ces mémoires sont distinctes des **branches A/B** d'une chaîne d'effets parallèle : les mémoires comparent des états complets, tandis que les branches traitent l'audio simultanément.

Pour comparer à niveau perçu égal, mettez en boucle le même passage représentatif pour les deux mémoires. Activez **Match** et laissez la mémoire actuelle effectuer sa mesure pendant environ trois secondes. Changez de mémoire avec A/B et laissez l'autre effectuer sa mesure. Chaque mémoire conserve sa propre compensation fixe en dB. Copy copie aussi cette compensation.

Cliquez sur le contrôle de valeur/Learn pour mesurer à nouveau ; cliquez pendant une mesure pour l'annuler. Désactiver Match permet d'entendre le niveau non corrigé. Match ne modifie pas Output ni son automation. Le bypass de l'hôte laisse passer le signal contourné et suspend la mesure.

**Play audio** signifie que la mesure n'a pas reçu suffisamment de signal d'entrée et de sortie exploitable ; tout résultat valide précédent est conservé. **LIMIT** signifie que la correction dépasserait la plage disponible de ±18 dB. Match est un outil de comparaison sur une courte durée, pas un indicateur LUFS intégré. Sa compensation reste fixe pendant vos modifications : relancez donc la mesure après un changement important. Charger un préréglage sonore efface la mesure de cette mémoire et lance une nouvelle mesure si Match reste actif. Match et les gains mesurés sont enregistrés dans les projets, séparément des fichiers `.fire`.

### Annuler et rétablir

Utilisez **Menu  >  Undo / Redo**, **Cmd/Ctrl-Z**, **Cmd/Ctrl-Shift-Z** ou **Ctrl-Y**. Undo regroupe les gestes normaux sur les potentiomètres et les opérations comme la duplication d'un module ou le remplacement d'une chaîne en modifications cohérentes. Il couvre aussi les formes de LFO, les routages, les points d'EQ et les changements de matériau figé. L'historique reste disponible lorsque vous fermez puis rouvrez l'éditeur, mais le chargement d'un projet du DAW démarre un nouvel historique. L'automation exécutée par le thread audio ne crée pas une entrée d'annulation à chaque mise à jour de paramètre. Fire conserve jusqu'à 100 modifications dans un budget de 64 Mio de captures d'état ; l'annulation ne change pas la taille actuelle de l'éditeur.

## 05. Chaînes, routage parallèle et Mid/Side

Fire divise l'entrée en une à quatre régions de fréquences. La chaîne de bande sélectionnée traite sa région ; les sorties de bandes sont ensuite réunies, puis traitées par la chaîne Master.

```text
Entrée -> séparation fréquentielle -> chaînes Band -> somme -> chaîne Master -> sortie
                                        |  |  |  |
                                    jusqu'à quatre bandes
```

Band Lab et Master Lab partagent les modules Drive, Shape, Compressor, OTT, Stereo, EQ, Lo-Fi, Chorus, Flanger, Phaser, Delay, Reverb, Granular et Chord Resonator. Les lignes d'origine peuvent être retirées et ajoutées à nouveau, comme les modules supplémentaires. Chaque chaîne dispose de huit emplacements d'insertion indépendants supplémentaires au maximum. Analysis est une page d'affichage Master amovible qui ne traite pas l'audio.

Cliquez sur **+** et choisissez un module. Cliquez sur sa ligne pour le modifier. Faites glisser le nom/le corps de la ligne pour changer l'ordre ; la ligne d'insertion indique la future position. Maintenez le pointeur près du bord de la liste pour faire défiler les lignes masquées. Relâchez pour valider. Escape, un dépôt en dehors de la liste ou un changement d'espace/de bande annule le glissement. Le réordonnancement conserve les paramètres et les routages du module. Les modules audio traitent le signal dans l'ordre affiché ; Output/Mix de la bande intervient après sa chaîne.

### Serial, Parallel, Mid et Side

![Le panneau Routing réunit le choix du canal, le mode Serial/Parallel, l'affectation aux branches et Blend. Chaque branche utilise les modules existants de la chaîne.](images/13-routing-detail.png)

Cliquez sur le bouton compact Stereo/Mid/Side près de CHAIN pour ouvrir Routing.

| Commande | Fonction |
| --- | --- |
| **Serial** | Les modules fonctionnent les uns après les autres. |
| **Parallel A/B** | Les deux branches reçoivent la même entrée de chaîne ; leurs sorties sont mélangées. |
| **To A / To B** | Déplacer un module vers l'autre branche. Le menu de ligne propose aussi **Move to Branch A/B**. |
| **Up / Down** | Modifier l'ordre des modules dans une branche. |
| **Blend** | Pourcentage de la branche B : 0% = A seule, 100% = B seule. |
| **Stereo** | Traiter le signal stéréo. |
| **Mid** | Traiter sa composante centrale en conservant la composante Side d'origine. |
| **Side** | Traiter sa composante de différence en conservant la composante Mid d'origine. |

Les branches parallèles partagent les emplacements de modules existants ; elles ne créent pas un second rack indépendant de huit emplacements. Une branche vide laisse passer l'entrée sans modification. Le mélange est linéaire, donc deux branches identiques conservent un gain unitaire. Les deux branches continuent à fonctionner aux positions extrêmes, ce qui maintient à jour les mémoires de delay/reverb. Les latences des branches sont alignées avant leur réunion ; Output/Mix de la section intervient après celle-ci. Mid/Side englobe toute la chaîne, y compris les deux branches.

Pour une compression parallèle, placez Compressor ou OTT sur B, laissez A vide, puis augmentez Blend à partir de 0% jusqu'à obtenir le détail et la densité adaptés à la source. Pour opposer deux colorations, placez un module Shape différent sur A et sur B. Blend accepte la modulation habituelle et l'automation de l'hôte ; recherchez **Routing / Blend** dans la matrice. Le mode Serial mémorise l'affectation aux branches et Blend pour une utilisation ultérieure. Le routage structurel et l'affectation aux branches sont automatisables par l'hôte, mais ne sont pas des destinations de modulation interne.

Lorsque Mid ou Side est sélectionné, les commandes de sortie de la section qui s'y prêtent agissent sur cette composante. Les effets stéréo sont ramenés par moyenne à la composante sélectionnée, pour que le traitement ne déborde pas sur celle qui doit rester intacte. Avec une entrée mono, Mid agit comme Stereo et Side laisse passer le signal avec la même latence. Choisir **Side** dans l'analyseur affiche seulement les différences ; cela n'active pas ce mode de traitement.

### Copier un module

Faites un clic droit sur une ligne de module, ou donnez-lui le focus et appuyez sur **Shift+F10**.

- **Copy settings** copie le module sans ses routages de modulation vers les destinations.
- **Copy with routing** inclut ces routages.
- **Duplicate settings** insère une copie des seuls réglages immédiatement après la source.
- **Paste ... after** insère le module copié après la ligne choisie. Pour le coller dans une chaîne vide, utilisez l'entrée du module copié dans **+**.

Un module copié conserve son affectation à une branche, mais ne modifie pas le mode de routage, Blend ni le mode Stereo/Mid/Side de la destination. Les presse-papiers des modules et des chaînes sont distincts. Ils peuvent être partagés entre les instances Fire du même format de plug-in au sein d'un même processus.

### Copier ou enregistrer une chaîne complète

Ouvrez le menu **...** de la chaîne. **Copy chain settings** ou **Copy chain with routing** copie les modules audio dans l'ordre. **Paste chain** remplace uniquement la chaîne de destination ; les autres bandes, Master, les séparations, Output/Mix et les sources de modulation globales restent inchangés. Copier une chaîne vide peut effacer les modules audio d'une autre chaîne. Analysis est exclu.

Utilisez **Save chain preset...** pour un fichier `.firechain` contenant seulement les réglages, ou **Save chain preset with routing...** pour inclure les affectations. **Load chain preset - replace...** remplace la chaîne actuelle à partir d'un fichier. Les fichiers de chaîne utilisent le dossier **Chains** à l'intérieur du dossier des préréglages. Ils comprennent le canal/le routage parallèle et les données des modules, notamment les modèles, les points d'EQ et l'audio figé.

« With routing » copie les affectations aux destinations, les plages, les courbes et les affectations de macros de profondeur. Cette option utilise les LFO, enveloppes, macros et configurations MIDI CC actuels de l'instance de destination ; elle ne remplace pas ces définitions de sources. Utilisez un préréglage complet du plug-in si vous avez besoin de l'intégralité du son d'origine.

Fire vérifie qu'un collage complet peut tenir dans des emplacements compatibles avant de modifier la destination. Certains modules d'origine nécessitent leur représentation de base compatible : le Lo-Fi Master d'origine utilise un moteur différent, et un Drive utilisant Legacy Link peut nécessiter son emplacement Drive d'origine ou une conversion vers Drive Comp. Si le collage ne peut pas préserver les réglages ou le routage requis, Fire explique le problème et laisse la destination inchangée.

### Ordre et trois niveaux de Mix

L'ordre influence à la fois le timbre et la dynamique. Un EQ avant la distorsion détermine les fréquences qui l'excitent le plus ; placé après, il façonne les harmoniques déjà produites. Un compresseur avant la distorsion stabilise le niveau qui l'attaque, tandis qu'un compresseur après contrôle le signal résultant. Un delay avant la distorsion peut faire interagir chaque répétition avec celle-ci ; placé après, il répète le son distordu.

Le **Mix** d'un module mélange généralement son entrée avec son signal traité. Dans le trajet adjacent Drive  >  Shape d'origine d'une bande, Shape Mix mélange l'intégralité de l'étage de distorsion commun, Drive compris. **Band Mix** mélange la chaîne complète de la bande avec son signal sec, et le **Mix** global mélange l'effet complet avec son trajet sec. Ces niveaux se cumulent : un Mix de module faible suivi d'un Band Mix faible peut rendre l'effet très discret. **Output** contrôle le niveau plutôt que la quantité de distorsion. Utilisez ce gain de sortie pour équilibrer les niveaux, puis réajustez-le après avoir modifié la chaîne.

> Pour disposer d'une branche parallèle réellement sèche, déplacez ou retirez ses modules de traitement avant de la considérer comme trajet de référence. Sur l'exemple de capture Routing, A contient des modules et n'est donc pas une branche sèche vide. Le Shape de bande d'origine conserve un trajet de compatibilité en mode Serial ; voir le chapitre 06.

## 06. Drive et Shape

Drive et Shape constituent le cœur du travail de distorsion dans Fire. **Drive** augmente le niveau entrant dans l'étage non linéaire. **Shape** choisit la courbe de transfert ou la coloration analogique et ajuste son asymétrie. Dans la chaîne de bande d'origine, des modules Drive  >  Shape adjacents utilisent un trajet de traitement commun. Les séparer en fait deux étages distincts ; les effets insérés entre eux influencent donc le signal qui atteint Shape.

![Warm Triode avec Vintage : illustration matérielle, commandes Shape locales et bande sélectionnée. La luminosité de la lampe réagit au niveau d'entrée et à Drive.](images/04-vintage-tube.png)

### Drive, Safe et compensation

| Commande | Rôle pratique |
| --- | --- |
| Drive | Gain d'entrée sur une échelle de commande de 0 à 100. Une valeur supérieure produit généralement plus de densité et de distorsion, mais le résultat dépend de la source et de Shape. |
| Safe | Limite le drive effectif en fonction des crêtes entrantes. Le drive appliqué peut ainsi être inférieur à la valeur demandée par le potentiomètre. |
| Extreme | Augmente fortement le gain disponible pour un même réglage Drive. Commencez avec une valeur faible lorsque vous l'activez. |
| Gain Comp / Comp | Applique une réduction de niveau estimée d'après le drive réellement utilisé. Facilite les comparaisons tout en laissant Output réglable indépendamment. |
| Output | Réglage manuel du niveau après la chaîne concernée. Ce n'est pas une seconde commande de distorsion. |

Safe gère le drive ; ce n'est pas un limiteur de sortie master. Gain Comp est une estimation, pas une mesure du niveau perçu. Son affichage suit la compensation appliquée ; le symbole d'approximation indique une estimation lorsque les données audio actuelles ne sont pas disponibles. Utilisez Match dans la barre supérieure pour une véritable comparaison de niveau sur l'ensemble de la chaîne.

Les anciens sons peuvent afficher **Legacy Link**. Ce mode conserve leur relation d'origine entre Drive et l'Output final de la bande. **Use Drive Comp** fait explicitement passer la bande à la méthode de compensation plus récente et réactive son réglage Output manuel mémorisé. Cela peut modifier le son. Dans une chaîne parallèle, Legacy Link agit sur l'Output final de la bande, donc sur les deux branches ; la compensation moderne reste associée à Drive.

### Commandes Shape

Choisissez un modèle dans le sélecteur Shape. **Bias** décale le signal par rapport à la courbe de mise en forme pour créer une asymétrie ; sa plage va de -1 à +1. **Rectification** modifie la partie négative de la forme d'onde traitée : 0 conserve sa polarité, 0.5 supprime la demi-onde négative et 1 retourne cette demi-onde vers le positif. Les réglages intermédiaires mélangent ces comportements. Bias et la rectification peuvent introduire une composante continue ; activez donc **DC** ou **DC Filter** si nécessaire. **Mix** mélange le résultat de la mise en forme avec son entrée ; dans le trajet adjacent Drive  >  Shape d'origine d'une bande, il englobe l'étage de distorsion commun complet, Drive compris.

> **Shape de bande d'origine en mode Serial :** son état désactivé conserve la courbe de distorsion sélectionnée pour assurer la compatibilité, tout en désactivant Bias, Rectification et le filtrage DC, et en ignorant le Shape Mix mémorisé. Retirer cette ligne Shape d'origine conserve également ce comportement Serial. Des commandes Shape grisées ne garantissent donc pas un signal propre. Placez Band Mix à zéro ou contournez toute la bande pour une comparaison sèche. Un Shape ajouté indépendamment possède un bypass sec/traité conventionnel, et le mode Parallel A/B contourne la courbe du Shape d'origine lorsqu'il est désactivé ou retiré.

La page analogique d'origine de la bande utilise la commande Drive de cette bande. Les modules Shape analogiques ajoutés indépendamment disposent de leur propre Drive, ce qui permet plusieurs étages de coloration avec des niveaux de drive différents dans une même chaîne. Dans un Shape analogique indépendant, le Drive local commande directement le modèle ; ne confondez pas les interrupteurs d'un module Drive séparé avec des commandes appartenant à cette instance Shape.

**HQ** utilise un trajet de distorsion à fréquence d'échantillonnage supérieure pour réduire l'aliasing indésirable. Son intérêt peut être particulièrement sensible sur les signaux brillants fortement saturés et les réglages de repliement de forme d'onde. Il sollicite davantage le processeur ; écoutez au niveau de qualité que vous comptez utiliser. Une courbe de transfert aide à comprendre une mise en forme statique, mais ne décrit pas tous les phénomènes temporels d'un modèle analogique magnétique ou sensible à l'alimentation.

### Modèles Shape numériques

![Une page Shape numérique affiche sa courbe de transfert et ses commandes d'asymétrie. Le graphique représente une courbe, pas le comportement dynamique complet d'un modèle analogique.](images/06-shape-panel.png)

| Modèle | Caractère et point de départ utile |
| --- | --- |
| Arctan | Courbe arrondie à saturation progressive ; utile pour une coloration mesurée. |
| Exp | Saturation douce exponentielle, avec une transition différente vers l'écrêtage. |
| Tanh | Saturation lisse et symétrique pour épaissir des sources variées. |
| Cubic | Écrêtage polynomial arrondi ; sélection numérique par défaut. |
| Hard | Écrêtage abrupt aux limites de la courbe pour des harmoniques aiguës marquées. |
| Sausage | Courbe ferme avec des angles adoucis près de l'écrêtage. |
| Sin | Repliement sinusoïdal répété ; augmenter Drive peut produire des changements complexes plutôt que simplement plus d'écrêtage. |
| Linear | Repliement de type triangulaire avec des transitions anguleuses. |
| Limit | Écrête dans une plage de niveau étroite. Rééquilibrez le niveau à l'oreille après l'avoir choisi. |
| Single Sin | Utilise une seule région sinusoïdale et supprime les échantillons situés au-delà, produisant des textures inhabituelles à fort drive. |
| Logic | Sélection historique dont la courbe de transfert stable actuelle est équivalente à Tanh. |
| Pit | Courbe saturante comportant une composante soustractive à fort niveau ; utile pour des variations moins conventionnelles à fort drive. |

Les noms décrivent les courbes disponibles dans Fire. Ils ne promettent pas le son d'un processeur de distorsion commercial précis. Pour une comparaison contrôlée, gardez Bias à zéro et Rectification à zéro, choisissez une courbe à la fois et équilibrez le niveau de sortie avant de vous décider.

### Colorations Analog Hardware

![Tape Saturation affiche un transport en mouvement pendant le traitement audio. Son animation accompagne le modèle sélectionné.](images/05-vintage-tape.png)

Ces modèles sont des colorations inspirées de circuits, et non des répliques étalonnées de matériels nommés. Les modèles d'amplificateur intègrent le comportement de l'alimentation et du point de fonctionnement ; Transformer et Tape Saturation intègrent une mémoire magnétique. Leur réponse dépend du niveau et de l'historique de l'audio : une note tenue et une transitoire brève peuvent donc se comporter différemment.

| Modèle | Usage suggéré |
| --- | --- |
| Warm Triode | Chaleur douce et asymétrique de type lampe sur voix, basse ou claviers. |
| Bright Pentode | Coloration à lampes plus présente lorsque la source a besoin de présence. |
| Class A Console | Densité subtile de type console sur des pistes individuelles ou un bus. |
| Tweed Breakup | Saturation de guitare souple et progressive, avec un crunch dynamique. |
| British Crunch | Crunch plus marqué, centré sur les médiums, pour guitares et éléments rythmiques. |
| Modern High Gain | Distorsion dense en cascade pour des timbres agressifs. |
| Diode Overdrive | Overdrive ciblé pour les solos, l'articulation de la basse ou les accents de batterie. |
| Germanium Fuzz | Fuzz asymétrique au caractère plus doux et plus sombre. |
| Silicon Fuzz | Fuzz plus brillant et plus ferme, aux harmoniques aiguës prononcées. |
| MOSFET Drive | Coloration asymétrique entre saturation douce et overdrive plus dur. |
| Transformer | Coloration magnétique et saturation dépendant de l'historique du signal. |
| Tape Saturation | Coloration d'enregistrement magnétique polarisé, avec mise en forme liée à l'enregistrement/la lecture et aux pertes de tête. |

Ces descriptions guident l'écoute sans garantir un résultat timbral. Tape Saturation est un modèle de bande générique ; il ne propose pas de sélecteur de vitesse de bande, de polarisation ou de type de tête. Les commandes distinctes Tape/Wow/Flutter de Lo-Fi ont une autre fonction.

La luminosité de la lampe suit à la fois le signal entrant et Drive : le silence ne l'allume donc pas simplement parce que Drive est élevé. Les bobines de bande réagissent à l'activité audio et s'immobilisent lorsque le traitement cesse. L'illustration indique l'activité ; ce n'est pas un instrument de mesure électrique étalonné. Paper et Ink utilisent des versions schématiques de ces affichages ; le son reste identique.

## 07. Compressor et OTT

### Compressor

Compressor réduit le signal au-dessus de **Threshold** selon **Ratio**. **Attack** définit la vitesse de réaction de la réduction de gain et **Release** sa vitesse de retour. Une attaque plus lente préserve souvent davantage l'impact initial ; une attaque rapide le contrôle davantage. Commencez avec un Ratio d'environ 2:1 à 4:1, baissez Threshold jusqu'à entendre la compression, puis ajustez les temps en écoutant le rythme.

| Commande | Compressor de bande d'origine | Instance Compressor supplémentaire |
| --- | --- | --- |
| Threshold | -48 à 0 dB | -60 à 0 dB |
| Ratio | 1:1 à 20:1 | 1:1 à 20:1 |
| Attack | 0.1-200 ms | 0.1-200 ms |
| Release | 10-2000 ms | 1-1000 ms |
| Mix | Du signal sec au signal entièrement traité | Du signal sec au signal entièrement traité |

Le module de bande d'origine conserve ses plages historiques ; une instance ajoutée dans Band ou Master utilise les plages d'insertion ci-dessus. Un Ratio de 1:1 ne produit aucune compression. Compressor ne possède pas de potentiomètre dédié de compensation de gain : utilisez une commande Output appropriée ou l'organisation de votre chaîne pour rétablir le niveau. Réduisez Mix pour une compression parallèle au sein du module, ou utilisez les branches parallèles de la chaîne pour traiter séparément les trajets sec et comprimé.

### OTT : relever les détails faibles et contenir les crêtes fortes

Le module de bande d'origine nomme ses seuils **Up Thresh** et **Down Thresh**, et son gain de sortie **Gain**. Les OTT ajoutés indépendamment utilisent **Up**, **Down** et **Output** pour les mêmes fonctions.

OTT combine compression ascendante et descendante. Il relève les signaux faibles sous **Up / Up Thresh**, réduit les signaux forts au-dessus de **Down / Down Thresh** et affecte moins la région intermédiaire. La séparation multibande de Fire assure toute division fréquentielle : une instance OTT Master traite l'intégralité du signal qui lui parvient, sans créer en arrière-plan un autre ensemble de bandes de séparation.

![L'OTT Master utilise la même vue LIFT / PRESS que l'OTT de bande. L'entrée, la variation de gain et l'activité dynamique affichées proviennent de l'instance OTT sélectionnée.](images/09-ott-panel.png)

| Commande | Plage et fonctionnement |
| --- | --- |
| Depth | Intensité globale de la dynamique, de zéro au maximum. Le module de bande natif affiche 0-1 ; les instances ajoutées affichent un pourcentage. |
| Time | 10-400% ; met à l'échelle ensemble l'attaque et le relâchement. 100% correspond à 5 ms d'attaque et 100 ms de relâchement. |
| Up / Up Thresh | -72 à -6 dB ; seuil des signaux faibles. Reste au moins 6 dB sous Downward. |
| Down / Down Thresh | -36 à 0 dB ; seuil des signaux forts. |
| Output / Gain | -24 à +24 dB ; réglage de niveau après la dynamique. |
| Mix | Du signal sec au signal entièrement traité. |

Les rapports ascendant et descendant sont fixés à 4:1 et 8:1, avec des transitions douces. Le gain ascendant est limité et disparaît progressivement près du bruit de fond ; OTT n'est pas un générateur de bruit. Les deux canaux partagent son détecteur et son gain, ce qui préserve la relation stéréo.

L'affichage **LIFT / PRESS** montre l'activité du module sélectionné. Des rubans qui se soulèvent indiquent la compression ascendante ; le mouvement de pression indique la compression descendante. Chaque OTT ajouté indépendamment possède ses propres mesures, y compris les instances dans Master Lab. L'animation dynamique exclut le réglage Output ; tourner uniquement Output ne traduit donc pas une compression plus forte.

Pour l'OTT de bande d'origine, les lignes de seuil du spectre permettent aussi un réglage direct. Faites glisser une ligne verticalement, double-cliquez pour la réinitialiser ou utilisez les touches fléchées pour des ajustements précis lorsqu'elle a le focus. Commencez avec un Depth modéré et réduisez-le si l'ambiance de la pièce ou le bruit devient trop présent. Les valeurs Time inférieures à 100% accentuent la rapidité de réaction ; les valeurs plus longues ralentissent le mouvement.

## 08. Stereo

Stereo comprend **Width**, **Pan** et **Mix**. Width modifie l'équilibre entre les composantes centrale et de différence d'un signal stéréo. Sa position neutre est **0.50** dans le module de bande d'origine, ou **50%** dans les instances ajoutées. Des valeurs inférieures favorisent le centre ; des valeurs supérieures favorisent les côtés. Aux extrêmes, la composante opposée est supprimée : 100% ne signifie donc pas simplement « stéréo normale à largeur maximale ».

Pan atténue le canal opposé lorsque vous le déplacez à gauche ou à droite. Mix mélange le traitement stéréo avec l'entrée du module. L'effet a besoin d'informations stéréo pour élargir : un signal centré sans composante de différence ne peut pas acquérir des détails latéraux indépendants avec Width seul. Un bus de traitement mono contourne cette opération stéréo.

Vérifiez les réglages larges avec le spectre sur **Mono** et comparez à l'oreille en mono. Un élargissement obtenu en accentuant le signal de différence peut rendre la somme mono beaucoup plus faible. Réduisez Width, augmentez la part sèche du Mix du module ou gardez la bande grave plus proche de sa largeur neutre.

## 09. EQ et EQ dynamique

Chaque EQ accepte jusqu'à **12 points**. Sélectionnez un point numéroté pour modifier **Frequency**, **Gain**, **Q**, **Filter Type** et, lorsque cela s'applique, **Slope**. Utilisez **+** pour ajouter un point et **-** pour le supprimer. Chaque point dispose de sa propre commande d'activation. L'EQ peut être modifié lorsque son module est contourné ; ajouter un point active le module pour l'écoute.

![EQ Master : points du spectre et commandes du point sélectionné. Utilisez la navigation entre les points et le menu de type pour modifier le filtre voulu.](images/07-master-eq.png)

### EQ statique

| Type | Utilisation |
| --- | --- |
| Bell | Renforcer ou atténuer autour d'une fréquence choisie. |
| Low cut | Retirer le contenu grave sous la fréquence de coupure. |
| High cut | Retirer le contenu aigu au-dessus de la fréquence de coupure. |
| Low shelf | Ajuster une large région de basses fréquences. |
| High shelf | Ajuster une large région de hautes fréquences. |
| Notch | Supprimer une région fréquentielle étroite ; Gain est indisponible. |
| Band pass | Conserver une région autour de la fréquence centrale ; Gain est indisponible. |

Les filtres de coupure proposent des pentes de **12, 24, 36 ou 48 dB/oct**. Q règle la largeur de bande ou la résonance. Gain s'étend de -24 à +24 dB. Frequency couvre 20 Hz-20 kHz, la réponse utilisable dépendant aussi de la fréquence d'échantillonnage de l'hôte. Les points d'EQ insérés proposent Q 0.1-20. Dans l'EQ Master d'origine, les trois premiers points conservent Q 1-5, tandis que les suivants proposent Q 0.1-18.

L'EQ Master d'origine permet aussi l'édition sur le spectre. Double-cliquez dans un espace vide du spectre pour ajouter un filtre Bell à cet endroit, faites glisser un point pour changer fréquence/gain et utilisez la molette au-dessus pour ajuster Q. Delete/Backspace supprime le point sélectionné lorsque ce spectre a le focus. Ces gestes appartiennent à la vue d'édition de l'EQ Master ; ne les confondez pas avec l'ajout de séparations dans le spectre des bandes.

### EQ dynamique

<!-- figure:max-height=165 -->
![Les commandes dynamiques appartiennent au point Bell ou Shelf sélectionné. Range détermine le sens et l'amplitude maximale de la variation de gain.](images/08-dynamic-eq.png)

Sélectionnez **Bell**, **Low shelf** ou **High shelf**, puis ouvrez **Dynamic** dans l'en-tête du point et activez-le. Les autres types de filtre ne proposent pas de dynamique.

| Commande | Signification |
| --- | --- |
| Threshold, -60 à 0 dB | Niveau au-dessus duquel le détecteur demande une variation de gain. |
| Ratio, 1:1-20:1 | Intensité de la réponse dépendante du niveau ; 1:1 ne produit aucun changement. |
| Range, -24 à +24 dB | Variation de gain maximale signée : une valeur négative atténue, une valeur positive renforce. |
| Attack, 0.1-200 ms | Vitesse de réaction à une augmentation du niveau. |
| Release, 5-2000 ms | Vitesse de retour. |

Le détecteur écoute l'entrée d'origine du module EQ, filtrée autour de la fréquence du point, en utilisant la crête la plus forte entre gauche et droite. Ce n'est pas un sélecteur de sidechain externe, et les points précédents du même EQ ne l'alimentent pas en série. La somme du Gain de base et du gain dynamique reste comprise dans ±24 dB.

Observez les indications de variation de gain dynamique et de gain effectif pendant la lecture audio. La courbe en temps réel de l'EQ Master d'origine suit le gain effectif, mais son point déplaçable et son potentiomètre Gain conservent le réglage de base. Les réglages dynamiques acceptent l'automation de l'hôte, les préréglages, les verrouillages de paramètres et la copie de chaîne ; ils ne sont pas des destinations de modulation interne de Fire. Frequency, Gain et Q conservent leur prise en charge habituelle de la modulation.

## 10. Chorus, Flanger et Phaser

Ces effets ajoutent un mouvement périodique avec leurs propres balayages internes. Leurs potentiomètres Rate sont en hertz ; ne confondez pas ces oscillateurs internes avec la banque de LFO distincte. Le système de modulation de Fire peut néanmoins faire varier les commandes exposées de l'effet.

| Effet | Commandes et plages |
| --- | --- |
| Chorus | Rate 0.05-8 Hz ; Depth 0-100% ; Delay 5-30 ms ; Feedback -75 à +75% ; Width 0-100% ; Mix 0-100%. |
| Flanger | Rate 0.05-8 Hz ; Depth 0-100% ; Delay 0.1-10 ms ; Feedback -90 à +90% ; Width 0-100% ; Mix 0-100%. |
| Phaser | Rate 0.05-8 Hz ; Depth 0-100% ; Center 40-6000 Hz ; Feedback -85 à +85% ; Width 0-100% ; Mix 0-100%. |

Chorus utilise un court retard variable pour épaissir le son et créer un mouvement de hauteur. Flanger utilise un retard balayé plus court pour créer un motif métallique en peigne lorsqu'il est mélangé au signal sec. Phaser utilise une chaîne de six étages passe-tout pour un balayage arrondi d'encoches mobiles.

Depth définit l'amplitude du balayage. Delay règle le retard central de Chorus/Flanger ; Center règle la région fréquentielle du balayage de Phaser. Feedback renforce la résonance ; les valeurs négatives apportent un caractère différent. Width décale les balayages entre les canaux, tandis que zéro les aligne. Mix détermine l'interaction entre les signaux sec et traité.

Commencez autour de 50% de Mix pour des annulations marquées avec Flanger/Phaser. Un Rate lent et un Depth modéré conviennent aux nappes ; un Rate plus rapide peut animer les percussions ou les claviers. À 100% de signal traité, le caractère peut changer sensiblement, car le signal sec ne participe plus aux annulations.

## 11. Delay

Delay propose **Time**, **Feedback**, **Tone**, **Ping-Pong**, **Sync** et **Mix**. En mode de synchronisation **Free**, Time couvre **10-2000 ms**. Les choix Sync sont **1/16, 1/8, 1/8 D, 1/4, 1/4 D, 1/2 et 1 Bar**, D indiquant une valeur pointée. Les temps synchronisés suivent le tempo de l'hôte, dans la limite de capacité du delay de deux secondes. À tempo lent, une division longue peut atteindre cette limite au lieu de sa durée nominale complète.

Feedback, de 0 à 90%, contrôle les répétitions. Tone, de 500 Hz à 18 kHz, filtre la boucle de réinjection : les valeurs plus basses assombrissent les répétitions suivantes. Ping-Pong, de 0 à 100%, passe d'un delay stéréo ordinaire à une alternance entre les canaux. Mix règle le niveau des échos par rapport à l'entrée sèche.

Pour un slap vocal, utilisez le mode Free autour de 80-130 ms, un Feedback faible et un Mix réduit. Pour un mouvement rythmique, choisissez une croche pointée et un Feedback modéré, puis baissez Tone si les répétitions concurrencent l'original. Sur une piste de retour dédiée, un Mix à 100% est un point de départ utile ; en insertion, commencez beaucoup plus bas.

## 12. Reverb

Sélectionnez **Classic, Room, Hall, Plate, Spring ou Chamber**. Tous les modèles utilisent les mêmes six commandes : vous pouvez donc les comparer sans apprendre un panneau différent à chaque fois.

![Une Reverb insérée propose un menu d'algorithme et les commandes Size, Damping, Pre-delay, Width, Low Cut et Mix.](images/10-reverb-panel.png)

| Commande | Signification |
| --- | --- |
| Size, 0-100% | Modifie le caractère de l'espace/de la décroissance. Ce n'est pas un affichage étalonné du temps de réverbération. |
| Damping, 0-100% | Les valeurs plus élevées assombrissent la queue réinjectée. |
| Pre-delay, 0-200 ms | Retard avant le signal réverbéré, aidant à séparer la source de sa queue. |
| Width, 0-100% | Étendue stéréo de la réverbération. |
| Low Cut, 20-1500 Hz | Retire l'énergie grave entrant dans la réverbération. |
| Mix, 0-100% | Équilibre entre signal sec et réverbéré. |

**Classic** conserve le caractère de la réverbération d'origine de Fire. **Room** est un bon point de départ pour une ambiance compacte et des premières réflexions audibles. **Hall** convient à des espaces plus vastes et évolutifs. **Plate** forme une queue dense pour les voix, les caisses claires ou les sons tenus. **Spring** ajoute une coloration dispersive de type ressort, utile avec la guitare et en création sonore. **Chamber** propose un autre motif de premières réflexions et de décroissance, entre textures intimes et vastes. Il s'agit d'espaces algorithmiques, pas de captures par convolution de pièces ou de matériels précis.

Changer de modèle efface la mémoire de retard du modèle précédent avec une transition par le signal sec ; ne vous attendez donc pas à ce que l'ancienne queue continue à l'identique. Pour clarifier une voix, essayez d'augmenter Pre-delay et Low Cut avant de simplement réduire Mix. Pour conserver l'impact d'une batterie, choisissez un Size ressenti comme plus court et utilisez la réverbération avec parcimonie ou sur une branche parallèle.

## 13. Lo-Fi

Lo-Fi combine réduction de fréquence d'échantillonnage, quantification et mouvement de type bande. **Rate** est un facteur de réduction : les valeurs plus élevées maintiennent les échantillons plus longtemps, créant un caractère plus rugueux de fréquence réduite. **Bits** réduit la résolution en amplitude. **Jitter** fait varier le moment du maintien des échantillons plutôt que d'ajouter une piste de bruit séparée.

**Tape** ajoute de la saturation et adoucit les hautes fréquences. **Wow** ajoute une lente dérive de hauteur ; **Flutter** ajoute une variation plus rapide. **Mix** mélange le résultat Lo-Fi complet. Commencez avec Tape/Wow/Flutter à zéro pour entendre clairement la réduction de fréquence et de résolution, puis ajoutez une coloration à la fois.

Le Lo-Fi Master d'origine conserve Rate **1-64** et Bits **4-32**. Les instances Lo-Fi ajoutées utilisent Rate **1-32** et Bits **4-24**, Tape, Wow, Flutter et Jitter allant chacun de zéro à leur intensité maximale. Les commandes natives peuvent afficher des valeurs normalisées de 0 à 1 là où les commandes insérées affichent des pourcentages ; leur rôle en création sonore reste identique.

Pour des claviers usés, utilisez une réduction de fréquence légère, un Bits assez élevé, un peu de Tape et un Wow lent. Pour une batterie granuleuse, baissez d'abord Bits, puis augmentez Rate et Jitter jusqu'à obtenir le grain d'attaque souhaité. Tape dans Lo-Fi est une commande de coloration compacte ; le modèle dédié **Shape  >  Tape Saturation** est le modèle magnétique plus détaillé.

## 14. Chord Resonator

Chord Resonator excite une banque harmonique accordée à partir de l'audio entrant. Il fonctionne particulièrement bien avec les percussions, le bruit et les signaux riches en harmoniques. Il ne détecte pas la tonalité du morceau et ne réaccorde pas des notes quelconques selon une gamme.

Choisissez **Root**, de **C2 à C5**, et un **Chord** : **Major, Minor, Major 7, Minor 7, Sus 2, Sus 4, Fifth ou Octave**. L'affichage des notes indique la disposition de l'accord. Dans la nomenclature de Fire, le do central est C4.

**Color** équilibre la fondamentale et les harmoniques supérieures. **Decay** règle la durée nominale de décroissance de 60 dB, de **0.05 à 3 secondes**. **Width** répartit les voix dans la stéréo ; à zéro, chaque voix reçoit la même pondération gauche/droite, tout en conservant les canaux stéréo entrants. **Mix** équilibre l'attaque de la source et la résonance de l'accord.

Sur une boucle de percussion, commencez avec le C3 Minor 7 par défaut, un Decay modéré et un Mix faible. Choisissez un Root adapté au morceau. Placez le module dans une bande médium/aiguë si vous souhaitez garder les fondamentales graves directes. Aucune entrée MIDI n'est nécessaire : l'excitation est audio. Sans nouvel audio, seule la queue existante continue.

## 15. Granular, Freeze et fichiers d'échantillons

Granular utilise l'adaptation par Fire du **mode granulaire normal** de Mutable Instruments Clouds. Il enregistre l'audio entrant dans une courte mémoire circulaire et en lit des fragments superposés. Le moteur utilise de l'audio **stéréo 32 kHz, 16 bits**, avec conversion depuis et vers la fréquence de l'hôte. Cela participe à sa texture ; le signal sec transparent est conservé séparément.

![La page Granular développée contient les commandes Clouds, Freeze et l'accès à Sample.](images/11-granular-panel.png)

### Commandes des grains

| Commande | Fonctionnement |
| --- | --- |
| Position | Sélectionne la région de l'enregistrement ; le sens horaire se déplace vers l'audio le plus ancien. |
| Size | Durée des grains, 0-100%. |
| Pitch | Transpose les grains de -24 à +24 demi-tons. |
| Density | Le côté négatif programme des grains réguliers ; le côté positif les rend aléatoires. Le centre ne produit aucun nouveau grain. |
| Texture | Fait évoluer l'enveloppe des grains d'abrupte à plus douce ; les valeurs supérieures ajoutent de la diffusion. |
| Spread | Répartition stéréo des grains. |
| Feedback | Réinjecte la sortie granulaire dans le trajet de traitement. |
| Reverb | Ajoute l'ambiance propre au moteur Clouds. |
| Mix | Équilibre entre l'audio sec entrant et la sortie granulaire. |
| Freeze | Interrompt la mise à jour de l'enregistrement tandis que les grains continuent à jouer. |

Si Freeze est actif mais qu'aucun son ne sort, vérifiez qu'un enregistrement existe, que Density n'est pas au centre et que Mix est supérieur à zéro. Freeze conserve le matériau ; il ne maintient pas automatiquement chaque grain ou queue de réverbération dans un état exactement identique. Si l'enregistrement est vide, le module capture de l'audio frais avant de le figer.

### Capturer une texture en direct

1. Faites passer une courte source dans le module Granular avec Freeze désactivé.
2. Éloignez Density du centre et augmentez suffisamment Mix pour entendre les grains.
3. Appuyez sur **Freeze** au moment à conserver.
4. Explorez Position, Size et Pitch pendant la lecture du matériau figé.
5. Ajoutez un peu de Feedback ou de Reverb pour une texture plus soutenue.
6. Enregistrez le préréglage ou le projet de l'hôte pour conserver l'enregistrement figé avec le son.

Les enregistrements figés terminés sont intégrés aux préréglages, à l'état du projet, aux captures d'état A/B et aux fichiers portables de modules/chaînes. Le rechargement restaure l'enregistrement, y compris son contenu stéréo et sa position dans la mémoire circulaire ; la programmation des grains et les mémoires de delay/reverb redémarrent. L'hôte doit continuer à traiter l'audio pour qu'une texture figée puisse jouer.

### Importer un fichier

Cliquez sur **Sample** près de Freeze pour ouvrir **Granular / Sample**, puis sur **Choose audio**. Choisissez un fichier mono ou stéréo pris en charge dans le sélecteur de fichiers ; les formats décodables dépendent de la compilation/de la plateforme. Les fréquences d'échantillonnage source de **8-384 kHz** sont acceptées. Un matériau mono est copié dans les deux canaux.

![Après le choix d'un fichier, Sample présente la fenêtre sélectionnée et active Import and Freeze. Normalize est facultatif et initialement désactivé.](images/12-granular-import.png)

La durée maximale d'enregistrement est de **1.02175 seconde** : **32 696 trames stéréo à 32 kHz**. Cette limite n'augmente pas avec la fréquence d'échantillonnage de l'hôte. Pour une source plus longue, déplacez la commande de début ou saisissez un instant de début, puis examinez la forme d'onde de l'extrait sélectionné. La forme d'onde est un aperçu visuel de la fenêtre importée, pas un bouton de préécoute distinct.

**Normalize to -1 dBFS** est désactivé par défaut. Activez-le pour ajuster une crête stéréo commune après conversion, sans modifier l'équilibre gauche/droite. Laissez-le désactivé pour préserver l'amplitude ; les crêtes de la source dépassant la plage de destination peuvent être écrêtées. Cliquez sur **Import and Freeze** lorsque l'extrait est prêt. Fire remplace uniquement l'enregistrement de ce module et active Freeze, avec une brève transition du signal traité pendant la lecture.

L'importation constitue une seule étape Undo. Undo peut restaurer un enregistrement figé précédent et ses réglages. Si le module enregistrait auparavant en direct, Undo rétablit le mode direct mais ne peut pas reconstituer l'ancien audio circulaire. L'enregistrement importé est intégré ; le fichier source n'a donc pas besoin de rester sur le disque. Fermer l'éditeur ou remplacer le module cible annule une importation en attente au lieu de la rediriger ailleurs.

### Exporter le matériau figé

<!-- figure:max-height=215 -->
![Un enregistrement capturé en direct peut être exporté depuis Sample. Start, Normalize et Import and Freeze restent indisponibles tant qu'aucun fichier à importer n'a été sélectionné.](images/12-granular-sample.png)

Avec un enregistrement figé valide, ouvrez Sample et cliquez sur **Export frozen WAV**. Choisissez une destination et confirmez le remplacement si le fichier existe déjà. Fire capture l'enregistrement avant d'ouvrir le sélecteur de destination ; les changements ultérieurs de paramètres ne modifient donc pas l'instantané exporté.

Le WAV contient de l'audio **stéréo 16 bits / 32 kHz** dans l'ordre chronologique. Il exporte l'enregistrement lui-même, **sans** programmation des grains, Pitch, Feedback, Reverb, Mix ni normalisation supplémentaire à l'exportation. Une normalisation appliquée lors de l'importation fait déjà partie de l'enregistrement stocké. Pour conserver une performance granulaire traitée, enregistrez ou effectuez le rendu de la sortie de Fire dans l'hôte. Un enregistrement capturé plus court exporte ses trames disponibles ; il n'est pas artificiellement prolongé jusqu'à la capacité maximale.

Les modes Clouds alternatifs de time-stretch, de delay en boucle et spectral ne sont pas inclus. L'entrée de déclenchement matérielle et les modes basse fidélité alternatifs ne le sont pas non plus. Freeze associé à la modulation permet de nombreuses variations dans le mode granulaire normal fourni.

## 16. Routages de modulation et macros

Fire propose jusqu'à 16 LFO, quatre enveloppes, quatre macros et quatre sources MIDI CC. La matrice de modulation relie ces sources aux commandes de destination compatibles. Plusieurs routages peuvent contrôler une même destination ; leurs contributions s'additionnent et le résultat final est limité à la plage du paramètre.

### Première affectation d'un LFO

1. Ouvrez **Mod Forge** et sélectionnez un LFO dans la liste de gauche. Utilisez **+** si la banque est vide.
2. Réglez son Rate. Activez **BPM** pour des divisions synchronisées au tempo de l'hôte, ou laissez-le désactivé pour une vitesse libre.
3. Cliquez sur **Assign**, puis sur la commande de destination dans Band Lab ou Master Lab. Vous pouvez aussi faire un clic droit sur un potentiomètre compatible et choisir une entrée **Add modulation source**.
4. Ouvrez la commande de grille/Matrix et examinez le routage. Ajustez Depth et choisissez **Bi** ou **Uni**.
5. Observez la plage de la couleur de la source autour du potentiomètre. L'encoche neutre indique sa valeur de base ; un point mobile évidé indique la valeur modulée.

**Add routing** dans la matrice crée une autre connexion. Une ligne contient Source, Destination, Depth, polarité, activation/bypass et suppression. Contourner un routage le conserve mais interrompt sa contribution. Faites un clic droit sur un potentiomètre affecté pour accéder à **Edit modulation in matrix** ou **Clear all modulation**. Un badge de source à plusieurs routages ou à plage personnalisée ouvre les routages concernés.

Cliquez sur Destination pour ouvrir un sélecteur avec recherche. Recherchez par commande, module, identifiant de paramètre ou bande, et filtrez par section/module. **Active chain** met en avant les modules actuels ; **All targets** inclut les modules absents et les bandes inactives. Une affectation indisponible peut rester mémorisée, mais l'afficher n'insère ni n'active le module. **Show control** ouvre la page cible et donne le focus au potentiomètre. Dans le sélecteur, Down passe de la recherche aux résultats, Enter sélectionne et Escape annule.

<!-- pagebreak -->

### Référence des affectations de routage

| Champ | Signification |
| --- | --- |
| **Depth** | -100% à +100% ; met à l'échelle les décalages de l'affectation. |
| **Bi** | Décalages bipolaires standards, de -50% à +50% de la plage normalisée de la cible. |
| **Uni** | Décalages unipolaires standards, de 0% à +100%. |
| **From / To** | Décalages personnalisés par rapport à la valeur de base, chacun de -100% à +100%. Modifier l'un d'eux active une plage personnalisée. |
| **Curve** | Linear, Ease in, Ease out ou S curve, appliquée avant l'interpolation entre les extrémités. |
| **Depth via** | Fixed, ou une macro qui fait varier le Depth du routage de zéro à sa pleine intensité. |

Les décalages sont des pourcentages de la **plage normalisée** du paramètre, pas nécessairement un nombre linéaire de Hz ou de dB. Par exemple, From -20% et To +30% à 100% de Depth couvrent ces décalages par rapport à la valeur de base du potentiomètre. À 50% de Depth, les deux décalages sont divisés par deux. Cliquer sur Bi/Uni rétablit ses extrémités standards. Une macro Depth via met à l'échelle le routage ; elle n'en remplace pas la source. Réglez cette macro au-dessus de zéro pour entendre le routage.

Les commandes de sources ne peuvent pas se moduler elles-mêmes de manière récursive. Le mode de routage et les affectations structurelles des modules ne sont pas des destinations ordinaires de modulation interne. Pour faire varier un équilibre parallèle, ciblez Blend plutôt que le mode de routage.

<!-- figure:landscape -->
![La matrice affiche les sources, les destinations et les commandes d'affectation. Plusieurs lignes peuvent cibler un même paramètre ; leurs contributions s'additionnent.](images/15-modulation-matrix.png)

## 17. Formes de LFO et lecture

![Mod Forge avec une véritable forme triangulaire modifiable. Sélectionnez un LFO dans la liste, modifiez sa courbe, puis affectez des destinations.](images/14-mod-forge-panel.png)

Fire démarre avec quatre LFO et en accepte jusqu'à 16. Sélectionnez-en un dans la liste défilante LFO BANK. Supprimer une source efface ses routages, sans renuméroter les autres sources. Un emplacement réutilisé repart avec de nouveaux réglages de forme/temps. Tous les LFO peuvent être supprimés ; les enveloppes, macros et MIDI CC restent disponibles.

L'éditeur propose les modes **Edit** et **Brush**. En mode Edit, double-cliquez dans un espace vide pour ajouter un point, faites glisser un point pour le déplacer ou double-cliquez sur un point pour le supprimer. Les premier et dernier points ne peuvent pas être supprimés. Faites glisser un segment pour incurver sa courbe. Shift-glisser crée un rectangle de sélection ; déplacer un point sélectionné déplace toute la sélection. Maintenez Cmd sur macOS ou Ctrl sur Windows pendant le glissement pour aligner sur la grille. Les commandes de grille X/Y définissent les divisions.

Faites un clic droit dans l'éditeur pour accéder à **Select All**, **Clear**, **Copy**, **Paste**, **Invert Horizontally** et **Invert Vertically**. Clear rétablit la forme droite par défaut. Le mode Brush dessine un segment prédéfini sélectionné avec le bouton gauche de la souris ; les choix comprennent Saw Up, Saw Down, Sine Convex, Sine Concave, Square High et Square Low.

Rate définit la vitesse, BPM choisit la synchronisation, Smooth adoucit la forme dessinée et Phase décale sa position de lecture. Le repère mobile suit la phase réellement utilisée pour l'audio. Il peut continuer lorsque le transport de l'hôte est arrêté si les appels de traitement audio continuent, et se fige/s'estompe lorsque ces appels cessent.

<!-- pagebreak -->

### Lecture et mouvements aléatoires reproductibles

Cliquez sur **Playback** pour le LFO sélectionné.

| Réglage | Comportement |
| --- | --- |
| **Loop** | Lecture continue libre ou synchronisée au tempo. |
| **Retrigger** | Un message MIDI Note On correspondant ou un Trigger manuel redémarre le cycle, qui continue ensuite en boucle. |
| **One-shot** | Un cycle par déclenchement, puis maintien de la valeur finale. |
| **Shape** | Lecture de la forme dessinée ; Smooth agit sur ce mode. |
| **Random Hold** | Passages par paliers entre des valeurs aléatoires. |
| **Random Glide** | Interpolation entre des valeurs aléatoires. |

Choisissez une Trigger Note de 0-127 ou **Any**, et un canal de 1-16 ou **Omni**. Note Off ne déclenche rien. **Trigger** démarre manuellement Retrigger ou One-shot sans nécessiter de MIDI entrant. Seed accepte 0-65535 ; la même graine et la même séquence de déclenchements reproduisent la séquence aléatoire. Chaque déclenchement avance dans la séquence aléatoire au lieu de repartir systématiquement d'une première valeur identique.

Le dessin et Smooth ne concernent que Shape. Un One-shot aléatoire ignore Phase : Hold conserve une valeur aléatoire, tandis que Glide réalise une transition. Un One-shot Shape commence à Phase et joue tout de même un cycle complet. Les réglages de lecture et les graines sont enregistrés avec le son ; un geste Trigger manuel ne l'est pas.

<!-- figure:max-height=220 -->
![Playback règle Loop/Retrigger/One-shot, les formes Shape ou aléatoires, le filtrage du déclenchement MIDI et la graine aléatoire.](images/17-lfo-playback.png)

## 18. Enveloppes et sidechain externe

Ouvrez **Matrix  >  Envelopes / Macros**, puis sélectionnez **ENV 1-4**. Chaque détecteur possède une entrée, des filtres, Attack, Release et Sensitivity indépendants.

![Chaque ENV possède une entrée de détecteur et un filtrage indépendants. Cet exemple sélectionne External sidechain ; l'hôte doit réellement lui envoyer un signal.](images/16-envelopes-macros.png)

| Commande | Plage / utilisation |
| --- | --- |
| **Detector** | Main input, External sidechain ou Band 1-4. |
| **High Pass** | 20 Hz-20 kHz, avec son propre interrupteur d'activation ; 80 Hz par défaut, contourné. |
| **Low Pass** | 20 Hz-20 kHz, avec son propre interrupteur d'activation ; 12 kHz par défaut, contourné. |
| **Attack** | 0.1-500 ms ; 10 ms par défaut. |
| **Release** | 5-2000 ms ; 200 ms par défaut. |
| **Sensitivity** | -24 à +24 dB ; 0 dB par défaut. |
| **Hold to listen** | Écoute momentanée du signal de détecteur filtré sélectionné. |

Les sources principale et de bande sont prélevées avant les effets. Les détecteurs de bande suivent les séparations actuelles avec leur propre filtrage à 24 dB/oct, avant le solo et les gains de sortie. Une bande inactive ou un sidechain déconnecté est silencieux. La détection d'énergie stéréo reste sensible à une entrée de polarités opposées. Les filtres du détecteur façonnent ce que l'enveloppe entend ; ce ne sont pas des commandes d'EQ du son traité.

Pour faire réagir la coloration à l'interprétation, sélectionnez Main input, réglez une Attack rapide et un Release modéré, puis routez cet ENV vers un Shape Mix ou Drive avec une faible profondeur positive. Augmentez Sensitivity jusqu'à ce que le jeu plus doux produise un mouvement utile. Ajustez Release pour un retour souple entre les notes, puis affinez Depth pour éviter un effet excessif.

Pour un contrôle externe, envoyez une autre piste vers l'entrée sidechain de Fire dans votre hôte, choisissez **External sidechain** et affectez cet ENV dans la matrice. Un routage à profondeur négative vers une commande de niveau ou de Mix adaptée peut l'atténuer lorsque le sidechain augmente. Le routage exact dépend du DAW et du format de plug-in ; sélectionner simplement External sidechain ne crée pas de connexion dans l'hôte.

Maintenez **Listen** avec la souris, ou Space/Return lorsque le bouton a le focus, pour vérifier le détecteur filtré. Relâchez pour revenir à la sortie normale. Masquer/fermer le panneau met également fin à l'écoute. Il s'agit d'un état de préécoute temporaire, qui n'est ni enregistré ni automatisé.

Le premier détecteur peut apparaître sous le nom **Envelope** dans un menu de sources, tandis que les onglets de commandes utilisent **ENV 1**. Les autres détecteurs sont des sources distinctes, pas des copies d'ENV 1.

## 19. Contrôle MIDI

Ouvrez **Mod Forge  >  MIDI**. Chacun des quatre emplacements peut écouter un CC 0-127 sur un canal donné ou en Omni. **Off** désactive l'emplacement.

1. Routez le MIDI de votre contrôleur/piste vers Fire dans l'hôte.
2. Cliquez sur **Learn** pour un emplacement et déplacez la commande matérielle souhaitée. Fire capture son numéro de CC et son canal. Cliquez à nouveau sur Learn, ou fermez le panneau, pour annuler.
3. Ouvrez Matrix et choisissez la source **MIDI CC 1-4** correspondante pour une destination.
4. Réglez sa plage, sa courbe et sa profondeur comme pour un routage LFO. Utilisez Omni si le même contrôleur doit fonctionner sur n'importe quel canal.

![Les quatre emplacements MIDI CC se configurent explicitement ou avec Learn. Ils deviennent des sources de modulation lorsque l'hôte transmet du MIDI à Fire.](images/18-midi-controls.png)

La configuration MIDI CC est enregistrée ; les positions entrantes des contrôleurs sont des données d'interprétation en direct. L'hôte doit transmettre le MIDI à Fire. Son identité d'effet AU est conservée ; certains hôtes AU ne peuvent donc pas lui envoyer de MIDI. Utilisez VST3 ou CLAP lorsque l'hôte accepte le routage MIDI vers les effets, ou utilisez l'automation normale des paramètres. Une note MIDI peut déclencher un LFO, mais ne transforme pas Fire en synthétiseur.

## 20. Recettes pratiques de création sonore

Ces recettes sont des points de départ, pas les spécifications de préréglages d'usine. Réduisez d'abord la source à un niveau d'écoute raisonnable, puis équilibrez le niveau perçu après traitement. Sélectionnez la bande voulue avant de modifier les réglages et vérifiez que son module est actif.

### Guitare : saturation sensible au toucher

1. Commencez avec une bande et placez Drive avant Shape.
2. Sélectionnez **Tweed Breakup**, puis commencez avec un Drive modéré, Safe actif et Extreme désactivé.
3. Gardez Bias et Rectification près de zéro pendant la recherche du caractère de base. Ajustez Drive en jouant doucement puis fortement.
4. Ajoutez un EQ avant Shape si les notes graves dominent la distorsion, ou après si le timbre généré est trop sombre ou agressif.
5. Ajoutez une Reverb **Spring** avec un Mix faible et augmentez Low Cut si la queue brouille le grave.
6. Égalisez le niveau du résultat. Les colorations de guitare de Fire sont des étages de distorsion, pas un simulateur complet de baffle et de microphone.

### Basse : garder le poids, ajouter de l'articulation

1. Divisez la source pour laisser la région des fondamentales graves relativement propre ; choisissez la séparation à l'oreille.
2. Sélectionnez la bande supérieure et essayez **Warm Triode**, **Diode Overdrive** ou **Transformer**.
3. Augmentez Drive jusqu'à ce que les notes soient lisibles sur de petites enceintes, puis baissez le Mix de cette bande si le résultat devient trop abrasif.
4. Utilisez un Ratio modéré dans Compressor si la dynamique des notes aiguës varie trop.
5. Vérifiez le mixage complet sans Solo et en mono. Évitez un Width extrême dans la bande grave.

### Voix : de la densité sans agressivité constante

1. Utilisez une seule bande ou un traitement léger ciblant certaines bandes. Ajoutez **Class A Console** ou Warm Triode avec un Drive faible.
2. Placez un EQ après Shape et créez un Bell à la fréquence agressive repérée à l'écoute.
3. Activez Dynamic pour ce point, sélectionnez un Range négatif et abaissez Threshold jusqu'à ce que seules les phrases plus fortes nécessitent une réduction.
4. Ajoutez une Reverb **Plate** avec assez de Pre-delay pour séparer les mots de la queue, ainsi que Low Cut pour garder la clarté.
5. Comparez à niveau égal et vérifiez que les consonnes restent naturelles.

Pour maîtriser une résonance vocale agressive, placez un Bell à la fréquence gênante, laissez le Gain de base près de 0 dB, réglez un Range négatif et abaissez Threshold jusqu'à ce que les syllabes fortes déclenchent une réduction. Commencez avec un Range modéré : l'objectif est souvent de contrôler quelques dB ponctuellement plutôt que d'effectuer une forte atténuation permanente.

### Batterie : énergie parallèle

1. Ouvrez Routing pour la chaîne et choisissez **Parallel A/B**.
2. Déplacez ou retirez tous les modules de traitement existants de A pour obtenir un trajet direct, puis placez **OTT**, et éventuellement Shape, sur B.
3. Utilisez un Depth OTT modéré, ajustez Up / Up Thresh pour révéler les coups plus faibles et Down / Down Thresh pour contrôler les crêtes.
4. Commencez avec Blend près de l'extrémité A et ajoutez B jusqu'à ce que le groove gagne en densité sans perdre l'attaque sèche.
5. Essayez Lo-Fi sur B pour la texture, puis comparez la boucle complète à un niveau de sortie similaire.

### Nappes et transitions : un nuage figé

1. Ajoutez Granular et alimentez-le avec un accord tenu, ou importez un extrait adapté avec **Sample**.
2. Activez Freeze, éloignez Density du centre et choisissez une Texture plus douce.
3. Baissez Pitch pour un fond sombre ou montez-le pour une couche plus légère ; ajustez Position pour éviter un clic indésirable ou une portion instable.
4. Ajoutez une modulation lente de Position ou Size, en gardant une plage de modulation maîtrisée.
5. Placez Hall Reverb à la suite, ou sur la même branche parallèle, et utilisez Blend pour introduire progressivement la texture.
6. Enregistrez le préréglage avec l'enregistrement figé. N'exportez son WAV brut que si vous avez besoin du matériau source ; effectuez le rendu dans l'hôte pour conserver l'effet évolutif complet.

### Macro de jeu : agir sur plusieurs destinations

Ajoutez un routage par destination et choisissez la même source Macro pour chacun. Réglez les profondeurs et polarités individuellement : une macro peut augmenter le Drive de distorsion, réduire Output et relever légèrement le Mix d'une réverbération. Commencez à zéro et tournez-la progressivement en écoutant. Renommez-la depuis la page principale lorsque sa fonction est claire. Les quatre macros sont des commandes 0-100% automatisables par l'hôte, avec une courte rampe de transition de valeur. Leurs noms, valeurs et routages sont enregistrés avec les sons, A/B et les projets.

## 21. Réglages, performances et dépannage

Choisissez **Menu  >  Skin  >  Modern / Vintage / Paper / Ink**, ou utilisez les quatre boutons d'apparence de Settings. Le son et l'historique d'annulation ne changent pas. Modern utilise le graphite/l'aluminium ; Vintage, le bois et les commandes physiques ; Paper et Ink, des schémas au trait fin avec des palettes claire et sombre. L'habillage est une préférence locale ; les nouvelles instances utilisent le choix enregistré, tandis que les instances déjà ouvertes conservent le leur. Redimensionnez l'éditeur depuis son coin ; il conserve une disposition 2:1 dans les dimensions prises en charge.

![Settings propose les choix d'apparence et la copie locale des diagnostics. Cette capture provient de l'hôte de test documentaire ; votre DAW et votre configuration audio seront différents.](images/22-settings.png)

**Menu  >  Settings** affiche la version, les informations de révision/compilation, le format, l'architecture, l'hôte, la fréquence d'échantillonnage, la taille de bloc, les canaux, la latence et l'état HQ. **Copy diagnostics** actualise et copie ces données localement pour les coller dans un rapport. Rien n'est envoyé automatiquement. Settings propose aussi **Auto-check for updates on startup** ; le menu propose **Check for New Version**.

| Symptôme | Vérification |
| --- | --- |
| Fire n'apparaît pas dans l'hôte | Installez un format compatible avec l'hôte, redémarrez/relancez l'analyse et vérifiez sa liste de validation ou de plug-ins désactivés. Sous macOS, AU et VST3 sont des paquets distincts. |
| Une ancienne version apparaît encore | Redémarrez l'hôte après le remplacement. Vérifiez la version/révision chargée dans Settings et le format/chemin utilisé par l'hôte ; plusieurs installations peuvent coexister. |
| Aucun son | Vérifiez que l'audio atteint la piste, que l'instrument précède Fire et que le routage/la sortie de l'hôte est actif. Vérifiez solo/activation des bandes, Output/Mix, ainsi que toute modulation extrême ou configuration granulaire figée. |
| La vue Side est vide | Un signal mono ou identique à gauche et à droite ne possède pas de contenu Side. Sélectionnez Stereo pour examiner l'énergie combinée. |
| Un graphique s'arrête ou semble vide | Vérifiez que les appels de traitement audio continuent, que la vue concernée est visible et que le spectre est réaffiché. Les affichages masqués ou fermés suspendent volontairement leur capture ; le traitement audio continue. |
| Un routage n'a aucun effet | Vérifiez son interrupteur d'activation, la présence de la source, Depth, la disponibilité de la destination et une éventuelle macro Depth via à zéro. Un ENV externe nécessite un véritable signal sidechain. |
| MIDI Learn ne réagit pas | Vérifiez le routage MIDI de l'hôte, la prise en charge du format, le canal et le type de message réellement émis. Learn attend un MIDI CC. |
| Modifier la courbe du LFO n'a aucun effet | Vérifiez le mode Wave dans Playback. Random Hold/Glide utilisent leur générateur aléatoire, pas la courbe enregistrée ni Smooth. |
| Le collage est refusé | La destination doit disposer d'emplacements libres compatibles et préserver la représentation de la source ; lisez la raison indiquée. Essayez une copie des seuls réglages si cela convient. |
| Match affiche Play audio ou LIMIT | Fournissez un passage représentatif avec une entrée/sortie exploitable ; ±18 dB est sa correction maximale. |
| La charge CPU augmente | Comparez le même passage avec HQ désactivé, réduisez les instances d'effets lourds inutilisées et fermez/repliez les vues d'analyse. Vérifiez que la taille de tampon de l'hôte convient à la session. |

HQ applique un suréchantillonnage 4x au traitement Shape, y compris aux modules Shape ajoutés. Son coût dépend du son et du nombre de moteurs actifs ; Tape Saturation et les autres modèles non linéaires peuvent coûter davantage que les formes numériques simples. Aucun pourcentage CPU unique n'est pertinent pour tous les ordinateurs, fréquences d'échantillonnage, tailles de tampon et préréglages. Comparez dans les mêmes conditions et conservez HQ là où le résultat audible le justifie. Replier l'analyse réduit le travail visuel ; cela ne désactive pas les effets audio et ne fige pas les queues de delay/reverb.

Pour un rapport reproductible, joignez les diagnostics, le préréglage ou une chaîne minimale, la séquence d'actions, le résultat attendu et ce qui s'est réellement produit. Précisez si le problème apparaît dans une instance nouvellement créée et après la réouverture du projet.

### Un même son, des habillages différents

![Paper utilise un fond clair et des commandes au trait fin. Le traitement et le son enregistré restent inchangés.](images/02-overview-paper.png)

![Modern utilise une interface numérique sombre. Vintage et Ink sont illustrés ailleurs dans ce manuel.](images/03-overview-modern.png)

## 22. Référence rapide et assistance

### Référence clavier et pointeur

Ces raccourcis concernent l'interface de Fire lorsqu'elle a le focus. Votre DAW ou un champ de texte actif peut traiter la même touche différemment.

| Action | Geste |
| --- | --- |
| Annuler | Cmd/Ctrl-Z, ou Menu > Undo |
| Rétablir | Cmd/Ctrl-Shift-Z ; Ctrl-Y lorsque la plateforme le prend en charge ; ou Menu > Redo |
| Écouter les préréglages voisins | Up/Down lorsque le champ de recherche de la bibliothèque n'a pas le focus |
| Quitter la bibliothèque | Escape, Back to Reactor ou double-clic sur un préréglage |
| Renommer une macro | Double-clic sur son nom ; Enter valide, Escape annule |
| Réinitialiser la valeur d'une macro | Double-clic sur le potentiomètre de macro |
| Ajouter/supprimer un point de LFO | Double-clic dans un espace vide/sur un point compatible en mode Edit |
| Aligner un point de LFO | Maintenir Cmd sur macOS ou Ctrl sur Windows pendant le glissement |
| Sélectionner plusieurs points de LFO | Shift-glisser pour tracer un rectangle de sélection |
| Ouvrir les options locales | Clic droit sur le potentiomètre, la ligne de module ou l'éditeur de LFO concerné |
| Annuler le déplacement d'un module | Escape ou dépôt en dehors de la chaîne |
| Diviser la bande sélectionnée | Return ou + lorsque le fond du spectre a le focus |
| Supprimer la bande sélectionnée | Delete/Backspace lorsque le fond du spectre a le focus |
| Écouter le détecteur | Maintenir Listen ; Space/Return lorsque la commande Listen a le focus |

### Termes utilisés dans ce manuel

| Terme | Signification dans Fire |
| --- | --- |
| Bande | Une région de fréquences et sa chaîne de traitement. |
| Master | La chaîne située après la recombinaison des signaux des bandes. |
| Module / insert | Un processeur dans une chaîne ; les instances ajoutées sont indépendantes. |
| Valeur de base | Valeur du potentiomètre avant l'ajout de la modulation interne. L'automation de l'hôte peut la déplacer. |
| Source de modulation | Un LFO, un détecteur d'enveloppe, une macro ou un signal MIDI CC. |
| Depth | Intensité et signe de la contribution d'une source à une destination. |
| Plage normalisée | Position interne de 0 à 1 d'une commande. Elle n'est pas nécessairement linéaire en Hz, ms ou dB. |
| Mémoires sonores A/B | Deux sons complets de comparaison sélectionnés dans l'en-tête. |
| Branches parallèles A/B | Deux trajets simultanés au sein d'une chaîne Band ou Master. |
| Mid / Side | Composantes centrale `(L+R)/2` et de différence `(L-R)/2` de l'audio stéréo. |
| Freeze | Maintien d'un enregistrement Granular pendant que ses grains continuent à jouer. |
| WAV figé brut | L'enregistrement stocké, sans la performance granulaire ni les effets en aval. |
| HQ | Trajet Shape à fréquence d'échantillonnage supérieure ; n'augmente pas la capacité d'enregistrement Granular. |
| Compensation de latence | Alignement par l'hôte du retard du plug-in avec les autres trajets. |

### Données de référence

| Élément | Édition actuelle |
| --- | --- |
| Bandes de fréquences | Jusqu'à quatre |
| Instances de modules supplémentaires | Jusqu'à huit par chaîne Band/Master, en plus des modules d'origine |
| Scènes d'usine | 236 dans onze collections, dont 36 scènes Analog Drive |
| Sources de modulation | Jusqu'à 16 LFO, quatre enveloppes, quatre macros et quatre sources MIDI CC |
| Longueur du nom d'une macro | Jusqu'à 32 caractères |
| Dimensions de l'éditeur | De 1000 x 500 à 2000 x 1000, rapport 2:1 |
| Trajet Shape HQ | Suréchantillonnage 4x |
| Correction de niveau perçu Match | Jusqu'à +/-18 dB ; valeurs mesurées distinctes pour les mémoires sonores A et B |
| Capacité d'enregistrement figé | 32 696 trames stéréo ; 1.02175 s à 32 kHz |
| Fréquences d'échantillonnage importées | 8-384 kHz, mono ou stéréo |
| Audio figé exporté | WAV PCM stéréo, 16 bits, 32 kHz |
| Annulation | Jusqu'à 100 modifications dans un budget de 64 Mio de captures d'état |

<!-- pagebreak -->

### À propos de cette édition et de l'assistance

Ce guide décrit le fonctionnement de l'implémentation 2.1.0 en développement ; il ne promet pas qu'une version antérieure possède les mêmes fonctions. Les nouveaux préréglages peuvent ne pas se charger dans d'anciennes versions de Fire. Conservez des copies des préréglages et projets originaux importants avant de migrer une session de production vers une autre version.

Les modules natifs/d'origine et les modules ajoutés indépendamment ont parfois des plages ou des comportements de compatibilité différents. Ce guide signale ces différences lorsqu'elles influencent l'utilisation. Les colorations analogiques sont des modèles et des processeurs créatifs, sans garantie de reproduire exactement un matériel nommé. Fire n'inclut pas de simulateur de baffle et de microphone de guitare.

Les captures ont été produites par les véritables composants du produit, avec de l'audio de démonstration généré et une bibliothèque de préréglages isolée. Elles ne contiennent aucune session ni interprétation enregistrée d'un utilisateur. La révision du code du produit est **9ea20d2**, incluant la correction d'affichage de l'OTT Master/inséré.

- [Page du produit Fire](https://www.bluewingsmusic.com/fire.html)
- [Versions officielles et notes de version](https://github.com/jerryuhoo/Fire/releases)
- [Suivi des problèmes](https://github.com/jerryuhoo/Fire/issues)

Lorsque vous signalez un problème, indiquez la version et le format chargés, les diagnostics copiés, des étapes reproductibles et, si possible, un préréglage ou une chaîne minimale. Décrivez le résultat attendu et le résultat obtenu. Les diagnostics sont copiés localement ; transmettez-les uniquement par le canal d'assistance de votre choix.
