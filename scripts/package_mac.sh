#!/usr/bin/env bash
set -euo pipefail

# Settings
APP_NAME="MoPanning"
BUILD_DIR="build"
CONFIG="Release"
DIST_DIR="build"
DMG_NAME="${APP_NAME}-macOS.dmg"
VOLNAME="${APP_NAME}"
STAGE_DIR="${DIST_DIR}/${APP_NAME}_DMG"

# Configure & build
cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${CONFIG}" -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build "${BUILD_DIR}" --config "${CONFIG}"

# Locate the .app bundle
CANDIDATES=(
  "${BUILD_DIR}/${APP_NAME}_artefacts/${CONFIG}/${APP_NAME}.app"
  "${BUILD_DIR}/${APP_NAME}_artefacts/${APP_NAME}.app"
  "${BUILD_DIR}/${CONFIG}/${APP_NAME}.app"
)

APP_BUNDLE=""
for c in "${CANDIDATES[@]}"; do
  if [[ -d "$c" ]]; then
    APP_BUNDLE="$c"
    break
  fi
done

if [[ -z "${APP_BUNDLE}" ]]; then
  echo "Could not find ${APP_NAME}.app in expected build locations."
  exit 1
fi

echo "Found app bundle: ${APP_BUNDLE}"

# Codesign the app (ad-hoc)
codesign --force --deep --sign - "${APP_BUNDLE}"

# Copy the app bundle to a new staging folder
rm -rf "${STAGE_DIR}"
mkdir -p "${STAGE_DIR}"
ditto "${APP_BUNDLE}" "${STAGE_DIR}/${APP_NAME}.app"

# Add an Applications link for drag-and-drop install
ln -s /Applications "${STAGE_DIR}/Applications" || true

# Create compressed DMG
DMG_PATH="${DIST_DIR}/${DMG_NAME}"
rm -f "${DMG_PATH}"
hdiutil create -volname "${VOLNAME}" -srcfolder "${STAGE_DIR}" -ov -format UDZO "${DMG_PATH}"

echo "DMG created: ${DMG_PATH}"

# Clean up staging folder
rm -rf "${STAGE_DIR}"