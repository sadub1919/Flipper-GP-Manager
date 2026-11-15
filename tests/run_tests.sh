#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
GP_TEST_TMP="$(mktemp -d)"
trap 'rm -rf "$GP_TEST_TMP"' EXIT HUP INT TERM
FLAGS='-std=c11 -Wall -Wextra -Werror -O1'
if [ "${GP_SANITIZE:-0}" = 1 ]; then FLAGS="$FLAGS -g -fsanitize=address,undefined"; fi
export PYTHONDONTWRITEBYTECODE=1
$CC $FLAGS -I. tests/test_engine.c race_engine.c weather.c tyre_model.c -o "$GP_TEST_TMP/engine"
"$GP_TEST_TMP/engine"
$CC $FLAGS -I. tests/test_save.c gp_save.c weekend.c qualifying.c race_engine.c weather.c tyre_model.c race_clock.c -o "$GP_TEST_TMP/save"
"$GP_TEST_TMP/save"
$CC $FLAGS -I. tests/test_weather.c gp_save.c weekend.c qualifying.c race_engine.c weather.c tyre_model.c race_clock.c -o "$GP_TEST_TMP/weather"
"$GP_TEST_TMP/weather"
$CC $FLAGS -I. tests/test_tyres.c gp_save.c weekend.c qualifying.c race_engine.c weather.c tyre_model.c race_clock.c -o "$GP_TEST_TMP/tyres"
"$GP_TEST_TMP/tyres"
$CC $FLAGS -I. tests/test_career.c gp_save.c weekend.c qualifying.c race_engine.c weather.c tyre_model.c race_clock.c -o "$GP_TEST_TMP/career"
"$GP_TEST_TMP/career"
$CC $FLAGS -I. tests/test_championship.c gp_save.c weekend.c qualifying.c race_engine.c weather.c tyre_model.c race_clock.c -o "$GP_TEST_TMP/championship"
"$GP_TEST_TMP/championship"
$CC $FLAGS -I. tests/test_battles.c gp_save.c weekend.c qualifying.c race_engine.c weather.c tyre_model.c race_clock.c -o "$GP_TEST_TMP/battles"
"$GP_TEST_TMP/battles"
$CC $FLAGS -I. tests/test_events.c gp_save.c weekend.c qualifying.c race_engine.c weather.c tyre_model.c race_clock.c -o "$GP_TEST_TMP/events"
"$GP_TEST_TMP/events"
$CC $FLAGS -I. tests/test_clock.c race_clock.c race_engine.c weather.c tyre_model.c -o "$GP_TEST_TMP/clock"
"$GP_TEST_TMP/clock"
$CC $FLAGS -I. tests/test_balance.c race_engine.c weather.c tyre_model.c -o "$GP_TEST_TMP/balance"
"$GP_TEST_TMP/balance"
$CC $FLAGS -I. tests/test_strategy.c race_engine.c weather.c tyre_model.c -o "$GP_TEST_TMP/strategy"
"$GP_TEST_TMP/strategy"
$CC $FLAGS -I. tests/test_qualification.c qualifying.c race_engine.c weather.c tyre_model.c -o "$GP_TEST_TMP/qualification"
"$GP_TEST_TMP/qualification"
$CC $FLAGS -I. tests/test_logos.c team_logos.c -o "$GP_TEST_TMP/logos"
"$GP_TEST_TMP/logos"
$CC $FLAGS -Itests/mocks -I. tests/test_ui.c tests/mock_storage.c gp_save.c weekend.c qualifying.c team_logos.c logo_render.c race_engine.c weather.c tyre_model.c race_clock.c -o "$GP_TEST_TMP/ui"
"$GP_TEST_TMP/ui"
$CC $FLAGS -DGP_RASTER -Itests/mocks -I. tests/test_presentation.c tests/mock_storage.c gp_save.c weekend.c qualifying.c ru_font.c track_shapes.c team_logos.c logo_render.c race_engine.c weather.c tyre_model.c race_clock.c -o "$GP_TEST_TMP/presentation"
"$GP_TEST_TMP/presentation"
$CC $FLAGS -DGP_RASTER -Itests/mocks -I. tests/test_weekend.c tests/mock_storage.c gp_save.c weekend.c qualifying.c ru_font.c track_shapes.c team_logos.c logo_render.c race_engine.c weather.c tyre_model.c race_clock.c -o "$GP_TEST_TMP/weekend"
"$GP_TEST_TMP/weekend"
$CC $FLAGS -DGP_RASTER -Itests/mocks -I. tests/test_markers.c tests/mock_storage.c gp_save.c weekend.c qualifying.c ru_font.c track_shapes.c team_logos.c logo_render.c race_engine.c weather.c tyre_model.c race_clock.c -o "$GP_TEST_TMP/markers"
"$GP_TEST_TMP/markers"
mkdir "$GP_TEST_TMP/screens"
$CC $FLAGS -DGP_RASTER -Itests/mocks -I. tests/test_screenshots.c tests/mock_storage.c gp_save.c weekend.c qualifying.c ru_font.c track_shapes.c team_logos.c logo_render.c race_engine.c weather.c tyre_model.c race_clock.c -o "$GP_TEST_TMP/screenshots"
"$GP_TEST_TMP/screenshots" "$GP_TEST_TMP/screens"
python3 tests/check_screenshots.py "$GP_TEST_TMP/screens"
python3 tests/test_tracks.py
python3 tests/test_manifest.py
python3 tests/test_localization.py
printf '\nALL TESTS PASSED. Host checks complete; device checks are separate.\n'
