#!/bin/bash
# ═══════════════════════════════════════════════════════════
# MixLauncher v2.0 — Offline Bundle Hazırlama Scripti
# Bu script, Fabric 1.21.4 ön sürümü için gerekli tüm
# dosyaları 'offline_bundle' klasörüne toplar.
# ═══════════════════════════════════════════════════════════

set -e

BUNDLE_DIR="offline_bundle"
MC_DIR="$HOME/.mixcrafter"
GAME_VER="1.21.4"

echo "╔══════════════════════════════════════════════════╗"
echo "║  MixLauncher Offline Bundle Hazırlayıcı         ║"
echo "║  Fabric $GAME_VER Ön Sürüm                      ║"
echo "╚══════════════════════════════════════════════════╝"
echo ""

# Temizle ve oluştur
rm -rf "$BUNDLE_DIR"
mkdir -p "$BUNDLE_DIR/versions/$GAME_VER"
mkdir -p "$BUNDLE_DIR/libraries"
mkdir -p "$BUNDLE_DIR/assets/indexes"
mkdir -p "$BUNDLE_DIR/assets/objects"

# ═══ 1. Vanilla 1.21.4 Dosyaları ═══
echo "[1/4] Vanilla $GAME_VER dosyaları kopyalanıyor..."
if [ -d "$MC_DIR/versions/$GAME_VER" ]; then
    cp -r "$MC_DIR/versions/$GAME_VER/"* "$BUNDLE_DIR/versions/$GAME_VER/"
    echo "  ✓ Version JSON ve JAR kopyalandı"
else
    echo "  ✗ HATA: $MC_DIR/versions/$GAME_VER bulunamadı!"
    echo "    Önce launcher'dan $GAME_VER sürümünü indirin."
    exit 1
fi

# ═══ 2. Fabric Loader Dosyaları ═══
echo "[2/4] Fabric Loader dosyaları kopyalanıyor..."
FABRIC_DIR=""
for dir in "$MC_DIR/versions"/fabric-loader-*-"$GAME_VER"; do
    if [ -d "$dir" ]; then
        FABRIC_DIR="$dir"
        break
    fi
done

if [ -n "$FABRIC_DIR" ]; then
    FABRIC_NAME=$(basename "$FABRIC_DIR")
    mkdir -p "$BUNDLE_DIR/versions/$FABRIC_NAME"
    cp -r "$FABRIC_DIR/"* "$BUNDLE_DIR/versions/$FABRIC_NAME/"
    echo "  ✓ Fabric Loader kopyalandı: $FABRIC_NAME"
else
    echo "  ✗ HATA: Fabric loader bulunamadı!"
    echo "    Önce launcher'dan Fabric ile $GAME_VER profili oluşturun."
    exit 1
fi

# ═══ 3. Kütüphaneler ═══
echo "[3/4] Kütüphaneler kopyalanıyor..."
if [ -d "$MC_DIR/libraries" ]; then
    cp -r "$MC_DIR/libraries/"* "$BUNDLE_DIR/libraries/"
    LIB_COUNT=$(find "$BUNDLE_DIR/libraries" -name "*.jar" | wc -l)
    echo "  ✓ $LIB_COUNT kütüphane kopyalandı"
else
    echo "  ✗ UYARI: libraries klasörü bulunamadı"
fi

# ═══ 4. Asset'ler ═══
echo "[4/4] Asset dosyaları kopyalanıyor..."
if [ -d "$MC_DIR/assets" ]; then
    cp -r "$MC_DIR/assets/"* "$BUNDLE_DIR/assets/"
    ASSET_COUNT=$(find "$BUNDLE_DIR/assets" -type f | wc -l)
    echo "  ✓ $ASSET_COUNT asset dosyası kopyalandı"
else
    echo "  ✗ UYARI: assets klasörü bulunamadı"
fi

# ═══ 5. Profil JSON ═══
echo ""
echo "Profil yapılandırması oluşturuluyor..."
cat > "$BUNDLE_DIR/profiles.json" << 'PROFILJSON'
[
  {
    "name": "Fabric 1.21.4",
    "gameVersion": "1.21.4",
    "loader": "fabric",
    "modsPath": ""
  }
]
PROFILJSON
echo "  ✓ profiles.json oluşturuldu"

# ═══ Özet ═══
TOTAL_SIZE=$(du -sh "$BUNDLE_DIR" | cut -f1)
echo ""
echo "╔══════════════════════════════════════════════════╗"
echo "║  Offline Bundle Hazır!                           ║"
echo "║  Klasör: $BUNDLE_DIR                             ║"
echo "║  Toplam Boyut: $TOTAL_SIZE                       ║"
echo "╠══════════════════════════════════════════════════╣"
echo "║  Sonraki Adımlar:                                ║"
echo "║  1. Windows'ta Inno Setup Compiler kurun         ║"
echo "║  2. MixLauncher_Offline_Setup.iss dosyasını açın ║"
echo "║  3. Compile butonuna basın                       ║"
echo "║  4. installer_output/ klasöründe .exe oluşur     ║"
echo "╚══════════════════════════════════════════════════╝"
