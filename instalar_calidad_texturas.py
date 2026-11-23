#!/usr/bin/env python3
"""
Instala el deslizador "Calidad de texturas" en tu copia de dsr-native.

Copia este archivo en la MISMA carpeta que:
    dsr-native-texture-quality.zip
    texture_quality_slider.patch
y ejecútalo:
    python instalar_calidad_texturas.py            (te pregunta la carpeta)
    python instalar_calidad_texturas.py RUTA       (carpeta del repositorio)

Qué hace según la carpeta de destino:
  * Si es tu clon de GitHub (tiene .git): aplica el parche encima, sin pisar
    el resto de tus archivos. Si el parche no encaja, ofrece copiar solo los
    archivos modificados desde el zip.
  * Si no existe o está vacía: descomprime el proyecto completo ahí, con la
    estructura correcta (sin la carpeta "dsr-native/" de más).
Después puede crear una rama, hacer commit y subirla a GitHub, que es lo que
dispara la compilación del APK (pestaña Actions).

Solo usa la biblioteca estándar de Python 3.6+ (y git, si quieres subirlo).
"""
import argparse
import os
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

AQUI = Path(__file__).resolve().parent
ZIP_NOMBRE = "dsr-native-texture-quality.zip"
PARCHE_NOMBRE = "texture_quality_slider.patch"
RAMA = "texture-quality"
MENSAJE_COMMIT = "Add texture quality slider (25-100%)"
# Archivo que identifica un clon de este proyecto.
MARCA_PROYECTO = Path("android") / "app" / "build.gradle"


def salir(msg, codigo=1):
    print("\nERROR: " + msg)
    sys.exit(codigo)


def preguntar(pregunta, por_defecto=False, auto=False):
    """Sí/no. Con --yes responde siempre el valor por defecto 'sí'."""
    if auto:
        return True
    sufijo = " [S/n] " if por_defecto else " [s/N] "
    try:
        r = input(pregunta + sufijo).strip().lower()
    except EOFError:
        return por_defecto
    if not r:
        return por_defecto
    return r in ("s", "si", "sí", "y", "yes")


def git(args, cwd, **kw):
    return subprocess.run(["git"] + args, cwd=str(cwd), stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, universal_newlines=True, **kw)


def hay_git():
    try:
        return subprocess.run(["git", "--version"], stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE).returncode == 0
    except OSError:
        return False


def buscar_archivo(nombre, patron):
    p = AQUI / nombre
    if p.is_file():
        return p
    candidatos = sorted(AQUI.glob(patron))
    return candidatos[0] if candidatos else None


def archivos_del_parche(parche):
    """Rutas (relativas al proyecto) que modifica el parche: líneas '+++ b/ruta'."""
    rutas = []
    for linea in parche.read_text(encoding="utf-8", errors="replace").splitlines():
        if linea.startswith("+++ "):
            ruta = linea[4:].split("\t")[0].strip()
            if ruta.startswith("b/"):
                ruta = ruta[2:]
            if ruta and ruta != "/dev/null":
                rutas.append(ruta)
    return rutas


def raiz_del_zip(zf):
    """Carpeta única de primer nivel dentro del zip ('dsr-native/'), o ''."""
    primeras = {n.split("/")[0] for n in zf.namelist() if n.strip("/")}
    if len(primeras) == 1:
        unica = primeras.pop()
        if any(n.startswith(unica + "/") for n in zf.namelist()):
            return unica + "/"
    return ""


def extraer(zf, destino, solo=None):
    """Extrae quitando la carpeta raíz. 'solo' = conjunto de rutas a extraer."""
    raiz = raiz_del_zip(zf)
    destino = destino.resolve()
    n = 0
    for info in zf.infolist():
        nombre = info.filename[len(raiz):] if raiz and info.filename.startswith(raiz) else info.filename
        if not nombre or nombre.endswith("/"):
            continue
        if nombre == ".git" or nombre.startswith(".git/"):
            continue
        if solo is not None and nombre not in solo:
            continue
        ruta = (destino / nombre).resolve()
        if destino != ruta and destino not in ruta.parents:  # protección ../
            continue
        ruta.parent.mkdir(parents=True, exist_ok=True)
        with zf.open(info) as src, open(str(ruta), "wb") as dst:
            shutil.copyfileobj(src, dst)
        modo = info.external_attr >> 16
        if modo & 0o111:
            os.chmod(str(ruta), 0o755)
        n += 1
    return n


