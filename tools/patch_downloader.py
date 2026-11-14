#!/usr/bin/env python3
"""Faz o DownloaderActivity do GTA:SA 2.11.x não depender da Play Store.

Com o APK reassinado a Play recusa entregar os asset packs (erro -15) e o jogo
para no downloader. Este patch altera DownloaderActivityBase.smali para:

  1. hasAssetPack() sempre retornar true  -> nenhum download é pedido;
  2. packs sem localização conhecida receberem um caminho em armazenamento
     externo (<PACK_ROOT>/<nome_do_pack>) em vez de lançar exceção.

Uso: patch_downloader.py <pasta_do_apk_decodificado> [pacote]
"""
import pathlib
import re
import sys

PACK_ROOT = "/storage/emulated/0/Android/data/{package}/files/mtasa_packs/"

HAS_ASSET_PACK = """.method private final hasAssetPack(Ljava/lang/String;)Z
    .locals 0

    # mtasa: nunca pedir download à Play Store
    const/4 p1, 0x1

    return p1
.end method"""

MISSING_PACK = """    :cond_3
    # mtasa: pack sem localização -> caminho externo em vez de exceção
    new-instance p0, Ljava/lang/StringBuilder;

    const-string p1, "{root}"

    invoke-direct {{p0, p1}}, Ljava/lang/StringBuilder;-><init>(Ljava/lang/String;)V

    invoke-virtual {{p4}}, Lcom/rockstargames/oswrapper/DownloaderPackConfig;->getId()Ljava/lang/String;

    move-result-object p1

    invoke-virtual {{p0, p1}}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object p0

    invoke-virtual {{p0}}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object p0

    const-string p1, "MTASA"

    invoke-static {{p1, p0}}, Landroid/util/Log;->w(Ljava/lang/String;Ljava/lang/String;)I

    invoke-interface {{p2, p0}}, Ljava/util/List;->add(Ljava/lang/Object;)Z

    sget-object p0, Lkotlin/Unit;->INSTANCE:Lkotlin/Unit;

    return-object p0
.end method"""


def replace_method(text, signature_regex, transform):
    pattern = re.compile(r"^\.method [^\n]*" + signature_regex + r".*?^\.end method", re.S | re.M)
    match = pattern.search(text)
    if not match:
        return text, False
    return text[: match.start()] + transform(match.group(0)) + text[match.end():], True


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    root = pathlib.Path(sys.argv[1])
    package = sys.argv[2] if len(sys.argv) > 2 else "com.rockstargames.gtasa"

    files = list(root.glob("smali*/com/rockstargames/oswrapper/DownloaderActivityBase.smali"))
    if not files:
        print("patch_downloader: DownloaderActivityBase.smali não existe nesta build; nada a fazer")
        return
    path = files[0]
    text = path.read_text(encoding="utf-8")
    if "# mtasa:" in text:
        print("patch_downloader: já aplicado")
        return

    text, ok1 = replace_method(text, r"hasAssetPack\(Ljava/lang/String;\)Z", lambda _: HAS_ASSET_PACK)

    def patch_missing(method):
        head, sep, _ = method.partition("    :cond_3\n")
        if not sep or "returned null" not in method:
            raise SystemExit("patch_downloader: formato inesperado em onDownloadComplete$lambda$0")
        return head + MISSING_PACK.format(root=PACK_ROOT.format(package=package))

    text, ok2 = replace_method(text, r"onDownloadComplete\$lambda\$0\(", patch_missing)

    if not (ok1 and ok2):
        raise SystemExit(f"patch_downloader: métodos não encontrados (hasAssetPack={ok1}, lambda={ok2})")
    path.write_text(text, encoding="utf-8")
    print(f"downloader patcheado: {path}")
    print(f"packs ausentes serão procurados em {PACK_ROOT.format(package=package)}<nome>")


if __name__ == "__main__":
    main()
