#!/bin/bash
#
# Copyright (C) 2016 The CyanogenMod Project
# Copyright (C) 2017 The LineageOS Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

set -e

DEVICE=mata
VENDOR=essential

INITIAL_COPYRIGHT_YEAR=2017
COPYRIGHT="--copyright=$INITIAL_COPYRIGHT_YEAR"
TREBLE="--treble"

# Load extract_utils and do some sanity checks
MY_DIR="${BASH_SOURCE%/*}"
FILE="$MY_DIR/proprietary-files.txt"

if [[ ! -d "$MY_DIR" ]]; then MY_DIR="$PWD"; fi

LINEAGE_ROOT="$MY_DIR"/../../..

HELPER="$LINEAGE_ROOT"/vendor/lineage/build/tools/extract_utils.py
if [ ! -f "$HELPER" ]; then
    echo "Unable to find helper script at $HELPER"
    exit 1
fi

SRC="$1"

"$HELPER" --regen-only -d "$DEVICE" -v "$VENDOR" -s "$SRC" -r "$LINEAGE_ROOT" -f "$FILE" "$COMMON" "$COPYRIGHT" "$TREBLE"