def subir_a_github(repo, archivos, auto):
    if not preguntar("\n¿Crear la rama '%s', hacer commit y subirla a GitHub?" % RAMA, True, auto):
        return False
    r = git(["checkout", "-b", RAMA], repo)
    if r.returncode != 0:
        r = git(["checkout", RAMA], repo)  # ya existía
        if r.returncode != 0:
            print("No pude cambiar de rama:\n" + r.stderr)
            return False
    r = git(["add", "--"] + archivos, repo)
    if r.returncode != 0:
        print("git add falló:\n" + r.stderr)
        return False
    r = git(["commit", "-m", MENSAJE_COMMIT], repo)
    if r.returncode != 0:
        salida = (r.stdout + r.stderr).strip()
        if "nothing to commit" in salida or "nada para hacer commit" in salida:
            print("No hay cambios nuevos que guardar (¿ya estaban subidos?).")
        else:
            print("git commit falló:\n" + salida)
            if "user.email" in salida or "user.name" in salida or "identity" in salida.lower():
                print('Configura tu identidad una vez:\n  git config --global user.name "Tu nombre"\n'
                      '  git config --global user.email "tu@correo"\nY vuelve a ejecutar el script.')
            return False
    r = git(["push", "-u", "origin", RAMA], repo)
    if r.returncode != 0:
        print("git push falló (¿sesión de GitHub iniciada / permisos?):\n" + r.stderr)
        print("Puedes subirlo tú después con:  git push -u origin " + RAMA)
        return False
    print("Subido a GitHub, rama '%s'." % RAMA)
    return True


def pasos_finales(subido):
    print("\n" + "=" * 60)
    if subido:
        print("Siguiente paso: en GitHub abre la pestaña Actions.")
        print("El workflow 'Build' ya está compilando. Al acabar, baja a")
        print("'Artifacts' y descarga dsr-native-apk-... (contiene el .apk).")
    else:
        print("Siguiente paso: sube el proyecto a tu repositorio de GitHub")
        print("(git push, o subiendo la carpeta desde la web). El workflow")
        print("'Build' de la pestaña Actions generará el APK en 'Artifacts'.")
        print("Para compilar en tu PC: cd android && gradle assembleDebug")
    print("Recuerda: el APK no incluye los archivos del juego.")
    print("=" * 60)


