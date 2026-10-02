# Artemis

Scène `iostuff.artemis` 0.1.0. Elle se compile avec IOStuff Nodes. Elle ne contient pas le socle, ni SharedModules.

Le profil matériel se choisit dans `/system`. Cette scène ne teste pas `profileId`. Le profil qui porte le ruban et le relais est `iostuff.board.rgb-relay`.

## Séquence

Au repos, le relais `relay` est LOW et le ruban `strip` est noir.

`launch = true`, ou `GET /Launch/on`, ne démarre que depuis le repos. Le relais passe HIGH tout de suite. Trois secondes sans lumière, puis trente secondes : chaque pixel reçoit une teinte de la palette historique, renouvelée toutes les 300 ms. Le relais reste HIGH.

La fin des trente secondes, `launch = false`, ou `GET /Launch/off`, remet le relais à LOW, le ruban au noir, et la scène au repos.

Palette, dans l'ordre du firmware précédent : `(245,128,39)`, `(232,110,14)`, `(252,120,8)`, `(255,140,100)`, `(247,80,40)`.

## Pixels

Nodes expose la longueur du ruban par `IOStuff.count("strip")`, qui est le `pixelCount` de la Hardware Config.

Le défaut de `iostuff.board.rgb-relay`, quand `pixelCount` n'est pas encore enregistré, est 12. Avec cette valeur, les douze pixels historiques reçoivent chacun une couleur de la palette. Il n'y a pas de fenêtre fixe de 12 pixels ni de placement spatial nouveau.

Si `/system` enregistre une autre longueur, la même règle s'applique à chaque pixel configuré. Ce n'est pas une autre animation : c'est la palette historique sur la longueur demandée par le node. Un node déjà provisionné en profil RGB conserve son ancien `pixelCount` tant qu'on ne le change pas.

Pendant un flux IOLS, Nodes bloque `setPixel`, `show` et `setBrightness`. `output("relay", …)` continue. La machine à états et le relais ne s'arrêtent pas.

## Page

`/` affiche Artemis Launcher : statut, compte à rebours, Lancer ou Abandon. Le réseau historique n'y est plus. Le lien `/system` ouvre la configuration du node.

Core commande `POST /iostuff/v2/actions/launch` avec `true` ou `false`. L'état annoncé est `true` pendant l'allumage et le décollage.

## Compilation

Même module que le profil RGB déjà validé : ESP32-S3-WROOM-1 N8R8. GPIO 16 est le ruban, GPIO 18 le relais de ce module.

```text
esp32:esp32:esp32s3:FlashSize=8M,FlashMode=qio,PSRAM=opi,PartitionScheme=default_8MB
```

Ce dossier ne flashe pas.
