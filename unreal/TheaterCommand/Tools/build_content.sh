#!/bin/bash
# Rebuilds the handful of Unreal assets described in build_content.py.
# Close the Unreal editor on this project before running it.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PROJECT="$HERE/../TheaterCommand.uproject"
ENGINE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
"$ENGINE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJECT" -run=pythonscript -script="$HERE/build_content.py" -unattended -nosplash -nullrhi -stdout
