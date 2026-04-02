#!/usr/bin/env python3
"""Download Copernicus DEM GeoTIFF tiles from AWS S3 (no auth required).

Usage:
    python3 download_terrain.py --lat 52 --lon 21             # single tile
    python3 download_terrain.py --bbox 51,20,53,22            # bounding box
    python3 download_terrain.py --lat 52 --lon 21 --glo90     # 90m resolution
    python3 download_terrain.py --lat 52 --lon 21 -o ./maps   # custom output dir
"""

import argparse
import math
import sys
import urllib.request
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_OUT = SCRIPT_DIR.parent / "assets" / "terrain"

BUCKETS = {
    30: ("copernicus-dem-30m", "COG_10", "10"),
    90: ("copernicus-dem-90m", "COG_30", "30"),
}


def tile_name(lat: int, lon: int, res: int) -> str:
    """Build the Copernicus DEM tile name for the SW corner (lat, lon)."""
    _, cog, _ = BUCKETS[res]
    ns = "N" if lat >= 0 else "S"
    ew = "E" if lon >= 0 else "W"
    return f"Copernicus_DSM_{cog}_{ns}{abs(lat):02d}_00_{ew}{abs(lon):03d}_00_DEM"


def tile_url(lat: int, lon: int, res: int) -> str:
    bucket, _, _ = BUCKETS[res]
    name = tile_name(lat, lon, res)
    return f"https://{bucket}.s3.amazonaws.com/{name}/{name}.tif"


def tiles_for_bbox(lat_min: float, lon_min: float, lat_max: float, lon_max: float):
    """Yield (lat, lon) SW corners for all 1-degree tiles covering the bbox."""
    for lat in range(math.floor(lat_min), math.ceil(lat_max)):
        for lon in range(math.floor(lon_min), math.ceil(lon_max)):
            yield lat, lon


def download(url: str, dest: Path) -> bool:
    if dest.exists():
        print(f"  exists: {dest}")
        return True
    print(f"  downloading: {url}")
    try:
        def progress(count, block, total):
            mb = count * block / 1_048_576
            total_mb = total / 1_048_576 if total > 0 else 0
            if total_mb > 0:
                pct = min(100, count * block * 100 / total)
                print(f"\r  {mb:.1f}/{total_mb:.1f} MB ({pct:.0f}%)", end="", flush=True)
            else:
                print(f"\r  {mb:.1f} MB", end="", flush=True)

        urllib.request.urlretrieve(url, str(dest), reporthook=progress)
        print(f"\n  saved: {dest}")
        return True
    except urllib.error.HTTPError as e:
        print(f"\n  error {e.code}: {url}")
        return False
    except Exception as e:
        print(f"\n  error: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(description="Download Copernicus DEM GeoTIFF tiles")
    parser.add_argument("--lat", type=float, help="Latitude (downloads tile containing this point)")
    parser.add_argument("--lon", type=float, help="Longitude (downloads tile containing this point)")
    parser.add_argument("--bbox", type=str,
                        help="Bounding box: lat_min,lon_min,lat_max,lon_max (e.g. 51,20,53,22)")
    parser.add_argument("--glo90", action="store_true", help="Use GLO-90 (90m) instead of GLO-30 (30m)")
    parser.add_argument("-o", "--output", type=str, default=str(DEFAULT_OUT), help="Output directory")
    args = parser.parse_args()

    if args.lat is None and not args.bbox:
        parser.error("Specify --lat/--lon or --bbox")
    if args.lat is not None and args.lon is None:
        parser.error("--lon required with --lat")

    res = 90 if args.glo90 else 30
    out = Path(args.output)
    out.mkdir(parents=True, exist_ok=True)

    tiles = []
    if args.bbox:
        parts = [float(x) for x in args.bbox.split(",")]
        if len(parts) != 4:
            parser.error("--bbox needs 4 values: lat_min,lon_min,lat_max,lon_max")
        tiles = list(tiles_for_bbox(*parts))
    else:
        tiles = [(math.floor(args.lat), math.floor(args.lon))]

    res_str = "GLO-90" if res == 90 else "GLO-30"
    print(f"Copernicus DEM {res_str} — {len(tiles)} tile(s)\n")

    ok = 0
    for lat, lon in tiles:
        name = tile_name(lat, lon, res)
        url = tile_url(lat, lon, res)
        dest = out / f"{name}.tif"
        print(f"[{lat:+03d},{lon:+04d}] {name}")
        if download(url, dest):
            ok += 1

    print(f"\nDone: {ok}/{len(tiles)} tiles downloaded to {out}")
    return 0 if ok == len(tiles) else 1


if __name__ == "__main__":
    sys.exit(main())
