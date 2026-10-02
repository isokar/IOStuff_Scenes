# IOStuff Scenes

Bibliothèque des scènes IOStuff. Une scène se compile avec [IOStuff_Nodes](https://github.com/isokar/IOStuff_Nodes). Ce dépôt ne contient pas le socle, ni un installeur Core.

Quatre choses distinctes :

| Emplacement | Rôle |
| --- | --- |
| `src/` | Sources des scènes. |
| `bin/` | Images d'application OTA officiellement construites et validées sur hardware. |
| GitHub Releases | Distribution publique de ces images. |
| `catalog/firmware.json` | Index consommé par IOStuff Core. Il pointe vers les assets de release, pas vers les fichiers de `bin/` ni vers un dossier de build. |

## Sources

| Scène | Id source | Dossier |
| --- | --- | --- |
| [Cathedral](src/Cathedral/cathedral_nodes) | `iostuff.cathedral` 0.1.0 | `src/Cathedral/cathedral_nodes` |
| [Artemis](src/Artemis/artemis_nodes) | `iostuff.artemis` 0.1.0 | `src/Artemis/artemis_nodes` |
| [Aquarium](src/Aquarium/aquarium_nodes) | `iostuff.aquarium` 0.1.0 | `src/Aquarium/aquarium_nodes` |

La version écrite dans le sketch est la version source. Une image de test peut être compilée avec une macro, sans changer cette valeur. Aquarium 0.1.2 a été produit ainsi ; le sketch reste en 0.1.0.

Emplacement prévu, pas encore versionné : LightRoom.

Compilation N8R8, sans flasher :

```text
esp32:esp32:esp32s3:FlashSize=8M,FlashMode=qio,PSRAM=opi,PartitionScheme=default_8MB
```

Le sketch est `src/<Scene>/<scene>_nodes`.

## De la source à l'index

```text
Source → Build → Validation hardware → bin/ → GitHub Release → catalog
```

1. La source vit dans `src/`.
2. Le build produit un dossier de compilation. Ce dossier n'est pas une release. Il reste hors Git (`build/`, `.elf`, `.map`, `merged.bin`, bootloader, partitions).
3. La validation hardware confirme l'image, le profil NVS et le rollback.
4. Seule l'image d'application OTA validée est copiée dans `bin/<Scene>/<sceneVersion>/`.
5. La distribution publique est une GitHub Release, pas le fichier de `main`.
6. `catalog/firmware.json` enregistre l'URL HTTPS de cet asset, sa taille et son SHA256.

Un firmware entre dans `bin/` seulement après validation. Le nom est :

```text
iostuff.<scene>-<sceneVersion>-nodes-<nodesVersion>.bin
```

Exemple validé :

```text
bin/Aquarium/0.1.2/iostuff.aquarium-0.1.2-nodes-0.1.0.bin
```

La release correspondante, pas encore publiée, serait le tag `aquarium-0.1.2` et l'asset du même nom.
