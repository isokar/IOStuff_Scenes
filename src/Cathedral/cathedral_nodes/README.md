# Cathedral

Scène de référence `iostuff.cathedral` 0.1.0, validée sur hardware avec IOStuff Core 0.3.0 et IOStuff Nodes.

Elle dépend de la bibliothèque IOStuff_Nodes. Le profil matériel est lu dans la configuration système. Sur une carte vierge, le premier provisioning choisit `iostuff.board.rgb`.

Le requirement du manifeste est `LedStrip`, count 1. Les actions génériques sont `scene` (`select`) et `launch` (`toggle`). Les identifiants d'options `rise`, `silhouette`, `temperatures`, `vitraux` et `breath` ne correspondent aux indices 0–4 que dans `scene.cpp`.

`/` est la page de la scène. Les liens `/scene/0` … `/scene/4`, `/Launch/on` et `/Launch/off` restent pour cette page. Core utilise `GET /iostuff/v2/scene` et `POST /iostuff/v2/actions/<id>`.

`/system` appartient à Nodes. Cette page ne pilote pas les scénarios.

Compilation N8R8, sans flasher ici :

```text
arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=8M,FlashMode=qio,PSRAM=opi,PartitionScheme=default_8MB ^
  --library <chemin IOStuff_Nodes> ^
  src/Cathedral/cathedral_nodes
```

Le manifeste est `scene_manifest.json`.