def main():
    ap = argparse.ArgumentParser(description="Instala el deslizador de calidad de texturas en dsr-native.")
    ap.add_argument("repo", nargs="?", help="carpeta de tu clon de dsr-native (o donde crearlo)")
    ap.add_argument("--yes", "-y", action="store_true", help="no preguntar; aceptar las opciones por defecto")
    args = ap.parse_args()

    zip_path = buscar_archivo(ZIP_NOMBRE, "*texture-quality*.zip")
    parche = buscar_archivo(PARCHE_NOMBRE, "*texture_quality*.patch")
    if not zip_path:
        salir("No encuentro %s junto a este script (%s)." % (ZIP_NOMBRE, AQUI))
    if not parche:
        print("Aviso: no encuentro %s; solo podré descomprimir el zip." % PARCHE_NOMBRE)

    # --- carpeta de destino ---
    if args.repo:
        destino = Path(args.repo).expanduser()
    elif (AQUI / ".git").exists() and (AQUI / MARCA_PROYECTO).exists():
        destino = AQUI
    else:
        por_defecto = AQUI / "dsr-native"
        if args.yes:
            destino = por_defecto
        else:
            r = input("Carpeta de tu clon de dsr-native\n(Enter = %s): " % por_defecto).strip().strip('"')
            destino = Path(r).expanduser() if r else por_defecto
    destino = destino.resolve()
    print("\nDestino: %s" % destino)

    es_repo = (destino / ".git").exists()
    vacio = (not destino.exists()) or (destino.is_dir() and not any(destino.iterdir()))

    # --- caso 1: clon de git existente -> parche ---
    if es_repo:
        if not (destino / MARCA_PROYECTO).exists():
            if not preguntar("Esa carpeta tiene git pero no parece dsr-native. ¿Continuar igualmente?", False, False):
                salir("Cancelado.", 0)
        if not hay_git():
            salir("Hace falta git instalado para aplicar el parche (https://git-scm.com).")
        if not parche:
            salir("Falta el parche; sin él no puedo aplicarlo sobre tu repositorio.")
        archivos = archivos_del_parche(parche)

        if git(["apply", "--check", "-R", str(parche)], destino).returncode == 0:
            print("El parche ya está aplicado en esta carpeta. Nada que hacer.")
            subido = subir_a_github(destino, archivos, args.yes) if archivos else False
            pasos_finales(subido)
            return
        r = git(["apply", "--check", str(parche)], destino)
        aplicado = False
        if r.returncode == 0:
            r = git(["apply", str(parche)], destino)
            aplicado = r.returncode == 0
        else:  # CRLF de Windows y similares
            r2 = git(["apply", "--check", "--ignore-whitespace", str(parche)], destino)
            if r2.returncode == 0:
                aplicado = git(["apply", "--ignore-whitespace", str(parche)], destino).returncode == 0
        if aplicado:
            print("Parche aplicado: %d archivos modificados." % len(archivos))
            for a in archivos:
                print("  - " + a)
        else:
            print("El parche no encaja limpiamente (tu repositorio es distinto del que usé).")
            print("Detalle:\n" + r.stderr.strip()[:600])
            print("\nAlternativa: copiar desde el zip esos %d archivos, SOBRESCRIBIENDO los tuyos." % len(archivos))
            print("(Si tienes cambios propios en ellos, git los puede recuperar: git checkout -- <archivo>)")
            if not preguntar("¿Copiarlos ahora?", False, False):
                salir("Cancelado; no se modificó nada.", 0)
            with zipfile.ZipFile(str(zip_path)) as zf:
                n = extraer(zf, destino, solo=set(archivos))
            print("Copiados %d archivos desde el zip." % n)
        subido = subir_a_github(destino, archivos, args.yes)
        pasos_finales(subido)
        return

    # --- caso 2: carpeta nueva/vacía -> proyecto completo ---
    if not vacio:
        print("La carpeta existe, no es un repositorio git y no está vacía.")
        if not preguntar("¿Descomprimir el proyecto dentro de todos modos (puede sobrescribir archivos)?", False, False):
            salir("Cancelado; no se modificó nada.", 0)
    destino.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(str(zip_path)) as zf:
        n = extraer(zf, destino)
    print("Descomprimidos %d archivos en %s" % (n, destino))
    print("Estructura: android/, src/, docs/, tests/, third_party/, CMakeLists.txt, .github/ ...")

    subido = False
    if hay_git() and preguntar("\n¿Inicializar git aquí y conectarlo a tu repositorio de GitHub?", False, False):
        url = input("URL de tu repo (ej. https://github.com/TU_USUARIO/dsr-native.git): ").strip()
        if url:
            git(["init"], destino)
            git(["remote", "add", "origin", url], destino)
            git(["checkout", "-b", RAMA], destino)
            git(["add", "-A"], destino)
            r = git(["commit", "-m", MENSAJE_COMMIT], destino)
            if r.returncode != 0:
                print("git commit falló:\n" + (r.stdout + r.stderr).strip())
            else:
                r = git(["push", "-u", "origin", RAMA], destino)
                if r.returncode == 0:
                    subido = True
                    print("Subido a GitHub, rama '%s'." % RAMA)
                else:
                    print("git push falló:\n" + r.stderr)
                    print("Si tu repo ya tiene historial, es mejor clonarlo y volver a ejecutar este script "
                          "apuntando a esa carpeta (así se aplica solo el parche).")
    pasos_finales(subido)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nCancelado.")
        sys.exit(130)
