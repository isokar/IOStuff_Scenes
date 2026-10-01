# Aquarium

Scène `iostuff.aquarium` 0.1.0. Elle se compile avec IOStuff Nodes. Elle ne contient pas le socle, ni SharedModules.

Le profil matériel se choisit dans `/system`. Cette scène ne teste pas `profileId`. Le profil attendu est `iostuff.board.rgb` : ruban GPIO 16, WS2812 GRB, et le service `rgb-stream` déjà porté par ce profil. Aquarium ne pilote pas de relais.

## Cycle

Le ruban suit le cycle historique : aube, jour, crépuscule, nuit éteinte. Pas de lueur résiduelle. La luminosité du ruban est 220. Le calcul avance toutes les 45 ms. Le fondu de jour se fait par pas de 2 ; l’extinction, par pas de 10, puis un noir franc.

Défauts : mode Auto, météo Clair, fuseau Europe/Paris, lever 08:00, jour plein 09:00, crépuscule 19:00, extinction 20:00. Les durées de montée et de descente restent de 60 minutes quand on déplace le lever ou le coucher.

`clock` à −1 suit NTP (`pool.ntp.org`, `time.google.com`). Une valeur 0–23 fige l’heure à HH:30. Les raccourcis de la page `/phase/day` et `/phase/night` restent à 12:00 et 22:00.

## Pixels

Nodes expose la longueur par `IOStuff.count("strip")`. Le firmware historique peignait 14 pixels. Sur un module vierge, le profil RGB enregistre d’abord 38. Pour retrouver le ruban Aquarium, et pour que les trames IOLS aient la même longueur, `/system` doit enregistrer `pixelCount` 14.

Chaque pixel configuré reçoit la même loi, avec un déphasage de nuage propre à son index. Un ruban plus long prolonge cette loi. Il n’y a pas de seconde zone.

Pendant un flux IOLS, Nodes bloque `setPixel`, `show` et `setBrightness`. Le cycle continue en mémoire et reprend le ruban 1500 ms après la dernière trame valide.

## Persistance

Les réglages vivent dans le namespace Preferences `aquarium` : `lightRun`, `dawnStart`, `dayStart`, `duskStart`, `duskEnd`, `simHour`, `simMin`, `tzIndex`, `weather`.

Au premier démarrage, si ce namespace n’a pas encore `lightRun`, la scène recopie ces clés depuis l’ancien namespace `my-app` lorsqu’elles y sont, puis elle écrit les siennes. Elle ne réécrit pas le Wi-Fi ni le profil.

## Page

`/` affiche l’état, l’horloge, le mode, la météo, le fuseau, le lever, le coucher et l’heure de test. Le réseau historique n’y est plus. Le lien `/system` ouvre la configuration du node.

Core commande `POST /iostuff/v2/actions/<id>` pour `mode`, `weather`, `timezone`, `sunrise`, `sunset` et `clock`.

## Compilation

Même module que le profil RGB déjà validé : ESP32-S3-WROOM-1 N8R8.

```text
esp32:esp32:esp32s3:FlashSize=8M,FlashMode=qio,PSRAM=opi,PartitionScheme=default_8MB
```

Ce dossier ne flashe pas.

Le ping UDP legacy répond avec le type `0x0B` et la version `1.0`. La version annoncée en V2 est `0.1.0`.
